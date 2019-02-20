// system includes
#include <fstream>
#include <memory>
#include <utility>

// library includes
#include <boost/regex.hpp>
#include <subprocess.hpp>
#include <sys/mman.h>

// local headers
#include "linuxdeploy/core/elf.h"
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"

using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace elf {
            class ElfFile::PrivateData {
                public:
                    const bf::path path;
                    uint8_t elfClass = ELFCLASSNONE;
                    uint8_t elfABI = 0;

                public:
                    explicit PrivateData(bf::path path) : path(std::move(path)) {}

                public:
                    static std::string getPatchelfPath() {
                        // by default, try to use a patchelf next to the linuxdeploy binary
                        // if that isn't available, fall back to searching for patchelf in the PATH
                        std::string patchelfPath;

                        auto binDirPath = bf::path(util::getOwnExecutablePath());
                        auto localPatchelfPath = binDirPath.parent_path() / "patchelf";

                        if (bf::exists(localPatchelfPath)) {
                            patchelfPath = localPatchelfPath.string();
                        } else {
                            for (const bf::path directory : util::split(getenv("PATH"), ':')) {
                                if (!bf::is_directory(directory))
                                    continue;

                                auto path = directory / "patchelf";

                                if (bf::is_regular_file(path)) {
                                    patchelfPath = path.string();
                                    break;
                                }
                            }
                        }

                        if (patchelfPath.empty()) {
                            ldLog() << LD_ERROR << "Could not find patchelf" << std::endl;
                            throw std::runtime_error("could not find patchelf");
                        }

                        ldLog() << LD_DEBUG << "Using patchelf:" << patchelfPath << std::endl;
                        return patchelfPath;
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
                            case ELFCLASS32: {
                                    auto* ehdr = reinterpret_cast<Elf32_Ehdr*>(data.get());
                                    auto* shdr = reinterpret_cast<Elf32_Shdr*>(data.get() + ehdr->e_shoff);

                                    elfABI = ehdr->e_ident[EI_OSABI];
                                }
                                break;
                            case ELFCLASS64: {
                                    auto* ehdr = reinterpret_cast<Elf32_Ehdr*>(data.get());
                                    auto* shdr = reinterpret_cast<Elf32_Shdr*>(data.get() + ehdr->e_shoff);

                                    elfABI = ehdr->e_ident[EI_OSABI];
                                }
                                break;
                            default:
                                throw ElfFileParseError("Unknown ELF class: " + std::to_string(elfClass));
                        }
                    }
            };

            ElfFile::ElfFile(const boost::filesystem::path& path) {
                // check if file exists
                if (!bf::exists(path))
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

            std::vector<bf::path> ElfFile::traceDynamicDependencies() {
                // this method's purpose is to abstract this process
                // the caller doesn't care _how_ it's done, after all

                // for now, we use the same ldd based method linuxdeployqt uses

                std::vector<bf::path> paths;

                std::map<std::string, std::string> env;
                env.insert(std::make_pair(std::string("LC_ALL"), std::string("C")));

                subprocess::Popen lddProc(
                        {"ldd", d->path.string().c_str()},
                        subprocess::output{subprocess::PIPE},
                        subprocess::error{subprocess::PIPE},
                        subprocess::environment(env)
                );

                auto outputAndError = util::subprocess::check_output_error(lddProc);

                const boost::regex expr(R"(\s*(.+)\s+\=>\s+(.+)\s+\((.+)\)\s*)");
                boost::smatch what;

                for (const auto& line : util::splitLines(outputAndError.first)) {
                    if (boost::regex_search(line, what, expr)) {
                        auto libraryPath = what[2].str();
                        util::trim(libraryPath);
                        paths.push_back(bf::absolute(libraryPath));
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
                try {
                    subprocess::Popen patchelfProc(
                        {PrivateData::getPatchelfPath().c_str(), "--print-rpath", d->path.c_str()},
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    auto patchelfOutput = util::subprocess::check_output_error(patchelfProc);
                    auto& patchelfStdout = patchelfOutput.first;
                    auto& patchelfStderr = patchelfOutput.second;

                    if (patchelfProc.retcode() != 0) {
                        // if file is not an ELF executable, there is no need for a detailed error message
                        if (patchelfProc.retcode() == 1 && patchelfStderr.find("not an ELF executable")) {
                            return "";
                        } else {
                            ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << patchelfStderr;
                            return "";
                        }
                    }

                    util::trim(patchelfStdout, '\n');
                    util::trim(patchelfStdout);

                    return patchelfStdout;
                } catch (const std::exception&) {
                    return "";
                }
            }

            bool ElfFile::setRPath(const std::string& value) {
                try {
                    subprocess::Popen patchelfProc(
                        {PrivateData::getPatchelfPath().c_str(), "--set-rpath", value.c_str(), d->path.c_str()},
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    const auto patchelfOutput = util::subprocess::check_output_error(patchelfProc);
                    const auto& patchelfStderr = patchelfOutput.second;

                    if (patchelfProc.retcode() != 0) {
                        ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << patchelfStderr << std::endl;
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
        }
    }
}
