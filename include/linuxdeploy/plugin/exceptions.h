// system includes
#include <exception>

#pragma once

namespace linuxdeploy {
    namespace plugin {
        /*
         * Exception class indicating that the plugin doesn't implement the right API level for the selected class
         */
        class PluginError : public std::runtime_error {
            public:
                explicit PluginError(const std::string& msg) : std::runtime_error(msg) {}
        };

        /*
         * Exception class indicating that the plugin doesn't implement the right API level for the selected class
         */
        class WrongApiLevelError : public PluginError {
            public:
                explicit WrongApiLevelError(const std::string& msg) : PluginError(msg) {}
        };
    }
}
