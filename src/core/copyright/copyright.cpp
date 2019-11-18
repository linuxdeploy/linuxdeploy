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
                auto check_command = [](const std::string& command) {
                    auto p = subprocess::Popen(
                        command,
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    return p.wait();
                };

                if (check_command("command -v dpkg-query") == 0) {
                    ldLog() << LD_DEBUG << "Using dpkg-query to search for copyright files" << std::endl;
                    return std::make_shared<DpkgQueryCopyrightFilesManager>();
                }

                ldLog() << LD_DEBUG << "No usable copyright files manager implementation found" << std::endl;
                return nullptr;
            }
        }
    }
}
