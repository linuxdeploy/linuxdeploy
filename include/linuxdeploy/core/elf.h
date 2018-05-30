// system includes
#include <vector>
#include <string>

// library includes
#include <boost/filesystem.hpp>

#pragma once

namespace linuxdeploy {
    namespace core {
        namespace elf {
            class ElfFile {
                private:
                    class PrivateData;
                    PrivateData* d;

                public:
                    explicit ElfFile(const boost::filesystem::path& path);
                    ~ElfFile();

                public:
                    // recursively trace dynamic library dependencies of a given ELF file
                    // this works for both libraries and executables
                    // the resulting vector consists of absolute paths to the libraries determined by the same methods a system's
                    // linker would use
                    std::vector<boost::filesystem::path> traceDynamicDependencies();

                    // fetch rpath stored in binary
                    // it appears that according to the ELF standard, the rpath is ignored in libraries, therefore if the path
                    // points to an executable, an empty string is returned
                    std::string getRPath();

                    // set rpath in ELF file
                    // returns true on success, false otherwise
                    bool setRPath(const std::string& value);
            };
        }
    }
}
