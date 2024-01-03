// system includes
#include <filesystem>
#include <string>

// local includes
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/plugin/plugin.h"

#pragma once

namespace linuxdeploy {
    namespace plugin {
        namespace base {
            /*
             * Base class for plugins.
             */
            template<int API_LEVEL>
            class PluginBase : public IPlugin {
                private:
                    // private data class pattern
                    class PrivateData;

                    PrivateData* d;

                public:
                    // default constructor
                    // construct Plugin from given path
                    explicit PluginBase(const std::filesystem::path& path);

                    ~PluginBase() override;

                public:
                    int apiLevel() const override;

                    // get path to plugin
                    std::filesystem::path path() const override;

                    // get plugin type
                    PLUGIN_TYPE pluginType() const override;
                    std::string pluginTypeString() const override;

                    // run plugin
                    int run(const std::filesystem::path& appDirPath) override;
            };
        }
    }
}

// need to include implementation at the end of the file to solve issues like
// https://bytefreaks.net/programming-2/c/c-undefined-reference-to-templated-class-function
#include "linuxdeploy/plugin/base_impl.h"
