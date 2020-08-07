// system headers
#include <set>
#include <string>
#include <vector>

// library headers
#include <boost/filesystem.hpp>
#include <fnmatch.h>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/plugin/plugin.h"
#include "plugin_type0.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace plugin {
        // it should suffice to just use the base class's constructor code
        Type0Plugin::Type0Plugin(const boost::filesystem::path& path) : PluginBase(path) {}
    }
}
