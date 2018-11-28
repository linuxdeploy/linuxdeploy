// system includes
#include <map>
#include <string>
#include <regex>

// local includes
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/plugin/exceptions.h"

#pragma once

namespace linuxdeploy {
    namespace plugin {
        enum PLUGIN_TYPE {
            INPUT_TYPE = 0,
            OUTPUT_TYPE,
        };

        /*
         * Official regular expression to check filenames for plugins
         */
        static const std::regex PLUGIN_EXPR(R"(^linuxdeploy-plugin-([^\s\.-]+)(?:-[^\.]+)?(?:\..+)?$)");

        /*
         * Plugin interface.
         */
        class IPlugin {
                // declare interface's constructors and destructor
            protected:
                explicit IPlugin(const boost::filesystem::path& path) {};

                // deconstructor apparently needs to be defined, not just declared (with = 0)
                virtual ~IPlugin() = default;

            public:
                virtual boost::filesystem::path path() const = 0;
                virtual int apiLevel() const = 0;
                virtual PLUGIN_TYPE pluginType() const = 0;
                virtual std::string pluginTypeString() const = 0;
                virtual int run(const boost::filesystem::path& appDirPath) = 0;
        };

        /// Implementations are not public, see source directory for those headers ///

        /*
         * Factory function to create plugin from given executable.
         * This function automatically selects the correct subclass implementing the right API level, and
         */
        IPlugin* createPluginInstance(const boost::filesystem::path& path);

        /*
         * Finds all linuxdeploy plugins in $PATH and the current executable's directory and returns IPlugin instances for them.
         */
        std::map<std::string, IPlugin*> findPlugins();
    }
}
