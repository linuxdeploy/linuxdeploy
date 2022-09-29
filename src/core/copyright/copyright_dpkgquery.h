// system headers
#include <filesystem>

// local headers
#include "copyright.h"

#pragma once

namespace fs = std::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            class DpkgQueryCopyrightFilesManager : public ICopyrightFilesManager {
                private:
                    class PrivateData;

                public:
                    std::vector<fs::path> getCopyrightFilesForPath(const fs::path& path) override;
            };
        }
    }
}
