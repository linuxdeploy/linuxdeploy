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
#include "util.h"
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

        std::vector<IPlugin*> findPlugins() {
            std::vector<IPlugin*> foundPlugins;

            const auto PATH = getenv("PATH");

            const boost::regex expr(R"(^linuxdeploy-plugin-([^\s\.-]+)(?:-[a-zA-Z0-9_]+)?(?:\..+)?$)");

            for (const auto& dir : util::split(PATH, ':')) {
                if (!bf::is_directory(dir))
                    continue;

                ldLog() << LD_DEBUG << "Searching for plugins in directory" << dir << std::endl;

                for (bf::directory_iterator i(dir); i != bf::directory_iterator(); ++i) {
                    // file must be executable...
                    if (bf::status(*i).permissions() & (bf::owner_exe | bf::group_exe | bf::others_exe)) {
                        // ... and filename must match regular expression
                        if (boost::regex_match((*i).path().filename().string(), expr)) {
                            try {
                                auto* plugin = createPluginInstance(*i);
                                ldLog() << LD_DEBUG << "Found plugin:" << plugin->path() << std::endl;
                                foundPlugins.push_back(plugin);
                            } catch (const PluginError& e) {
                                ldLog() << LD_WARNING << "Could not load plugin" << (*i).path() << LD_NO_SPACE << ": " << e.what();
                            }
                        }
                    }
                }
            }

            return foundPlugins;
        }
    }
}
