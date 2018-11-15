#pragma once

// system includes
#include <stdexcept>
#include <string>

namespace linuxdeploy {
    namespace core {
        namespace desktopfile {
            /**
             * Desktop file library's base exception.
             */
            class DesktopFileError : public std::runtime_error {
            public:
                explicit DesktopFileError(const std::string& message = "unknown desktop file error") : runtime_error(message) {};
            };

            /**
             * Exception thrown by DesktopFileReader on parsing errors.
             */
            class ParseError : public DesktopFileError {
            public:
                explicit ParseError(const std::string& message = "unknown parse error") : DesktopFileError(message) {};
            };

            /**
             * I/O exception, thrown if files cannot be opened, reading or writing fails etc.
             */
             class IOError : public DesktopFileError {
             public:
                 explicit IOError(const std::string& message = "unknown I/O error") : DesktopFileError(message) {};
             };

             class UnknownSectionError : public DesktopFileError {
             public:
                 explicit UnknownSectionError(const std::string& section) : DesktopFileError("unknown section: " + section) {};
             };
        }
    }
}
