// local includes
#include "copyright.h"

#pragma once

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            class DpkgQueryCopyrightFilesManager : public ICopyrightFilesManager {
                private:
                    class PrivateData;

                public:
                    std::vector<bf::path> getCopyrightFilesForPath(const bf::path& path) override;
            };
        }
    }
}
