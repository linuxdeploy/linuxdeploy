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

                auto log = linuxdeploy::core::log::ldLog();
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

                auto printOutput = [&pfds, opfd, epfd, this, &process]() {
                    poll(pfds.data(), pfds.size(), -1);

                    if (opfd->revents & POLLIN) {
                        std::ostringstream oss;

                        std::vector<char> buf(4096);
                        auto* lineptr = buf.data();
                        auto n = buf.size();

                        while (getline(&lineptr, &n, process.output()) != -1) {
                            oss << "[" << d->name << "/stdout] " << buf.data();
                        }
                        linuxdeploy::core::log::ldLog() << oss.str();
                    }

                    if (epfd->revents & POLLIN) {
                        std::ostringstream oss;

                        std::vector<char> buf(4096);
                        auto* lineptr = buf.data();
                        auto n = buf.size();

                        while (getline(&lineptr, &n, process.error()) != -1) {
                            oss << "[" << d->name << "/stderr] " << buf.data();
                        }
                        linuxdeploy::core::log::ldLog() << oss.str();
                    }
                };

                do {
                    printOutput();
                } while (process.poll() < 0);

                printOutput();

                return process.retcode();
            }
        }
    }
}
