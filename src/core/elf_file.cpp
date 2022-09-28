// system headers
#include <assert.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <regex>
#include <sys/mman.h>
#include <utility>

// local headers
#include "linuxdeploy/core/elf_file.h"
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/subprocess.h"

using namespace linuxdeploy::core::log;

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
                        // by default, try to use a patchelf next to the linuxdeploy binary
                        // if that isn't available, fall back to searching for patchelf in the PATH
                        std::string patchelfPath;

                        const auto envPatchelf = getenv("PATCHELF");

                        // allows users to use a custom patchelf instead of the bundled one
                        if (envPatchelf != nullptr) {
                            ldLog() << LD_DEBUG << "Using patchelf specified in $PATCHELF:" << envPatchelf << std::endl;
                            patchelfPath = envPatchelf;
                        } else {
                            auto binDirPath = fs::path(util::getOwnExecutablePath());
                            auto localPatchelfPath = binDirPath.parent_path() / "patchelf";

                            if (fs::exists(localPatchelfPath)) {
                                patchelfPath = localPatchelfPath.string();
                            } else {
                                for (const fs::path directory : util::split(getenv("PATH"), ':')) {
                                    if (!fs::is_directory(directory))
                                        continue;

                                    auto path = directory / "patchelf";

                                    if (fs::is_regular_file(path)) {
                                        patchelfPath = path.string();
                                        break;
                                    }
                                }
                            }
                        }

                        if (!fs::is_regular_file(patchelfPath)) {
                            ldLog() << LD_ERROR << "Could not find patchelf: no such file:" << patchelfPath << std::endl;
                            throw std::runtime_error("Could not find patchelf");
                        }

                        ldLog() << LD_DEBUG << "Using patchelf:" << patchelfPath << std::endl;
                        return patchelfPath;
                    }

                private:
                    template<typename Ehdr_T, typename Shdr_T, typename Phdr_T>
                    void parseElfHeader(std::shared_ptr<uint8_t> data) {
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
                        for (uint64_t i = 0; i < ehdr->e_phnum && !isDynamicallyLinked; ++i) {
                            auto* nextPhdr = reinterpret_cast<Phdr_T*>(data.get() + ehdr->e_phoff + i * sizeof(Phdr_T));
                            switch (nextPhdr->p_type) {
                                case PT_DYNAMIC:
                                case PT_INTERP:
                                    isDynamicallyLinked = true;
                                    break;
                            }
                        }
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
                                parseElfHeader<Elf32_Ehdr, Elf32_Shdr, Elf32_Phdr>(data);
                                break;
                            case ELFCLASS64:
                                parseElfHeader<Elf64_Ehdr, Elf64_Shdr, Elf64_Phdr>(data);
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

            std::vector<fs::path> ElfFile::traceDynamicDependencies() {
                // this method's purpose is to abstract this process
                // the caller doesn't care _how_ it's done, after all

                // for now, we use the same ldd based method linuxdeployqt uses

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
                            throw DependencyNotFoundError("Could not find dependency: " + missingLib);
                        } else {
                            ldLog() << LD_DEBUG << "Invalid ldd output: " << line << std::endl;
                        }
                    }
                }

                return paths;
            }

            std::string ElfFile::getRPath() {
                // don't try to fetch patchelf path in a catchall to make sure the process exists when the tool cannot be found
                const auto patchelfPath = PrivateData::getPatchelfPath();

                try {
                    subprocess::subprocess patchelfProc({patchelfPath, "--print-rpath", d->path.string()});

                    const auto result = patchelfProc.run();

                    if (result.exit_code() != 0) {
                        // if file is not an ELF executable, there is no need for a detailed error message
                        if (result.exit_code() == 1 && result.stderr_string().find("not an ELF executable") != std::string::npos) {
                            return "";
                        } else {
                            ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << result.stderr_string();
                            return "";
                        }
                    }

                    auto stdoutContents = result.stdout_string();

                    util::trim(stdoutContents, '\n');
                    util::trim(stdoutContents);

                    return stdoutContents;
                } catch (const std::exception&) {
                    return "";
                }
            }

            bool ElfFile::setRPath(const std::string& value) {
                // don't try to fetch patchelf path in a catchall to make sure the process exists when the tool cannot be found
                const auto patchelfPath = PrivateData::getPatchelfPath();

                // calling (older versions of) patchelf on symlinks can lead to weird behavior, e.g., patchelf copying the
                // original file and patching the copy instead of patching the symlink target
                const auto canonicalPath = fs::canonical(d->path);

                ldLog() << LD_DEBUG << "Calling patchelf on canonical path" << canonicalPath << "instead of original path" << d->path << std::endl;

                try {
                    subprocess::subprocess patchelfProc({patchelfPath.c_str(), "--set-rpath", value.c_str(), canonicalPath.c_str()});

                    const auto result = patchelfProc.run();

                    if (result.exit_code() != 0) {
                        ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << result.stderr_string() << std::endl;
                        return false;
                    }
                } catch (const std::exception&) {
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
