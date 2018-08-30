// local includes
#include "copyright_dpkgquery.h"
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"

namespace linuxdeploy {
    namespace core {
        namespace copyright {
            using namespace log;

            std::vector<bf::path> DpkgQueryCopyrightFilesManager::getCopyrightFilesForPath(const bf::path& path) {
                auto call = [](const std::initializer_list<const char*>& args) {
                    auto proc = subprocess::Popen(
                        args,
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    auto output = proc.communicate();
                    return std::make_pair(proc.retcode(), output.first);
                };

                auto dpkgQueryPackages = call({"dpkg-query", "-S", path.c_str()});

                if (dpkgQueryPackages.first != 0
                    || dpkgQueryPackages.second.buf.empty()
                    || dpkgQueryPackages.second.buf.front() == '\0') {
                    ldLog() << LD_WARNING << "Could not find copyright files for file" << path << "using dpkg-query" << std::endl;
                    return {};
                }

                auto packageName = util::split(util::splitLines(dpkgQueryPackages.second.buf.data())[0], ':')[0];

                if (!packageName.empty()) {
                    auto copyrightFilePath = bf::path("/usr/share/doc") / packageName / "copyright";

                    if (bf::is_regular_file(copyrightFilePath)) {
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
