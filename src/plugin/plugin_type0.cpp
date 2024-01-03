// system headers
#include <filesystem>
#include "plugin_type0.h"

using namespace linuxdeploy::log;

namespace fs = std::filesystem;

namespace linuxdeploy {
    namespace plugin {
        // it should suffice to just use the base class's constructor code
        Type0Plugin::Type0Plugin(const std::filesystem::path& path) : PluginBase(path) {}
    }
}
