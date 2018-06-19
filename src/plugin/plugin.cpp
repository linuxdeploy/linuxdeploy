// system headers
#include <set>
#include <string>
#include <vector>

// library headers
#include <boost/filesystem.hpp>
#include <fnmatch.h>
#include <subprocess.hpp>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/plugin/base.h"
#include "linuxdeploy/plugin/plugin.h"
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
    }
}
