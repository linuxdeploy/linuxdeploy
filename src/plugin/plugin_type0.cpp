// system headers
#include <filesystem>
#include <set>
#include <string>
#include <vector>

// library headers
#include <fnmatch.h>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/plugin/plugin.h"
#include "plugin_type0.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

namespace fs = std::filesystem;

namespace linuxdeploy {
    namespace plugin {
        // it should suffice to just use the base class's constructor code
        Type0Plugin::Type0Plugin(const std::filesystem::path& path) : PluginBase(path) {}
    }
}
