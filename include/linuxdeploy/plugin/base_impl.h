// system headers
#include <set>
#include <string>
#include <vector>
#include <poll.h>

// library headers
#include <boost/filesystem.hpp>
#include <fnmatch.h>
#include <subprocess.hpp>

// local headers
#include "linuxdeploy/core/log.h"

#pragma once

// implementation of PluginBase in a header to solve issues like
// https://bytefreaks.net/programming-2/c/c-undefined-reference-to-templated-class-function
namespace linuxdeploy {
    namespace plugin {
        namespace base {
            using namespace linuxdeploy::core::log;

            template<int API_LEVEL>
            class PluginBase<API_LEVEL>::PrivateData {
                public:
                    const boost::filesystem::path pluginPath;
                    std::string name;
                    int apiLevel;
                    PLUGIN_TYPE pluginType;

                public:
                    explicit PrivateData(const boost::filesystem::path& path) : pluginPath(path) {
                        if (!boost::filesystem::exists(path)) {
                            throw PluginError("No such file or directory: " + path.string());
                        }

                        apiLevel = getApiLevelFromExecutable();
                        pluginType = getPluginTypeFromExecutable();

                        boost::cmatch res;
                        boost::regex_match(path.filename().c_str(), res, PLUGIN_EXPR);
                        name = res[1].str();
                    };

                private:
                    int getApiLevelFromExecutable() {
                        auto output = subprocess::check_output({pluginPath.c_str(), "--plugin-api-version"});
                        std::string stdoutOutput = output.buf.data();

                        try {
                            auto apiLevel = std::stoi(stdoutOutput);
                            return apiLevel;
                        } catch (const std::invalid_argument&) {
                            return -1;
                        }
                    }

                    PLUGIN_TYPE getPluginTypeFromExecutable() {
                        // assume input type
                        auto type = INPUT_TYPE;

                        // check whether plugin implements --plugin-type
                        try {
                            auto output = subprocess::check_output({pluginPath.c_str(), "--plugin-type"});

                            std::string stdoutOutput = output.buf.data();

                            // the specification requires a single line, but we'll silently accept more than that, too
                            if (std::count(stdoutOutput.begin(), stdoutOutput.end(), '\n') >= 1) {
                                auto firstLine = stdoutOutput.substr(0, stdoutOutput.find_first_of('\n'));

                                if (firstLine == "input")
                                    type = INPUT_TYPE;
                                else if (firstLine == "output")
                                    type = OUTPUT_TYPE;
                            }
                        } catch (const subprocess::CalledProcessError&) {}

                        return type;
                    }
            };

            template<int API_LEVEL>
            PluginBase<API_LEVEL>::PluginBase(const boost::filesystem::path& path) : IPlugin(path) {
                d = new PrivateData(path);

                if (d->apiLevel != API_LEVEL) {
                    std::stringstream msg;
                    msg << "This class only supports API level " << API_LEVEL << ", not " << d->apiLevel;
                    throw WrongApiLevelError(msg.str());
                }
            }

            template<int API_LEVEL>
            PluginBase<API_LEVEL>::~PluginBase() {
                delete d;
            }

            template<int API_LEVEL>
            boost::filesystem::path PluginBase<API_LEVEL>::path() const {
                return d->pluginPath;
            }

            template<int API_LEVEL>
            PLUGIN_TYPE PluginBase<API_LEVEL>::pluginType() const {
                return d->pluginType;
            }

            template<int API_LEVEL>
            std::string PluginBase<API_LEVEL>::pluginTypeString() const {
                switch ((int) d->pluginType) {
                    case INPUT_TYPE:
                        return "input";
                    case OUTPUT_TYPE:
                        return "output";
                    default:
                        return "<unknown>";
                }
            }

            template<int API_LEVEL>
            int PluginBase<API_LEVEL>::apiLevel() const {
                return d->apiLevel;
            }

            template<int API_LEVEL>
            int PluginBase<API_LEVEL>::run(const boost::filesystem::path& appDirPath) {
                auto pluginPath = path();
                auto args = {pluginPath.c_str(), "--appdir", appDirPath.string().c_str()};

                auto log = ldLog();
                log << "Running process:";
                for (const auto& arg : args) {
                    log << "" << arg;
                }
                log << std::endl;

                auto process = subprocess::Popen(args, subprocess::output{subprocess::PIPE}, subprocess::error{subprocess::PIPE});


                std::vector<pollfd> pfds(2);
                auto* opfd = &pfds[0];
                auto* epfd = &pfds[1];

                opfd->fd = fileno(process.output());
                opfd->events = POLLIN;

                epfd->fd = fileno(process.error());
                epfd->events = POLLIN;

                for (const auto& fd : {process.output(), process.error()}) {
                    auto flags = fcntl(fileno(fd), F_GETFL, 0);
                    flags |= O_NONBLOCK;
                    fcntl(fileno(fd), F_SETFL, flags);
                }

                auto printOutput = [&pfds, opfd, epfd, this, &process]() {
                    poll(pfds.data(), pfds.size(), -1);

                    auto printUntilLastLine = [this](std::vector<char>& buf, size_t& bufSize, const std::string& streamType) {
                        std::ostringstream oss;

                        while (true) {
                            const auto firstLineFeed = std::find(buf.begin(), buf.end(), '\n');
                            const auto firstCarriageReturn = std::find(buf.begin(), buf.end(), '\r');

                            if (firstLineFeed == buf.end() && firstCarriageReturn == buf.end())
                                break;
                            
                            const auto endOfLine = std::min(firstLineFeed, firstCarriageReturn);

                            std::string line(buf.begin(), endOfLine+1);

                            oss << "[" << d->name << "/" << streamType << "] " << line;

                            bufSize -= std::distance(buf.begin(), endOfLine+1);
                            buf.erase(buf.begin(), endOfLine+1);
                        }

                        auto messages = oss.str();
                        if (!messages.empty())
                            ldLog() << messages;
                    };

                    std::vector<char> stdoutBuf(16 * 1024);
                    std::vector<char> stderrBuf(16 * 1024);
                    size_t stdoutBufSize = 0;
                    size_t stderrBufSize = 0;

                    if (opfd->revents & POLLIN) {
                        if (stdoutBufSize >= stdoutBuf.size())
                            throw std::runtime_error("Buffer overflow");

                        while (true) {
                            auto bytesRead = fread(
                                reinterpret_cast<void*>(stdoutBuf.data() + stdoutBufSize),
                                sizeof(char),
                                static_cast<size_t>(stdoutBuf.size() - stdoutBufSize),
                                process.output()
                            );

                            if (bytesRead == 0)
                                break;

                            stdoutBufSize += bytesRead;

                            printUntilLastLine(stdoutBuf, stdoutBufSize, "stdout");
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    if (epfd->revents & POLLIN) {
                        if (stderrBufSize >= stderrBuf.size())
                            throw std::runtime_error("Buffer overflow");

                        while (true) {
                            auto bytesRead = fread(
                                reinterpret_cast<void*>(stderrBuf.data() + stderrBufSize),
                                sizeof(char),
                                static_cast<size_t>(stderrBuf.size() - stderrBufSize),
                                process.error()
                            );

                            if (bytesRead == 0)
                                break;

                            stderrBufSize += bytesRead;

                            printUntilLastLine(stderrBuf, stderrBufSize, "stderr");
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    return true;
                };

                int retcode;

                do {
                    if (!printOutput()) {
                        ldLog() << LD_ERROR << "Failed to communicate with process" << std::endl;
                        process.kill();
                        return -1;
                    }
                } while ((retcode = process.poll()) < 0);

                if (!printOutput()) {
                    ldLog() << LD_ERROR << "Failed to communicate with process" << std::endl;
                    process.kill();
                    return -1;
                }

                return retcode;
            }
        }
    }
}
