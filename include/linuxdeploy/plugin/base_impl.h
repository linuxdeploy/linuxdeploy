// system headers
#include <fcntl.h>
#include <filesystem>
#include <poll.h>
#include <set>
#include <string>
#include <vector>

// library headers
#include <fnmatch.h>
#include <thread>

// local headers
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/plugin/plugin_process_handler.h"

#pragma once

// implementation of PluginBase in a header to solve issues like
// https://bytefreaks.net/programming-2/c/c-undefined-reference-to-templated-class-function
namespace linuxdeploy {
    namespace plugin {
        namespace base {
            using namespace linuxdeploy::log;

            template<int API_LEVEL>
            class PluginBase<API_LEVEL>::PrivateData {
                public:
                    const std::filesystem::path pluginPath;
                    std::string name;
                    int apiLevel;
                    PLUGIN_TYPE pluginType;

                public:
                    explicit PrivateData(const std::filesystem::path& path) : pluginPath(path) {
                        if (!std::filesystem::exists(path)) {
                            throw PluginError("No such file or directory: " + path.string());
                        }

                        ldLog() << LD_DEBUG << "Probing plugin" << path.string() << std::endl;

                        apiLevel = getApiLevelFromExecutable();
                        pluginType = getPluginTypeFromExecutable();

                        std::cmatch res;
                        std::regex_match(path.filename().c_str(), res, PLUGIN_EXPR);
                        name = res[1].str();
                    };

                private:
                    subprocess::subprocess_env_map_t getFixedEnvironment() {
                        auto rv = subprocess::get_environment();
                        rv.erase("VERBOSE");
                        return rv;
                    }

                    int getApiLevelFromExecutable() {
                        const auto arg =  "--plugin-api-version";

                        // during plugin detection, we must make sure $VERBOSE is not passed to the AppImage runtime
                        // otherwise, in combination with $APPIMAGE_EXTRACT_AND_RUN, the runtime will spam the file
                        // extraction messages, which makes parsing the output very hard
                        const subprocess::subprocess proc({pluginPath.string(), arg}, getFixedEnvironment());
                        const auto stdoutOutput = proc.check_output();

                        if (stdoutOutput.empty()) {
                            ldLog() << LD_WARNING << "received empty response from plugin" << pluginPath << "while trying to fetch data for" <<  "--plugin-api-version" << std::endl;
                            return -1;
                        }

                        if (getenv("DEBUG_PLUGIN_DETECTION")) {
                            ldLog() << LD_DEBUG << "output from plugin:" << stdoutOutput << std::endl;
                        }

                        try {
                            const int parsedApiLevel = std::stoi(stdoutOutput);
                            return parsedApiLevel;
                        } catch (const std::exception&) {
                            return -1;
                        }
                    }

                    PLUGIN_TYPE getPluginTypeFromExecutable() {
                        // assume input type
                        auto type = INPUT_TYPE;

                        // check whether plugin implements --plugin-type
                        try {
                            // during plugin detection, we must make sure $VERBOSE is not passed to the AppImage runtime
                            // otherwise, in combination with $APPIMAGE_EXTRACT_AND_RUN, the runtime will spam the file
                            // extraction messages, which makes parsing the output very hard
                            const subprocess::subprocess proc({pluginPath.c_str(), "--plugin-type"}, getFixedEnvironment());
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
            PluginBase<API_LEVEL>::PluginBase(const std::filesystem::path& path) : IPlugin(path) {
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
            std::filesystem::path PluginBase<API_LEVEL>::path() const {
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
            int PluginBase<API_LEVEL>::run(const std::filesystem::path& appDirPath) {
                plugin_process_handler handler(d->name, path());
                return handler.run(appDirPath);
            }
        }
    }
}
