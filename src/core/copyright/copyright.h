// system includes
#include <memory>
#include <vector>

#pragma once

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            class CopyrightFilesManagerError : public std::runtime_error {
                explicit CopyrightFilesManagerError(const std::string& msg) : std::runtime_error(msg) {}
            };

            class ICopyrightFilesManager {
                public:
                    static std::shared_ptr<ICopyrightFilesManager> getInstance();

                public:
                    virtual std::vector<std::filesystem::path> getCopyrightFilesForPath(const std::filesystem::path& path) = 0;
            };
        }
    }
}
