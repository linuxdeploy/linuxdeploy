// system includes
#include <fstream>

// library includes
#include <boost/regex.hpp>
#include <subprocess.hpp>

// local headers
#include "linuxdeploy/core/elf.h"
#include "linuxdeploy/core/log.h"
#include "util.h"

using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace elf {
            // thrown by constructor if file is not an ELF file
            class ElfFileParseError : public std::runtime_error {
                public:
                    explicit ElfFileParseError(const std::string& msg) : std::runtime_error(msg) {}
            };

            class ElfFile::PrivateData {
                public:
                    const bf::path path;

                public:
                    explicit PrivateData(const bf::path& path) : path(path) {}
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
            }

            ElfFile::~ElfFile() {
                delete d;
            }

            std::vector<bf::path> ElfFile::traceDynamicDependencies() {
                // this method's purpose is to abstract this process
                // the caller doesn't care _how_ it's done, after all

                // for now, we use the same ldd based method linuxdeployqt uses

                std::vector<bf::path> paths;

                subprocess::Popen lddProc(
                    {"ldd", d->path.string().c_str()},
                    subprocess::output{subprocess::PIPE},
                    subprocess::error{subprocess::PIPE}
                );

                auto lddOutput = lddProc.communicate();
                auto& lddStdout = lddOutput.first;
                auto& lddStderr = lddOutput.second;

                if (lddProc.retcode() != 0) {
                    ldLog() << LD_ERROR << "Call to ldd failed:" << std::endl << lddStderr.buf.data() << std::endl;
                    return {};
                }

                std::string lddStdoutContents(lddStdout.buf.data());

                const boost::regex expr(R"(\s*(.+)\s+\=>\s+(.+)\s+\((.+)\)\s*)");
                boost::smatch what;

                for (const auto& line : util::splitLines(lddStdoutContents)) {
                    if (boost::regex_search(line, what, expr)) {
                        auto libraryPath = what[2].str();
                        util::trim(libraryPath);
                        paths.push_back(bf::absolute(libraryPath));
                    } else {
                        ldLog() << LD_DEBUG << "Invalid ldd output: " << line << std::endl;
                    }
                }

                return paths;
            }

            std::string getPatchelfPath() {
                // by default, try to use a patchelf next to the linuxdeploy binary
                // if that isn't available, fall back to searching for patchelf in the PATH
                std::string patchelfPath = "patchelf";

                // FIXME: reading /proc/self/exe line is Linux specific
                std::vector<char> buf(PATH_MAX, '\0');
                if (readlink("/proc/self/exe", buf.data(), buf.size()) != -1) {
                    auto binDirPath = bf::path(buf.data());
                    auto localPatchelfPath = binDirPath.parent_path() / "patchelf";
                    if (bf::exists(localPatchelfPath))
                        patchelfPath = localPatchelfPath.string();
                }

                ldLog() << LD_DEBUG << "Using patchelf:" << patchelfPath << std::endl;

                return patchelfPath;
            }

            std::string ElfFile::getRPath() {
                try {
                    subprocess::Popen patchelfProc(
                        {getPatchelfPath().c_str(), "--print-rpath", d->path.c_str()},
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    auto patchelfOutput = patchelfProc.communicate();
                    auto& patchelfStdout = patchelfOutput.first;
                    auto& patchelfStderr = patchelfOutput.second;

                    if (patchelfProc.retcode() != 0) {
                        std::string errStr(patchelfStderr.buf.data());

                        // if file is not an ELF executable, there is no need for a detailed error message
                        if (patchelfProc.retcode() == 1 && errStr.find("not an ELF executable")) {
                            return "";
                        } else {
                            ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << errStr;
                            return "";
                        }
                    }


                    std::string retval = patchelfStdout.buf.data();
                    util::trim(retval, '\n');
                    util::trim(retval);

                    return retval;
                } catch (const std::exception&) {
                    return "";
                }
            }

            bool ElfFile::setRPath(const std::string& value) {
                try {
                    subprocess::Popen patchelfProc(
                        {getPatchelfPath().c_str(), "--set-rpath", value.c_str(), d->path.c_str()},
                        subprocess::output(subprocess::PIPE),
                        subprocess::error(subprocess::PIPE)
                    );

                    auto patchelfOutput = patchelfProc.communicate();
                    auto& patchelfStdout = patchelfOutput.first;
                    auto& patchelfStderr = patchelfOutput.second;

                    if (patchelfProc.retcode() != 0) {
                        ldLog() << LD_ERROR << "Call to patchelf failed:" << std::endl << patchelfStderr.buf;
                        return false;
                    }
                } catch (const std::exception&) {
                    return false;
                }

                return true;
            }
        }
    }
}
