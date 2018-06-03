// system includes
#include <magic.h>
#include <string>

// local includes
#include "magicwrapper.h"

namespace linuxdeploy {
    namespace util {
        namespace magic {
            class MagicWrapper::PrivateData {
                private:
                    magic_t cookie;

                public:
                    PrivateData() noexcept(false) {
                        cookie = magic_open(MAGIC_DEBUG | MAGIC_SYMLINK | MAGIC_MIME_TYPE);

                        if (cookie == nullptr)
                            throw MagicError("Failed to open magic database");
                    }

                    ~PrivateData() {
                        if (cookie != nullptr) {
                            magic_close(cookie);
                            cookie = nullptr;
                        }
                    };

                    std::string fileType(const std::string& path) {
                        const auto* buf = magic_file(cookie, path.c_str());

                        if (buf == nullptr)
                            return "";

                        return buf;
                    }
            };

            MagicWrapper::MagicWrapper() {
                d = new PrivateData();
            }

            MagicWrapper::~MagicWrapper() {
                delete d;
            }
        }
    }
}
