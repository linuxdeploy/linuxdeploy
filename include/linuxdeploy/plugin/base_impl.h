// system headers
#include <set>
#include <string>
#include <vector>
#include <fcntl.h>
#include <poll.h>

// library headers
#include <boost/filesystem.hpp>
#include <fnmatch.h>
#include <thread>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/process.h"

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
                        const auto arg =  "--plugin-api-version";

                        const subprocess::subprocess proc({pluginPath.string(), arg});
                        const auto stdoutOutput = proc.check_output();

                        if (stdoutOutput.empty()) {
                            ldLog() << LD_WARNING << "received empty response from plugin" << pluginPath << "while trying to fetch data for" <<  "--plugin-api-version" << std::endl;
                            return -1;
                        }

                        try {
                            auto apiLevel = std::stoi(stdoutOutput);
                            return apiLevel;
                        } catch (const std::exception&) {
                            return -1;
                        }
                    }

                    PLUGIN_TYPE getPluginTypeFromExecutable() {
                        // assume input type
                        auto type = INPUT_TYPE;

                        // check whether plugin implements --plugin-type
                        try {
                            const subprocess::subprocess proc({pluginPath.c_str(), "--plugin-type"});
                            const auto stdoutOutput = proc.check_output();

                            // the specification requires a single line, but we'll silently accept more than that, too
                            if (std::count(stdoutOutput.begin(), stdoutOutput.end(), '\n') >= 1) {
                                auto firstLine = stdoutOutput.substr(0, stdoutOutput.find_first_of('\n'));

                                if (firstLine == "input")
                                    type = INPUT_TYPE;
                                else if (firstLine == "output")
                                    type = OUTPUT_TYPE;
                            }
                        } catch (const std::logic_error&) {}

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
                const auto pluginPath = path();
                const std::initializer_list<std::string> args = {pluginPath.string(), "--appdir", appDirPath.string()};

                auto log = ldLog();
                log << "Running process:";
                for (const auto& arg : args) {
                    log << "" << arg;
                }
                log << std::endl;

                subprocess::subprocess_env_map_t environmentVariables{};

                if (this->pluginType() == PLUGIN_TYPE::INPUT_TYPE) {
                    // add $LINUXDEPLOY, which points to the current binary
                    // we do not need to pass $APPIMAGE or alike, since while linuxdeploy is running, the path in the
                    // temporary mountpoint of its AppImage will be valid anyway
                    environmentVariables["LINUXDEPLOY"] = linuxdeploy::util::getOwnExecutablePath();
                }

                subprocess::process process(args, environmentVariables);

                std::vector<pollfd> pfds(2);
                auto* opfd = &pfds[0];
                auto* epfd = &pfds[1];

                opfd->fd = process.stdout_fd();
                opfd->events = POLLIN;

                epfd->fd = process.stderr_fd();
                epfd->events = POLLIN;

                for (const auto& fd : {process.stdout_fd(), process.stderr_fd()}) {
                    auto flags = fcntl(fd, F_GETFL, 0);
                    flags |= O_NONBLOCK;
                    fcntl(fd, F_SETFL, flags);
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
                    size_t currentStdoutBufSize = 0;
                    size_t currentStderrBufSize = 0;

                    if ((opfd->revents & POLLIN) != 0) {
                        if (currentStdoutBufSize >= stdoutBuf.size())
                            throw std::runtime_error("Buffer overflow");

                        while (true) {
                            auto bytesRead = read(
                                process.stdout_fd(),
                                reinterpret_cast<void*>(stdoutBuf.data() + currentStdoutBufSize),
                                static_cast<size_t>(stdoutBuf.size() - currentStdoutBufSize)
                            );

                            if (bytesRead == 0)
                                break;

                            currentStdoutBufSize += bytesRead;

                            printUntilLastLine(stdoutBuf, currentStdoutBufSize, "stdout");
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    if ((epfd->revents & POLLIN) != 0) {
                        if (currentStderrBufSize >= stderrBuf.size())
                            throw std::runtime_error("Buffer overflow");

                        while (true) {
                            auto bytesRead = read(
                                process.stderr_fd(),
                                reinterpret_cast<void*>(stderrBuf.data() + currentStderrBufSize),
                                static_cast<size_t>(stderrBuf.size() - currentStderrBufSize)
                            );

                            if (bytesRead == 0)
                                break;

                            currentStderrBufSize += bytesRead;

                            printUntilLastLine(stderrBuf, currentStderrBufSize, "stderr");
                        }
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(50));

                    return true;
                };

                do {
                    if (!printOutput()) {
                        ldLog() << LD_ERROR << "Failed to communicate with process" << std::endl;
                        process.kill();
                        return -1;
                    }
                } while (process.is_running());

                if (!printOutput()) {
                    ldLog() << LD_ERROR << "Failed to communicate with process" << std::endl;
                    process.kill();
                    return -1;
                }

                const auto retcode = process.close();
                return retcode;
            }
        }
    }
}
