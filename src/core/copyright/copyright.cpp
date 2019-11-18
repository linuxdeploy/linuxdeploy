// local includes
#include "linuxdeploy/core/log.h"
#include "copyright.h"

// specializations
#include "copyright_dpkgquery.h"

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            using namespace log;

            std::shared_ptr<ICopyrightFilesManager> ICopyrightFilesManager::getInstance() {
                // very simple but for our purposes good enough which like algorithm to find binaries in $PATH
                // TODO: move into separate function to be used in the entire code base
                // FIXME: remove dependency on subprocess library
                static const auto which = [](const std::string& name) -> bf::path {
                    const auto* path = getenv("PATH");

                    if (path == nullptr)
                        return "";

                    for (const auto& binDir : subprocess::util::split(path, ":")) {
                        if (!bf::is_directory(binDir)) {
                            continue;
                        }

                        for (bf::directory_iterator it(binDir); it != bf::directory_iterator{}; ++it) {
                            const auto binary = it->path();

                            std::cout << binary << std::endl;

                            if (binary.filename() == name) {
                                // TODO: check if file is executable (skip otherwise)
                                return binary;
                            }
                        }
                    }

                    return {};
                };

                if (!which("dpkg-query").empty()) {
                    ldLog() << LD_DEBUG << "Using dpkg-query to search for copyright files" << std::endl;
                    return std::make_shared<DpkgQueryCopyrightFilesManager>();
                }

                ldLog() << LD_DEBUG << "No usable copyright files manager implementation found" << std::endl;
                return nullptr;
            }
        }
    }
}
