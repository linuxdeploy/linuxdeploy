#pragma once

// system headers
#include <filesystem>

namespace linuxdeploy {
    namespace plugin {
        class plugin_process_handler {
        private:
            std::string name_;
            std::filesystem::path path_;

        public:
            plugin_process_handler(std::string name, std::filesystem::path path);

            int run(const std::filesystem::path& appDir) const;
        };
    }
}
