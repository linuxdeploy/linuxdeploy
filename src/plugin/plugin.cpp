// system headers
#include <filesystem>
#include <regex>
#include <set>
#include <string>

// local headers
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/plugin/plugin.h"
#include "linuxdeploy/util/util.h"
#include "plugin_type0.h"

using namespace linuxdeploy::log;

namespace fs = std::filesystem;

namespace linuxdeploy {
    namespace plugin {
        IPlugin* createPluginInstance(const std::filesystem::path& path) {
            IPlugin* rv = nullptr;

            // test whether it's a type 0 plugin
            try {
                rv = new Type0Plugin(path);
            } catch (const WrongApiLevelError& e) {
                ldLog() << LD_DEBUG << e.what() << std::endl;
            }

            return rv;
        }

        std::map<std::string, IPlugin*> findPlugins() {
            std::map<std::string, IPlugin*> foundPlugins;

            const auto PATH = getenv("PATH");

            auto paths = util::split(PATH, ':');

            auto currentExeDir = fs::path(util::getOwnExecutablePath()).parent_path();
            paths.insert(paths.begin(), currentExeDir.string());

            // if shipping as an AppImage, search for plugins in AppImage's location first
            // this way, plugins in the AppImage's directory take precedence over bundled ones
            if (getenv("APPIMAGE") != nullptr) {
                auto appImageDir = fs::path(getenv("APPIMAGE")).parent_path();
                paths.insert(paths.begin(), appImageDir.string());
            }

            // also, look for plugins in current working directory
            // could be useful in a "use linuxdeploy centrally, but download plugins into project directory" scenario
            std::shared_ptr<char> cwd(get_current_dir_name(), free);
            paths.emplace_back(cwd.get());

            for (const auto& dir : paths) {
                if (!fs::is_directory(dir))
                    continue;

                ldLog() << LD_DEBUG << "Searching for plugins in directory" << dir << std::endl;

                bool extendedDebugLoggingEnabled = (getenv("DEBUG_PLUGIN_DETECTION") != nullptr);

                for (fs::directory_iterator i(dir); i != fs::directory_iterator(); ++i) {
                    // must be a file, and not a directory
                    try {
                        if (fs::is_directory(fs::absolute(*i))) {
                            if (extendedDebugLoggingEnabled)
                                ldLog() << LD_DEBUG << "Entry is a directory, skipping:" << i->path() << std::endl;

                            continue;
                        }

                        // file must be executable...
                        static constexpr auto isExecutable = fs::perms::owner_exec | fs::perms::group_exec | fs::perms::others_exec;
                        if (!(static_cast<unsigned int>(fs::status(*i).permissions()) & static_cast<unsigned int>(isExecutable))) {
                            if (extendedDebugLoggingEnabled)
                                ldLog() << LD_DEBUG << "File/symlink is not executable, skipping:" << i->path() << std::endl;

                            continue;
                        }
                    } catch(std::filesystem::filesystem_error const& ex) {
                        ldLog() << LD_DEBUG
                        << "Got file system error while looking for plugins :  " << ex.what() << std::endl
                        << "path1(): " << ex.path1() << std::endl
                        << "path2(): " << ex.path2() << std::endl
                        << "code().value():    " << ex.code().value() << std::endl
                        << "code().message():  " << ex.code().message() << std::endl
                        << "code().category(): " << ex.code().category().name() << std::endl;
                    }

                    // entry name must match regular expression
                    std::cmatch res;

                    if (!std::regex_match(i->path().filename().string().c_str(), res, PLUGIN_EXPR)) {
                        ldLog() << LD_DEBUG << "Doesn't match plugin regex, skipping:" << i->path() << std::endl;
                        continue;
                    }

                    try {
                        auto name = res[1].str();
                        auto* plugin = createPluginInstance(*i);
                        if (plugin == nullptr) {
                            ldLog() << LD_DEBUG << "Failed to create instance for plugin" << i->path() << std::endl;;
                        } else {
                            ldLog() << LD_DEBUG << "Found plugin '" << LD_NO_SPACE << name << LD_NO_SPACE << "':" << plugin->path() << std::endl;

                            if (foundPlugins.find(name) != foundPlugins.end()) {
                                ldLog() << LD_DEBUG << "Already found" << name << "plugin in" << foundPlugins[name]->path() << std::endl;
                            } else {
                                foundPlugins[name] = plugin;
                            }
                        }
                    } catch (const PluginError& e) {
                        ldLog() << LD_WARNING << "Could not load plugin" << i->path() << LD_NO_SPACE << ": " << e.what();
                    }
                }
            }
            return foundPlugins;
        }
    }
}
