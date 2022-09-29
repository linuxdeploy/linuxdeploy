// system includes
#include <filesystem>
#include <vector>
#include <string>
// including system elf header, which allows for interpretation of the return values of the methods
#include <elf.h>

#pragma once

namespace linuxdeploy {
    namespace core {
        namespace elf_file {
            // thrown by constructor if file is not an ELF file
            class ElfFileParseError : public std::runtime_error {
                public:
                    explicit ElfFileParseError(const std::string& msg) : std::runtime_error(msg) {}
            };

            // thrown by traceDynamicDependencies() if a dependency is missing
            class DependencyNotFoundError : public std::runtime_error {
                public:
                    explicit DependencyNotFoundError(const std::string& msg) : std::runtime_error(msg) {}
            };

            class ElfFile {
                private:
                    class PrivateData;
                    PrivateData* d;

                public:
                    explicit ElfFile(const std::filesystem::path& path);
                    ~ElfFile();

                public:
                    // return system ELF OS ABI
                    static uint8_t getSystemElfABI();

                    // return system ELF class (32-bit or 64-bit)
                    static uint8_t getSystemElfClass();

                    // return system (ELF) endianness
                    static uint8_t getSystemElfEndianness();

                public:
                    // recursively trace dynamic library dependencies of a given ELF file
                    // this works for both libraries and executables
                    // the resulting vector consists of absolute paths to the libraries determined by the same methods a system's
                    // linker would use
                    std::vector<std::filesystem::path> traceDynamicDependencies();

                    // fetch rpath stored in binary
                    // it appears that according to the ELF standard, the rpath is ignored in libraries, therefore if the path
                    // points to an executable, an empty string is returned
                    std::string getRPath();

                    // set rpath in ELF file
                    // returns true on success, false otherwise
                    bool setRPath(const std::string& value);

                    // return ELF class
                    uint8_t getElfClass();

                    // return OS ABI
                    uint8_t getElfABI();

                    // check if this file is a debug symbols file
                    bool isDebugSymbolsFile();

                    // check whether the file contains a dynsym section
                    bool isDynamicallyLinked();
            };
        }
    }
}
