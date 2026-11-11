// system headers
#include <algorithm>
#include <assert.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <sys/mman.h>
#include <utility>

// local headers
#include "linuxdeploy/core/elf_file.h"
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "excludelist.h"

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
                    // PT_INTERP, empty if there is none
                    std::string interpreter;

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
                                    if (nextPhdr->p_offset < dataSize) {
                                        const auto* value =
                                            reinterpret_cast<const char*>(data.get() + nextPhdr->p_offset);
                                        interpreter.assign(value, strnlen(value, std::min<uint64_t>(
                                            nextPhdr->p_filesz, dataSize - nextPhdr->p_offset)));
                                    }
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

            std::vector<fs::path> ElfFile::traceDynamicDependencies(const std::vector<std::string>& excludeLibraryPatterns,
                                                                    const fs::path& preloadFile,
                                                                    std::set<fs::path>* warnedPreloads) {
                // this method's purpose is to abstract this process
                // the caller doesn't care _how_ it's done, after all

                // for now, we use the same ldd based method linuxdeployqt uses

                // static ELF files have no dynamic dependencies
                if (!isDynamicallyLinked())
                    return {};

                std::vector<fs::path> paths;

                auto env = subprocess::get_environment();
                env["LC_ALL"] = "C";
                // FreeBSD's ldd lists libraries not named lib* without their path or whether they were found, so give
                // them the format of the others
                env["LD_TRACE_LOADED_OBJECTS_FMT2"] = "\t%o => %p (%x)\n";

                // workaround for https://sourceware.org/bugzilla/show_bug.cgi?id=25263
                // when you pass an absolute path to ldd, it can find libraries referenced in the rpath properly
                // this bug was first found when trying to find a library next to the binary which contained $ORIGIN
                // note that this is just a bug in ldd, the linker has always worked as intended
                const auto resolvedPath = fs::canonical(d->path);

                subprocess::subprocess lddProc({"ldd", resolvedPath.string()}, env);

                const auto result = lddProc.run();

                // exec* reports a missing ldd (e.g., Alpine without musl-utils) or interpreter (e.g., glibc's ldd
                // script without bash) as ENOENT; do not search for ldd up front, as which() follows other rules
                if (result.exec_error() == ENOENT)
                    throw DependencyTraceError("Failed to find or run ldd, which is required to trace the dependencies of " +
                                               resolvedPath.string() +
                                               " (it or its interpreter may need to be installed)");

                auto lddError = [&](const std::string& reason) {
                    std::ostringstream message;
                    message << "Failed to run ldd on " << resolvedPath.string() << ": " << reason;
                    if (!result.stdout_string().empty())
                        message << "\nstdout:\n" << result.stdout_string();
                    if (!result.stderr_string().empty())
                        message << "\nstderr:\n" << result.stderr_string();
                    return DependencyTraceError(message.str());
                };

                // the output of an ldd which could not be run or was terminated may be incomplete, even if all its
                // diagnostics would be accepted otherwise
                // strerror(), as exit_description() would repeat that ldd could not be run
                if (result.exec_error() != 0)
                    throw lddError(strerror(result.exec_error()));
                if (result.term_signal() != 0)
                    throw lddError(result.exit_description());

                // missing libraries are rejected, unless they are excluded
                auto checkMissingLibrary = [&](std::string library) {
                    util::trim(library);
                    util::trim(library, '\t');
                    // libraries needed by their path are excluded by their file name, too, like when they are found
                    if (!util::isInExcludelist(library, excludeLibraryPatterns) &&
                        !util::isInExcludelist(fs::path(library).filename(), excludeLibraryPatterns))
                        throw DependencyNotFoundError("Could not find dependency: " + library);
                    ldLog() << LD_WARNING << resolvedPath.string() << "depends on excluded library:" << library << std::endl;
                };

                if (result.exit_code() != 0) {
                    // glibc's ldd reports files it cannot load (e.g., for another architecture) as not dynamic; musl's
                    // and FreeBSD's diagnostics for them are rejected below
                    if (result.stdout_string().find("not a dynamic executable") != std::string::npos || result.stderr_string().find("not a dynamic executable") != std::string::npos) {
                        ldLog() << LD_WARNING << this->d->path << "is not linked dynamically" << std::endl;
                        return {};
                    }

                    // musl's ldd exits non-zero for missing libraries, but still lists the resolved ones; unlike
                    // glibc's ldd without -r, it also resolves symbols, which fails for plugins whose symbols the host
                    // provides
                    // accept excluded missing libraries and unresolved symbols, but reject all other errors, even for
                    // excluded libraries (e.g., one which exists, but is not a loadable ELF file)
                    static const std::regex muslMissingLibraryExpr(
                        R"(^Error loading shared library ([^:]+): No such file or directory(?: \(needed by .+\))?$)");
                    static const std::regex muslMissingSymbolExpr(
                        R"(^Error relocating (.+): (.+): symbol not found$)");

                    std::vector<std::string> missingLibraries;
                    std::vector<std::pair<std::string, std::string>> missingSymbols;
                    bool sawUnexpectedError = false;

                    for (auto& line : util::splitLines(result.stderr_string())) {
                        util::trim(line);

                        if (line.empty())
                            continue;

                        std::smatch match;
                        if (std::regex_match(line, match, muslMissingLibraryExpr)) {
                            missingLibraries.push_back(match[1].str());
                        } else if (std::regex_match(line, match, muslMissingSymbolExpr)) {
                            missingSymbols.emplace_back(match[1].str(), match[2].str());
                        } else {
                            sawUnexpectedError = true;
                        }
                    }

                    // musl reports every relocation, so count each symbol once per file
                    std::sort(missingSymbols.begin(), missingSymbols.end());
                    missingSymbols.erase(std::unique(missingSymbols.begin(), missingSymbols.end()),
                                         missingSymbols.end());

                    // check all diagnostics first, so a missing library cannot hide an unexpected error after it
                    // a non-zero exit status without known diagnostics is a failure, too
                    if (sawUnexpectedError || (missingLibraries.empty() && missingSymbols.empty()))
                        throw lddError(result.exit_description());

                    for (const auto& library : missingLibraries)
                        checkMissingLibrary(library);

                    // the symbols may be provided at runtime (e.g., by the program loading a plugin); as every deployed
                    // library is traced again, warn once with a count, and list them at debug level
                    if (!missingSymbols.empty()) {
                        ldLog() << LD_WARNING << resolvedPath.string() << LD_NO_SPACE << ": ignoring"
                                << missingSymbols.size()
                                << (missingSymbols.size() == 1 ? "unresolved symbol" : "unresolved symbols")
                                << "of it or its dependencies, which must be provided at runtime (e.g., by an excluded "
                                   "library or the program loading this file)" << std::endl;
                        ldLog() << LD_DEBUG << "ldd diagnostics for" << resolvedPath.string() << LD_NO_SPACE << ":"
                                << std::endl << result.stderr_string();
                    }
                }

                // FreeBSD's ldd appends an address to missing libraries, which the expression for resolved libraries
                // would match, so check for missing libraries first
                static const std::regex missingLibraryExpr(R"(\s*(.+)\s+=>\s+not found(?:\s+\([^)]+\))?\s*)");
                static const std::regex expr(R"(\s*(.+)\s+\=>\s+(.+)\s+\((.+)\)\s*)");
                // other objects are listed without "=>", by their path and address
                static const std::regex loadedObjectExpr(R"(\s*(.+)\s+\(([^)]+)\)\s*)");
                std::smatch what;

                const auto lddLines = util::splitLines(result.stdout_string());

                auto trimmed = [](std::string value) {
                    util::trim(value);
                    util::trim(value, '\t');
                    return value;
                };

                // entries without "=>" are the loader, the vDSO or, on glibc, libraries needed by their path
                // musl's libc is also its loader, which its ldd lists by the name it was run as (e.g.,
                // /lib/ld-musl-x86_64.so.1, or "ldd" for shared objects), and as the target of the libraries it
                // provides itself (e.g., libc.so), which must not be deployed; ordinary libraries are listed only once
                std::set<std::string> listedWithoutArrow;
                std::set<std::string> resolvedPaths;
                for (const auto& line : lddLines) {
                    if (std::regex_match(line, missingLibraryExpr))
                        continue;

                    if (!util::stringContains(line, "=>")) {
                        if (std::regex_match(line, what, loadedObjectExpr))
                            listedWithoutArrow.insert(trimmed(what[1].str()));
                    } else if (std::regex_search(line, what, expr)) {
                        resolvedPaths.insert(trimmed(what[2].str()));
                    }
                }

                // in ldd's output, names without a slash are not loaded files (e.g., the vDSO, or musl's "ldd")
                // recognize loaders by their file name, not by substrings, which other paths may contain (e.g.,
                // build-linux-x86_64/), also if they are listed with "=>", like glibc 2.36 did (see #210)
                // recognize the traced file's PT_INTERP by its path, too (e.g., LSB's ld-lsb-x86-64.so.3)
                // musl's loaders are named ld-musl-<arch>.so.1, and no arch contains a dot
                auto isInterpreter = [this](const std::string& path) {
                    return !d->interpreter.empty() && path == d->interpreter;
                };
                auto isRuntimeFile = [&](const std::string& path) {
                    static const std::regex loaderFileNameExpr(
                        R"(ld-linux(-.+)?\.so\..+|ld(64)?\.so\..+|ld-musl-[^.]+\.so\.1)");
                    return !util::stringContains(path, "/") || isInterpreter(path) ||
                           std::regex_match(fs::path(path).filename().string(), loaderFileNameExpr);
                };

                // loaders open libraries needed by their path (e.g., an absolute DT_NEEDED) only at that path, and use
                // a deployed copy only if the path is relative to $ORIGIN (which glibc's ldd lists expanded), so
                // deploy them, but warn; ldd lists the needs of all loaded files, so the file needing it is unknown
                auto warnNeededByPath = [&](const std::string& name, const fs::path& libraryPath) {
                    const auto fileName = libraryPath.filename();
                    if (util::isInExcludelist(fileName, excludeLibraryPatterns) ||
                        util::isInExcludelist(fileName, generatedExcludelist)) {
                        ldLog() << LD_DEBUG << resolvedPath.string() << "(or one of its dependencies) needs excluded "
                                   "library" << name << "by its path" << std::endl;
                        return;
                    }

                    ldLog() << LD_WARNING << resolvedPath.string() << "(or one of its dependencies) needs" << name
                            << "by its path, so it must exist at that path at runtime, unless the path is relative to "
                               "$ORIGIN and the copy is deployed there" << std::endl;
                };

                // ldd lists the libraries $LD_PRELOAD and /etc/ld.so.preload preload by their entry there, followed by
                // "=>" and the path if it is a name or contains tokens (e.g., /usr/$LIB/libfoo.so)
                // $LD_PRELOAD is kept for ldd, as wrappers rely on it (e.g., fakechroot redirects the file system), but
                // all entries of both are left out, also names (e.g., fakeroot's library)
                // the preloaded libraries' own dependencies are still deployed, as ldd lists them like the program's
                const auto preloadVariable = env.find("LD_PRELOAD");
                const auto preloadVariableEntries = preloadVariable == env.end()
                                                    ? std::vector<std::string>{}
                                                    : util::splitPreloadList(preloadVariable->second);
                const auto preloadFileEntries = util::readPreloadEntries(preloadFile);
                // returns what preloads the library listed as name and found at libraryPath, or an empty string
                auto preloadedBy = [&](const std::string& name, const std::string& libraryPath) -> std::string {
                    auto lists = [&](const std::vector<std::string>& entries) {
                        return std::find(entries.begin(), entries.end(), name) != entries.end() ||
                               std::find(entries.begin(), entries.end(), libraryPath) != entries.end();
                    };
                    if (lists(preloadVariableEntries))
                        return "$LD_PRELOAD";
                    if (lists(preloadFileEntries))
                        return preloadFile.string();
                    return {};
                };
                // a library which is preloaded and needed, too, is only listed as preloaded, so warn about each
                // name the user can pass to --library, and the entry if it differs (e.g., contains $LIB)
                // callers which pass no set (e.g., plugins tracing every file in the AppDir) do not deploy what they
                // trace, so they only get a debug message
                auto skipPreloaded = [&](const std::string& entry, const fs::path& libraryPath,
                                         const std::string& preloader) {
                    if (warnedPreloads == nullptr || !warnedPreloads->insert(libraryPath).second) {
                        ldLog() << LD_DEBUG << "skipping preloaded library" << libraryPath << std::endl;
                        return;
                    }
                    const auto listedAs = entry == libraryPath.string() ? "" : " (" + entry + ")";
                    ldLog() << LD_WARNING << "Not deploying" << libraryPath.string() + listedAs << LD_NO_SPACE
                            << ", which" << preloader
                            << "preloads, deploy it with --library if the program needs it, too" << std::endl;
                };

                for (const auto& line : lddLines) {
                    if (std::regex_match(line, what, missingLibraryExpr)) {
                        checkMissingLibrary(what[1].str());
                        continue;
                    }

                    if (!util::stringContains(line, "=>")) {
                        if (!std::regex_match(line, what, loadedObjectExpr)) {
                            ldLog() << LD_DEBUG << "Invalid ldd output: " << line << std::endl;
                            continue;
                        }

                        // musl's loader may have any name, but its libc aliases resolve to it
                        const auto name = trimmed(what[1].str());
                        if (isRuntimeFile(name) || resolvedPaths.count(name) != 0) {
                            ldLog() << LD_DEBUG << "skipping runtime object" << line << std::endl;
                        } else if (const auto preloader = preloadedBy(name, name); !preloader.empty()) {
                            skipPreloaded(name, name, preloader);
                        } else {
                            warnNeededByPath(name, name);
                            paths.push_back(fs::absolute(name));
                        }
                        continue;
                    }

                    if (std::regex_search(line, what, expr)) {
                        const auto name = trimmed(what[1].str());
                        const auto libraryPath = trimmed(what[2].str());

                        // ldd lists loaders by their path, also with "=>" (glibc 2.36), so a name without a slash
                        // was searched for, and is a library even if it is named like a loader (e.g., ld.so.1, a
                        // loader only on some Linux architectures)
                        // glibc lists a renamed PT_INTERP with "=>" and the loader's path
                        if (!util::stringContains(libraryPath, "/") ||
                            (util::stringContains(name, "/") && isRuntimeFile(name)) ||
                            listedWithoutArrow.count(libraryPath) != 0) {
                            ldLog() << LD_DEBUG << "skipping runtime object" << line << std::endl;
                            continue;
                        }

                        if (const auto preloader = preloadedBy(name, libraryPath); !preloader.empty()) {
                            skipPreloaded(name, libraryPath, preloader);
                            continue;
                        }

                        // musl's and FreeBSD's ldd list libraries needed by their path with "=>"
                        if (util::stringContains(name, "/"))
                            warnNeededByPath(name, libraryPath);

                        paths.push_back(fs::absolute(libraryPath));
                    } else {
                        ldLog() << LD_DEBUG << "Invalid ldd output: " << line << std::endl;
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
