// system headers
#include <algorithm>
#include <assert.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <regex>
#include <sys/mman.h>
#include <utility>

// local headers
#include "linuxdeploy/core/elf_file.h"
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/subprocess.h"

// DT_FLAGS_1 flag of position-independent executables (PIEs), missing in older C libraries' headers
#ifndef DF_1_PIE
#define DF_1_PIE 0x08000000
#endif

using namespace linuxdeploy::log;

namespace fs = std::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace elf_file {
            class ElfFile::PrivateData {
                public:
                    const fs::path path;
                    uint8_t elfClass = ELFCLASSNONE;
                    uint8_t elfABI = 0;
                    bool isDebugSymbolsFile = false;
                    bool isDynamicallyLinked = false;

                public:
                    explicit PrivateData(fs::path path) : path(std::move(path)) {}

                public:
                    static std::string getPatchelfPath() {
                        const auto patchelf = util::findTool("PATCHELF", "patchelf");
                        ldLog() << LD_DEBUG << "Using patchelf (" << LD_NO_SPACE << patchelf.source << LD_NO_SPACE << "):"
                                << patchelf.path << std::endl;
                        return patchelf.path;
                    }

                    // throws a PatchelfError if patchelf cannot be started or run, which fails the deployment; the
                    // callers handle a patchelf which ran, but failed
                    static subprocess::subprocess_result runPatchelf(std::vector<std::string> args) {
                        const auto patchelfPath = getPatchelfPath();
                        args.insert(args.begin(), patchelfPath);

                        auto run = [&]() {
                            try {
                                return subprocess::subprocess(args).run();
                            } catch (const std::exception& e) {
                                throw PatchelfError("Failed to start patchelf \"" + patchelfPath + "\": " + e.what());
                            }
                        };

                        auto result = run();

                        if (result.exec_error() != 0) {
                            // quoted, as the path may be just the name (e.g., "patchelf")
                            throw PatchelfError("Failed to find or run patchelf \"" + patchelfPath + "\": " +
                                                strerror(result.exec_error()));
                        }

                        return result;
                    }

                private:
                    template<typename Ehdr_T, typename Shdr_T, typename Phdr_T, typename Dyn_T>
                    void parseElfHeader(std::shared_ptr<uint8_t> data, size_t dataSize) {
                        // TODO: the following code will _only_ work if the native byte order equals the program's
                        // this should not be a big problem as we don't offer ARM builds yet, and require the user to
                        // use a matching binary for the target binaries

                        auto* ehdr = reinterpret_cast<Ehdr_T*>(data.get());

                        elfABI = ehdr->e_ident[EI_OSABI];

                        std::vector<Shdr_T> sections;

                        // parse section header table
                        // first, we collect all entries in a vector so we can conveniently iterate over it
                        for (uint64_t i = 0; i < ehdr->e_shnum; ++i) {
                            auto* nextShdr = reinterpret_cast<Shdr_T*>(data.get() + ehdr->e_shoff + i * sizeof(Shdr_T));
                            sections.emplace_back(*nextShdr);
                        }

                        auto getString = [data, &sections, ehdr](uint64_t offset) {
                            assert(ehdr->e_shstrndx != SHN_UNDEF);
                            const auto& stringTableSection = sections[ehdr->e_shstrndx];
                            return std::string{reinterpret_cast<char*>(data.get() + stringTableSection.sh_offset + offset)};
                        };

                        // now that we can look up texts, we can create a map to easily access the sections by name
                        std::unordered_map<std::string, Shdr_T> sectionsMap;
                        std::for_each(sections.begin(), sections.end(), [&sectionsMap, &getString](const Shdr_T& shdr) {
                            const auto headerName = getString(shdr.sh_name);
                            sectionsMap.insert(std::make_pair(headerName, shdr));
                        });

                        // this function is based on observations of the behavior of:
                        // - strip --only-keep-debug
                        // - objcopy --only-keep-debug
                        isDebugSymbolsFile = (sectionsMap[".text"].sh_type == SHT_NOBITS);

                        // https://stackoverflow.com/a/7298931
                        const Phdr_T* dynamicPhdr = nullptr;
                        for (uint64_t i = 0; i < ehdr->e_phnum; ++i) {
                            auto* nextPhdr = reinterpret_cast<Phdr_T*>(data.get() + ehdr->e_phoff + i * sizeof(Phdr_T));
                            switch (nextPhdr->p_type) {
                                case PT_INTERP:
                                    isDynamicallyLinked = true;
                                    break;
                                case PT_DYNAMIC:
                                    dynamicPhdr = nextPhdr;
                                    break;
                            }
                        }

                        // shared libraries have no PT_INTERP, but a PT_DYNAMIC segment
                        // static-pie executables look the same, but need no other libraries
                        // and are marked as PIE by the linker, which shared libraries never are
                        if (!isDynamicallyLinked && dynamicPhdr != nullptr)
                            isDynamicallyLinked = isDebugSymbolsFile || !isStaticPie<Dyn_T>(data, dataSize, dynamicPhdr);
                    }

                    // the caller must make sure the file has no PT_INTERP
                    // not suitable for debug symbols files, whose dynamic section has been stripped,
                    // so the segment points to unrelated data or beyond the end of the file
                    template<typename Dyn_T, typename Phdr_T>
                    static bool isStaticPie(const std::shared_ptr<uint8_t>& data, size_t dataSize, const Phdr_T* dynamicPhdr) {
                        if (dynamicPhdr->p_offset >= dataSize)
                            return false;

                        bool isPie = false;

                        const auto dynamicSize = std::min<uint64_t>(dynamicPhdr->p_filesz, dataSize - dynamicPhdr->p_offset);
                        const auto dynamicEntries = dynamicSize / sizeof(Dyn_T);
                        for (uint64_t i = 0; i < dynamicEntries; ++i) {
                            auto* dyn = reinterpret_cast<Dyn_T*>(data.get() + dynamicPhdr->p_offset + i * sizeof(Dyn_T));

                            if (dyn->d_tag == DT_NULL)
                                break;

                            // a PIE which needs other libraries but has no PT_INTERP must still be traced
                            // (e.g., linked with --no-dynamic-linker)
                            if (dyn->d_tag == DT_NEEDED)
                                return false;

                            if (dyn->d_tag == DT_FLAGS_1 && (dyn->d_un.d_val & DF_1_PIE) != 0)
                                isPie = true;
                        }

                        return isPie;
                    }

                public:
                    void readDataUsingElfAPI() {
                        int fd = open(path.c_str(), O_RDONLY);
                        auto map_size = static_cast<size_t>(lseek(fd, 0, SEEK_END));

                        std::shared_ptr<uint8_t> data(
                            static_cast<uint8_t*>(mmap(nullptr, map_size, PROT_READ, MAP_SHARED, fd, 0)),
                            [map_size](uint8_t* p) {
                                if (munmap(static_cast<void*>(p), map_size) != 0) {
                                    int error = errno;
                                    throw ElfFileParseError(std::string("Failed to call munmap(): ") + strerror(error));
                                }
                                p = nullptr;
                            }
                        );
                        close(fd);

                        // check which ELF "class" (32-bit or 64-bit) to use
                        // the "class" is available at a specific constant offset in the section e_ident, which
                        // happens to be the first section, so just reading one byte at EI_CLASS yields the data we're
                        // looking for
                        elfClass = data.get()[EI_CLASS];

                        switch (elfClass) {
                            case ELFCLASS32:
                                parseElfHeader<Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr, Elf32_Dyn>(data, map_size);
                                break;
                            case ELFCLASS64:
                                parseElfHeader<Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr, Elf64_Dyn>(data, map_size);
                                break;
                            default:
                                throw ElfFileParseError("Unknown ELF class: " + std::to_string(elfClass));
                        }
                    }
            };

            ElfFile::ElfFile(const std::filesystem::path& path) {
                // check if file exists
                if (!fs::exists(path))
                    throw ElfFileParseError("No such file or directory: " + path.string());

                // check magic bytes
                std::ifstream ifs(path.string());
                if (!ifs)
                    throw ElfFileParseError("Could not open file: " + path.string());

                std::vector<char> magicBytes(4);
                ifs.read(magicBytes.data(), 4);

                if (strncmp(magicBytes.data(), "\177ELF", 4) != 0)
                    throw ElfFileParseError("Invalid magic bytes in file header");

                d = new PrivateData(path);
                d->readDataUsingElfAPI();
            }

            ElfFile::~ElfFile() {
                delete d;
            }

            std::vector<fs::path> ElfFile::traceDynamicDependencies(const std::vector<std::string>& excludeLibraryPatterns) {
                // this method's purpose is to abstract this process
                // the caller doesn't care _how_ it's done, after all

                // for now, we use the same ldd based method linuxdeployqt uses

                // static ELF files have no dynamic dependencies
                if (!isDynamicallyLinked())
                    return {};

                std::vector<fs::path> paths;

                auto env = subprocess::get_environment();
                env["LC_ALL"] = "C";

                // workaround for https://sourceware.org/bugzilla/show_bug.cgi?id=25263
                // when you pass an absolute path to ldd, it can find libraries referenced in the rpath properly
                // this bug was first found when trying to find a library next to the binary which contained $ORIGIN
                // note that this is just a bug in ldd, the linker has always worked as intended
                const auto resolvedPath = fs::canonical(d->path);

                subprocess::subprocess lddProc({"ldd", resolvedPath.string()}, env);

                const auto result = lddProc.run();

                if (result.exit_code() != 0) {
                    if (result.stdout_string().find("not a dynamic executable") != std::string::npos || result.stderr_string().find("not a dynamic executable") != std::string::npos) {
                        ldLog() << LD_WARNING << this->d->path << "is not linked dynamically" << std::endl;
                        return {};
                    }

                    throw std::runtime_error{"Failed to run ldd: exited with code " + std::to_string(result.exit_code())};
                }

                const std::regex expr(R"(\s*(.+)\s+\=>\s+(.+)\s+\((.+)\)\s*)");
                std::smatch what;

                auto lddLines = util::splitLines(result.stdout_string());

                // filter known-problematic, known-unneeded lines
                // see https://github.com/linuxdeploy/linuxdeploy/issues/210
                lddLines.erase(
                    std::remove_if(lddLines.begin(), lddLines.end(), [&lddLines](auto line) {
                        if (util::stringContains(line, "linux-vdso.so") || util::stringContains(line, "ld-linux-")) {
                            ldLog() << LD_DEBUG << "skipping linker related object" << line << std::endl;
                            return true;
                        }

                        return false;
                    }),
                    lddLines.end()
                );

                for (const auto& line : lddLines) {
                    if (std::regex_search(line, what, expr)) {
                        auto libraryPath = what[2].str();
                        util::trim(libraryPath);
                        paths.push_back(fs::absolute(libraryPath));
                    } else {
                        if (util::stringContains(line, "=> not found")) {
                            auto missingLib = line;
                            static const std::string pattern = "=> not found";
                            missingLib.erase(missingLib.find(pattern), pattern.size());
                            util::trim(missingLib);
                            util::trim(missingLib, '\t');
                            if (!util::isInExcludelist(missingLib, excludeLibraryPatterns)) {
                                throw DependencyNotFoundError("Could not find dependency: " + missingLib);
                            }
                            ldLog() << LD_WARNING << resolvedPath.string() << "depends on excluded library:" << missingLib << std::endl;
                        } else {
                            ldLog() << LD_DEBUG << "Invalid ldd output: " << line << std::endl;
                        }
                    }
                }

                return paths;
            }

            std::string ElfFile::getRPath() {
                // only the dynamic section can contain an rpath, and patchelf fails on files without a dynamic section,
                // e.g., static executables, or debug symbols files, whose dynamic section has been stripped
                if (!isDynamicallyLinked() || isDebugSymbolsFile())
                    return "";

                const auto result = PrivateData::runPatchelf({"--print-rpath", d->path.string()});

                if (result.exit_code() != 0) {
                    // if file is not an ELF executable, there is no need for a detailed error message
                    if (result.exit_code() == 1 && result.stderr_string().find("not an ELF executable") != std::string::npos)
                        return "";

                    // throw instead of returning an empty rpath, which would replace the existing one
                    auto message = "Call to patchelf failed (" + result.exit_description() +
                                   "): could not read the rpath of " + d->path.string();
                    if (!result.stderr_string().empty())
                        message += "\n" + result.stderr_string();
                    throw PatchelfError(message);
                }

                auto stdoutContents = result.stdout_string();

                util::trim(stdoutContents, '\n');
                util::trim(stdoutContents);

                return stdoutContents;
            }

            bool ElfFile::setRPath(const std::string& value) {
                // calling (older versions of) patchelf on symlinks can lead to weird behavior, e.g., patchelf copying the
                // original file and patching the copy instead of patching the symlink target
                const auto canonicalPath = fs::canonical(d->path);

                ldLog() << LD_DEBUG << "Calling patchelf on canonical path" << canonicalPath << "instead of original path" << d->path << std::endl;

                const auto result = PrivateData::runPatchelf({"--set-rpath", value, canonicalPath.string()});

                if (result.exit_code() != 0) {
                    ldLog() << LD_ERROR << "Call to patchelf failed (" << LD_NO_SPACE << result.exit_description()
                            << LD_NO_SPACE << "):" << std::endl << result.stderr_string() << std::endl;
                    return false;
                }

                return true;
            }

            uint8_t ElfFile::getSystemElfABI() {
                // the only way to get the system's ELF ABI is to read the own executable using the ELF header,
                // and get the ELFOSABI flag
                auto self = std::shared_ptr<char>(realpath("/proc/self/exe", nullptr), [](char* p) { free(p); });

                if (self == nullptr)
                    throw ElfFileParseError("Could not read /proc/self/exe");

                std::ifstream ifs(self.get());

                if (!ifs)
                    throw ElfFileParseError("Could not open file: " + std::string(self.get()));

                // the "class" is available at a specific constant offset in the section e_ident, which
                // happens to be the first section, so just reading one byte at EI_CLASS yields the data we're
                // looking for
                ifs.seekg(EI_OSABI);

                char buf;
                ifs.read(&buf, 1);

                return static_cast<uint8_t>(buf);
            }

            uint8_t ElfFile::getSystemElfClass() {
                #if __SIZEOF_POINTER__ == 4
                return ELFCLASS32;
                #elif __SIZEOF_POINTER__ == 8
                return ELFCLASS64;
                #else
                #error "Invalid address size"
                #endif
            }

            uint8_t ElfFile::getSystemElfEndianness() {
                #if __BYTE_ORDER == __LITTLE_ENDIAN
                return ELFDATA2LSB;
                #elif __BYTE_ORDER == __BIG_ENDIAN
                return ELFDATA2MSB;
                #else
                #error "Unknown machine endianness"
                #endif
            }

            uint8_t ElfFile::getElfClass()  {
                return d->elfClass;
            }

            uint8_t ElfFile::getElfABI() {
                return d->elfABI;
            }

            bool ElfFile::isDebugSymbolsFile() {
                return d->isDebugSymbolsFile;
            }

            bool ElfFile::isDynamicallyLinked() {
                return d->isDynamicallyLinked;
            }
        }
    }
}
