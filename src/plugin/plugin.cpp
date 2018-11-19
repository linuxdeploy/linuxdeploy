// system headers
#include <set>
#include <string>
#include <vector>

// library headers
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <fnmatch.h>
#include <subprocess.hpp>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/plugin/base.h"
#include "linuxdeploy/plugin/plugin.h"
#include "linuxdeploy/util/util.h"
#include "plugin_type0.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace plugin {
        IPlugin* createPluginInstance(const boost::filesystem::path& path) {
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

            auto currentExeDir = bf::path(util::getOwnExecutablePath()).parent_path();
            paths.insert(paths.begin(), currentExeDir.string());

            // if shipping as an AppImage, search for plugins in AppImage's location first
            // this way, plugins in the AppImage's directory take precedence over bundled ones
            if (getenv("APPIMAGE") != nullptr) {
                auto appImageDir = bf::path(getenv("APPIMAGE")).parent_path();
                paths.insert(paths.begin(), appImageDir.string());
            }

            // also, look for plugins in current working directory
            // could be useful in a "use linuxdeploy centrally, but download plugins into project directory" scenario
            std::shared_ptr<char> cwd(get_current_dir_name(), free);
            paths.emplace_back(cwd.get());

            for (const auto& dir : paths) {
                if (!bf::is_directory(dir))
                    continue;

                ldLog() << LD_DEBUG << "Searching for plugins in directory" << dir << std::endl;

                bool extendedDebugLoggingEnabled = (getenv("DEBUG_PLUGIN_DETECTION") != nullptr);

                for (bf::directory_iterator i(dir); i != bf::directory_iterator(); ++i) {
                    // must be a file, and not a directory
                    if (!(bf::is_directory(bf::absolute(*i)))) {
                        if (extendedDebugLoggingEnabled)
                            ldLog() << LD_DEBUG << "Entry is a directory, skipping:" << i->path() << std::endl;

                        continue;
                    }

                    // file must be executable...
                    if (!(bf::status(*i).permissions() & (bf::owner_exe | bf::group_exe | bf::others_exe))) {
                        if (extendedDebugLoggingEnabled)
                            ldLog() << LD_DEBUG << "File/symlink is not executable, skipping:" << i->path() << std::endl;

                        continue;
                    }

                    // entry name must match regular expression
                    boost::cmatch res;

                    if (!boost::regex_match(i->path().filename().string().c_str(), res, PLUGIN_EXPR)) {
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
