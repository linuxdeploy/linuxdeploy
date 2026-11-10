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

            // thrown when dynamic dependencies cannot be determined
            class DependencyTraceError : public std::runtime_error {
                public:
                    explicit DependencyTraceError(const std::string& msg) : std::runtime_error(msg) {}
            };

            // thrown by traceDynamicDependencies() if a dependency is missing
            class DependencyNotFoundError : public DependencyTraceError {
                public:
                    explicit DependencyNotFoundError(const std::string& msg) : DependencyTraceError(msg) {}
            };

            // thrown by getRPath() and setRPath() if patchelf cannot be run, and by getRPath() if it fails to read the rpath
            class PatchelfError : public std::runtime_error {
                public:
                    explicit PatchelfError(const std::string& msg) : std::runtime_error(msg) {}
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
                    // static ELF files return an empty vector
                    // the resulting vector consists of absolute paths to the libraries determined by the same methods a system's
                    // linker would use
                    std::vector<std::filesystem::path> traceDynamicDependencies(const std::vector<std::string>& excludeLibraryPatterns={});

                    // fetch rpath stored in binary
                    // it appears that according to the ELF standard, the rpath is ignored in libraries, therefore if the path
                    // points to an executable, an empty string is returned
                    // static and debug symbols files have no rpath, so an empty string is returned for them, too
                    // throws PatchelfError if patchelf cannot be run or fails, as an empty rpath would replace the existing one
                    std::string getRPath();

                    // set rpath in ELF file
                    // returns true on success, false otherwise
                    // throws PatchelfError if patchelf cannot be run
                    bool setRPath(const std::string& value);

                    // return ELF class
                    uint8_t getElfClass();

                    // return OS ABI
                    uint8_t getElfABI();

                    // check if this file is a debug symbols file
                    bool isDebugSymbolsFile();

                    // check whether the file contains a program interpreter or dynamic section
                    // static-pie executables are not considered dynamically linked
                    bool isDynamicallyLinked();
            };
        }
    }
}
