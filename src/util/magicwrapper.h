// system includes
#include <exception>
#include <string>
#include <stdexcept>
#include <utility>

#pragma once

namespace linuxdeploy {
    namespace util {
        namespace magic {
            // thrown by constructors if opening the magic database fails
            class MagicError : public std::runtime_error {
                public:
                    explicit MagicError(const std::string& msg) : std::runtime_error(msg) {}
            };

            class Magic {
                private:
                    class PrivateData;
                    PrivateData *d;

                public:
                    Magic();
                    ~Magic();

                public:
                    // returns MIME-style description of <path>
                    std::string fileType(const std::string& path);
            };
        }
    }
}
