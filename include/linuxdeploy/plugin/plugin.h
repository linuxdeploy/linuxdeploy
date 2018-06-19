// system includes
#include <set>
#include <string>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "linuxdeploy/core/log.h"

#pragma once

namespace linuxdeploy {
    namespace plugin {
        /*
         * Searches for plugins in PATH (and in the current binary directory). Returns pointers to the plugin
         */

        /*
         * Exception class indicating that the plugin doesn't implement the right API level for the selected class
         */
        class WrongApiLevelError : public std::runtime_error {
            public:
                explicit WrongApiLevelError(const std::string& msg) : std::runtime_error(msg) {}
        };

        enum PLUGIN_TYPE {
            INPUT_TYPE = 0,
            OUTPUT_TYPE,
        };

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
        };

        /// Implementations are not public, see source directory for those headers ///

        /*
         * Factory function to create plugin from given executable.
         * This function automatically selects the correct subclass implementing the right API level, and
         */
        IPlugin* createPluginInstance(const boost::filesystem::path& path);
    }
}
