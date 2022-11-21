// local includes
#include "copyright_dpkgquery.h"
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/subprocess.h"

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            using namespace log;

            std::vector<fs::path> DpkgQueryCopyrightFilesManager::getCopyrightFilesForPath(const fs::path& path) {
                const auto realpath = fs::canonical(path);

                if (std::string(realpath) != std::string(path)) {
                    ldLog() << LD_DEBUG << "Canonical path" << realpath << "not equivalent to" << path << std::endl;
                }

                subprocess::subprocess proc{{"dpkg-query", "-S", realpath.c_str()}};

                auto result = proc.run();

                if (result.exit_code() != 0 || result.stdout_contents().empty()) {
                    ldLog() << LD_WARNING << "Could not find copyright files for file" << path << "using dpkg-query" << std::endl;
                    return {};
                }

                auto packageName = util::split(util::splitLines(result.stdout_string())[0], ':')[0];

                if (!packageName.empty()) {
                    auto copyrightFilePath = fs::path("/usr/share/doc") / packageName / "copyright";

                    if (fs::is_regular_file(copyrightFilePath)) {
                        return {copyrightFilePath};
                    }
                } else {
                    ldLog() << LD_WARNING << "Could not find copyright files for file" << path << "using dpkg-query" << std::endl;
                }

                return {};
            }
        }
    }
}
