// system includes
#include <exception>
#include <string>
#include <utility>

#pragma once

namespace linuxdeploy {
    namespace util {
        namespace magic {
            // thrown by constructors if opening the magic database fails
            class MagicError : std::exception {
                private:
                    const std::string message;

                public:
                    explicit MagicError() : message() {}

                    explicit MagicError(std::string message) : message(std::move(message)) {}

                    const char* what() {
                        return message.c_str();
                    }
            };

            class Magic {
                private:
                    class PrivateData;
                    PrivateData *d;

                public:
                    Magic();
                    ~Magic();
            };
        }
    }
}
