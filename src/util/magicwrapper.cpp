// system includes
#include <magic.h>
#include <string>

// local includes
#include "magicwrapper.h"

namespace linuxdeploy {
    namespace util {
        namespace magic {
            class Magic::PrivateData {
                public:
                    magic_t cookie;

                public:
                    PrivateData() noexcept(false) {
                        cookie = magic_open(MAGIC_CHECK | MAGIC_MIME_TYPE | MAGIC_MIME_ENCODING);

                        if (cookie == nullptr)
                            throw MagicError("Failed to open magic database: " + std::string(magic_error(cookie)));

                        // load magic data from default location
                        if (magic_load(cookie, nullptr) != 0)
                            throw MagicError("Failed to load magic data: " + std::string(magic_error(cookie)));
                    }

                    ~PrivateData() {
                        if (cookie != nullptr) {
                            magic_close(cookie);
                            cookie = nullptr;
                        }
                    };
            };

            Magic::Magic() {
                d = new PrivateData();
            }

            Magic::~Magic() {
                delete d;
            }

            std::string Magic::fileType(const std::string& path) {
                const auto* buf = magic_file(d->cookie, path.c_str());

                if (buf == nullptr)
                    throw MagicError("magic_file() failed: " + std::string(magic_error(d->cookie)));

                return buf;
            }
        }
    }
}
