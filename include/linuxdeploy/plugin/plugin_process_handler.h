#pragma once

// library headers
#include <boost/filesystem/path.hpp>

namespace linuxdeploy {
    namespace plugin {
        class plugin_process_handler {
        private:
            std::string name_;
            boost::filesystem::path path_;

        public:
            plugin_process_handler(std::string name, boost::filesystem::path path);

            int run(const boost::filesystem::path& appDir) const;
        };
    }
}
