// system headers
#include <filesystem>
#include <string>

// local headers
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/plugin/base.h"

#pragma once

namespace linuxdeploy {
    namespace plugin {
        /*
         * Type 0 plugin type
         * This is the type used during the development of the initial plugin system.
         */
        class Type0Plugin : public base::PluginBase<0> {
            public:
                explicit Type0Plugin(const std::filesystem::path& path);
        };
    }
}
