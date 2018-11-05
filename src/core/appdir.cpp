// system headers
#include <set>
#include <string>
#include <vector>

// library headers
#include <boost/filesystem.hpp>
#include <CImg.h>
#include <fnmatch.h>
#include <subprocess.hpp>


// local headers
#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/core/elf.h"
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/util/util.h"
#include "copyright.h"

// auto-generated headers
#include "excludelist.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

using namespace cimg_library;
namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace appdir {
            class AppDir::PrivateData {
                public:
                    bf::path appDirPath;

                    // store deferred operations
                    // these can be executed by calling excuteDeferredOperations
                    std::map<bf::path, bf::path> copyOperations;
                    std::set<bf::path> stripOperations;
                    std::map<bf::path, std::string> setElfRPathOperations;

                    // stores all files that have been visited by the deploy functions, e.g., when they're blacklisted,
                    // have been added to the deferred operations already, etc.
                    // lookups in a single container are a lot faster than having to look up in several ones, therefore
                    // the little amount of additional memory is worth it, considering the improved performance
                    std::set<bf::path> visitedFiles;

                    // used to automatically rename resources to improve the UX, e.g. icons
                    std::string appName;

                    std::shared_ptr<copyright::ICopyrightFilesManager> copyrightFilesManager;

                public:
                    PrivateData() : copyOperations(), stripOperations(), setElfRPathOperations(), visitedFiles(), appDirPath(), appName() {
                        copyrightFilesManager = copyright::ICopyrightFilesManager::getInstance();
                    };

                public:
                    // calculate library directory name for given ELF file, taking system architecture into account
                    static std::string getLibraryDirName(const bf::path& path) {
                        const auto systemElfClass = elf::ElfFile::getSystemElfClass();
                        const auto elfClass = elf::ElfFile(path).getElfClass();

                        std::string libDirName = "lib";

                        if (systemElfClass != elfClass) {
                            if (elfClass == ELFCLASS32)
                                libDirName += "32";
                            else
                                libDirName += "64";
                        }

                         return libDirName;
                    }

                    // actually copy file
                    // mimics cp command behavior
                    bool copyFile(const bf::path& from, bf::path to, bool overwrite = false) {
                        ldLog() << "Copying file" << from << "to" << to << std::endl;

                        try {
                            if (!to.parent_path().empty() && !bf::is_directory(to.parent_path()) && !bf::create_directories(to.parent_path())) {
                                ldLog() << LD_ERROR << "Failed to create parent directory" << to.parent_path() << "for path" << to << std::endl;
                                return false;
                            }

                            if (*(to.string().end() - 1) == '/' || bf::is_directory(to))
                                to /= from.filename();

                            if (!overwrite && bf::exists(to)) {
                                ldLog() << LD_DEBUG << "File exists, skipping:" << to << std::endl;
                                return true;
                            }

                            bf::copy_file(from, to, bf::copy_option::overwrite_if_exists);
                        } catch (const bf::filesystem_error& e) {
                            ldLog() << LD_ERROR << "Failed to copy file" << from << "to" << to << LD_NO_SPACE << ":" << e.what() << std::endl;
                            return false;
                        }

                        return true;
                    }

                    // create symlink
                    bool symlinkFile(const bf::path& target, const bf::path& symlink, const bool useRelativePath = true) {
                        ldLog() << "Creating symlink for file" << target << "in/as" << symlink << std::endl;

                        /*try {
                            if (!symlink.parent_path().empty() && !bf::is_directory(symlink.parent_path()) && !bf::create_directories(symlink.parent_path())) {
                                ldLog() << LD_ERROR << "Failed to create parent directory" << symlink.parent_path() << "for path" << symlink << std::endl;
                                return false;
                            }

                            if (*(symlink.string().end() - 1) == '/' || bf::is_directory(symlink))
                                symlink /= target.filename();

                            if (bf::exists(symlink) || bf::symbolic_link_exists(symlink))
                                bf::remove(symlink);

                            if (relativeDirectory != "") {
                                // TODO
                            }

                            bf::create_symlink(target, symlink);
                        } catch (const bf::filesystem_error& e) {
                            return false;
                        }*/

                        if (!useRelativePath) {
                            ldLog() << LD_ERROR << "Not implemented" << std::endl;
                            return false;
                        }

                        subprocess::Popen proc({"ln", "-f", "-s", "--relative", target.c_str(), symlink.c_str()},
                            subprocess::output(subprocess::PIPE),
                            subprocess::error(subprocess::PIPE)
                        );

                        auto outputs = util::subprocess::check_output_error(proc);

                        if (proc.retcode() != 0) {
                            ldLog() << LD_ERROR << "ln subprocess failed:" << std::endl
                                    << outputs.first << std::endl << outputs.second << std::endl;
                            return false;
                        }

                        return true;
                    }

                    bool hasBeenVisitedAlready(const bf::path& path) {
                        return visitedFiles.find(path) != visitedFiles.end();
                    }

                    // execute deferred copy operations registered with the deploy* functions
                    bool executeDeferredOperations() {
                        bool success = true;

                        while (!copyOperations.empty()) {
                            const auto& pair = *(copyOperations.begin());
                            const auto& from = pair.first;
                            const auto& to = pair.second;

                            if (!copyFile(from, to))
                                success = false;

                            copyOperations.erase(copyOperations.begin());
                        }

                        if (!success)
                            return false;

                        if (getenv("NO_STRIP") != nullptr) {
                            ldLog() << LD_WARNING << "$NO_STRIP environment variable detected, not stripping binaries" << std::endl;
                            stripOperations.clear();
                        } else {
                            const auto stripPath = getStripPath();

                            while (!stripOperations.empty()) {
                                const auto& filePath = *(stripOperations.begin());

                                if (util::stringStartsWith(elf::ElfFile(filePath).getRPath(), "$")) {
                                    ldLog() << LD_WARNING << "Not calling strip on binary" << filePath << LD_NO_SPACE
                                            << ": rpath starts with $" << std::endl;
                                } else {
                                    ldLog() << "Calling strip on library" << filePath << std::endl;

                                    std::map<std::string, std::string> env;
                                    env.insert(std::make_pair(std::string("LC_ALL"), std::string("C")));

                                    subprocess::Popen proc(
                                        {stripPath.c_str(), filePath.c_str()},
                                        subprocess::output(subprocess::PIPE),
                                        subprocess::error(subprocess::PIPE),
                                        subprocess::environment(env)
                                    );

                                    std::string err = util::subprocess::check_output_error(proc).second;

                                    if (proc.retcode() != 0 &&
                                        !util::stringContains(err, "Not enough room for program headers")) {
                                        ldLog() << LD_ERROR << "Strip call failed:" << err << std::endl;
                                        success = false;
                                    }
                                }

                                stripOperations.erase(stripOperations.begin());
                            }
                        }

                        if (!success)
                            return false;

                        while (!setElfRPathOperations.empty()) {
                            const auto& currentEntry = *(setElfRPathOperations.begin());
                            const auto& filePath = currentEntry.first;
                            const auto& rpath = currentEntry.second;

                            ldLog() << "Setting rpath in ELF file" << filePath << "to" << rpath << std::endl;
                            if (!elf::ElfFile(filePath).setRPath(rpath)) {
                                ldLog() << LD_ERROR << "Failed to set rpath in ELF file:" << filePath << std::endl;
                                success = false;
                            }

                            setElfRPathOperations.erase(setElfRPathOperations.begin());
                        }

                        return true;
                    }

                    // search for copyright file for file and deploy it to AppDir
                    bool deployCopyrightFiles(const bf::path& from, const std::string& logPrefix = "") {
                        ldLog() << logPrefix << LD_NO_SPACE << "Deploying copyright files for file" << from << std::endl;

                        if (copyrightFilesManager == nullptr)
                            return false;

                        auto copyrightFiles = copyrightFilesManager->getCopyrightFilesForPath(from);

                        if (copyrightFiles.empty())
                            return false;

                        for (const auto& file : copyrightFiles) {
                            std::string targetDir = file.string();
                            targetDir.erase(0, 1);
                            deployFile(file, appDirPath / targetDir);
                        }

                        return true;
                    }

                    // register copy operation that will be executed later
                    // by compiling a list of files to copy instead of just copying everything, one can ensure that
                    // the files are touched once only
                    void deployFile(const bf::path& from, bf::path to, bool verbose = false) {
                        if (verbose)
                            ldLog() << "Deploying file" << from << "to" << to << std::endl;

                        // not sure whether this is 100% bullet proof, but it simulates the cp command behavior
                        if (to.string().back() == '/' || bf::is_directory(to)) {
                            to /= from.filename();
                        }

                        copyOperations[from] = to;

                        // mark file as visited
                        visitedFiles.insert(from);
                    }

                    std::string getLogPrefix(int recursionLevel) {
                        std::string logPrefix;
                        for (int i = 0; i < recursionLevel; i++)
                            logPrefix += "  ";
                        return logPrefix;
                    }

                    bool deployElfDependencies(const bf::path& path, int recursionLevel = 0) {
                        auto logPrefix = getLogPrefix(recursionLevel);

                        ldLog() << logPrefix << LD_NO_SPACE << "Deploying dependencies for ELF file" << path << std::endl;
                        try {
                            for (const auto &dependencyPath : elf::ElfFile(path).traceDynamicDependencies()) {
                                if (!deployLibrary(dependencyPath, recursionLevel + 1))
                                    return false;
                            }
                        } catch (const elf::DependencyNotFoundError& e) {
                            ldLog() << LD_ERROR << e.what() << std::endl;
                            return false;
                        }

                        return true;
                    }

                    static std::string getStripPath() {
                        // by default, try to use a strip next to the linuxdeploy binary
                        // if that isn't available, fall back to searching for strip in the PATH
                        std::string stripPath = "strip";

                        auto binDirPath = bf::path(util::getOwnExecutablePath()).parent_path();
                        auto localStripPath = binDirPath / "strip";

                        if (bf::exists(localStripPath))
                            stripPath = localStripPath.string();

                        ldLog() << LD_DEBUG << "Using strip:" << stripPath << std::endl;

                        return stripPath;
                    }

                    bool deployLibrary(const bf::path& path, int recursionLevel = 0, bool forceDeploy = false,const bf::path &destination = bf::path()) {
                        auto logPrefix = getLogPrefix(recursionLevel);

                        if (!forceDeploy && hasBeenVisitedAlready(path)) {
                            ldLog() << LD_DEBUG << logPrefix << LD_NO_SPACE << "File has been visited already:" << path << std::endl;
                            return true;
                        }


                        static auto isInExcludelist = [&logPrefix](const bf::path& fileName) {
                            for (const auto& excludePattern : generatedExcludelist) {
                                // simple string match is faster than using fnmatch
                                if (excludePattern == fileName)
                                    return true;

                                auto fnmatchResult = fnmatch(excludePattern.c_str(), fileName.string().c_str(), FNM_PATHNAME);
                                switch (fnmatchResult) {
                                    case 0:
                                        return true;
                                    case FNM_NOMATCH:
                                        break;
                                    default:
                                        ldLog() << LD_ERROR << logPrefix << LD_NO_SPACE << "fnmatch() reported error:" << fnmatchResult << std::endl;
                                        return false;
                                }
                            }

                            return false;
                        };

                        if (!forceDeploy && isInExcludelist(path.filename())) {
                            ldLog() << logPrefix << LD_NO_SPACE << "Skipping deployment of blacklisted library" << path << std::endl;

                            // mark file as visited
                            visitedFiles.insert(path);

                            return true;
                        }

                        // note for self: make sure to have a trailing slash in libraryDir, otherwise copyFile won't
                        // create a directory
                        bf::path libraryDir = appDirPath / "usr" / (getLibraryDirName(path) + "/");

                        ldLog() << logPrefix << LD_NO_SPACE << "Deploying shared library" << path;
                        if (!destination.empty())
                            ldLog() << " (destination:" << destination << LD_NO_SPACE << ")";
                        ldLog() << std::endl;

                        auto destinationPath = destination.empty() ? libraryDir : destination;

                        // not sure whether this is 100% bullet proof, but it simulates the cp command behavior
                        if (destinationPath.string().back() == '/' || bf::is_directory(destinationPath)) {
                            destinationPath /= path.filename();
                        }

                        deployFile(path, destinationPath);
                        deployCopyrightFiles(path, logPrefix);

                        std::string rpath = "$ORIGIN";

                        if (!destination.empty()) {
                            std::string rpathDestination = destination.string();

                            if (destination.string().back() == '/') {
                                rpathDestination = destination.string();

                                while (rpathDestination.back() == '/')
                                    rpathDestination.erase(rpathDestination.end() - 1, rpathDestination.end());
                            } else {
                                rpathDestination = destination.parent_path().string();
                            }

                            auto relPath = bf::relative(bf::absolute(libraryDir), bf::absolute(rpathDestination));
                            rpath = "$ORIGIN/" + relPath.string() + ":$ORIGIN";
                        }


                        setElfRPathOperations[destinationPath] = rpath;
                        stripOperations.insert(destinationPath);

                        if (!deployElfDependencies(path, recursionLevel))
                            return false;

                        return true;
                    }

                    bool deployExecutable(const bf::path& path, const boost::filesystem::path& destination) {
                        if (hasBeenVisitedAlready(path)) {
                            ldLog() << LD_DEBUG << "File has been visited already:" << path << std::endl;
                            return true;
                        }

                        ldLog() << "Deploying executable" << path << std::endl;

                        // FIXME: make executables executable

                        auto destinationPath = destination.empty() ? appDirPath / "usr/bin/" : destination;

                        deployFile(path, destinationPath);
                        deployCopyrightFiles(path);

                        std::string rpath = "$ORIGIN/../" + getLibraryDirName(path);

                        if (!destination.empty()) {
                            std::string rpathDestination = destination.string();

                            if (destination.string().back() == '/') {
                                rpathDestination = destination.string();

                                while (rpathDestination.back() == '/')
                                    rpathDestination.erase(rpathDestination.end() - 1, rpathDestination.end());
                            } else {
                                rpathDestination = destination.parent_path().string();
                            }

                            auto relPath = bf::relative(bf::absolute(appDirPath) / "usr" / getLibraryDirName(path), bf::absolute(rpathDestination));
                            rpath = "$ORIGIN/" + relPath.string();
                        }

                        setElfRPathOperations[destinationPath / path.filename()] = rpath;
                        stripOperations.insert(destinationPath / path.filename());

                        if (!deployElfDependencies(path))
                            return false;

                        return true;
                    }

                    bool deployDesktopFile(const desktopfile::DesktopFile& desktopFile) {
                        if (hasBeenVisitedAlready(desktopFile.path())) {
                            ldLog() << LD_DEBUG << "File has been visited already:" << desktopFile.path() << std::endl;
                            return true;
                        }

                        if (!desktopFile.validate()) {
                            ldLog() << LD_ERROR << "Failed to validate desktop file:" << desktopFile.path() << std::endl;
                        }

                        ldLog() << "Deploying desktop file" << desktopFile.path() << std::endl;

                        deployFile(desktopFile.path(), appDirPath / "usr/share/applications/");

                        return true;
                    }

                    bool deployIcon(const bf::path& path) {
                        if (hasBeenVisitedAlready(path)) {
                            ldLog() << LD_DEBUG << "File has been visited already:" << path << std::endl;
                            return true;
                        }

                        ldLog() << "Deploying icon" << path << std::endl;

                        std::string resolution;

                        // if file is a vector image, use "scalable" directory
                        if (util::strLower(path.filename().extension().string()) == ".svg") {
                            resolution = "scalable";
                        } else {
                            try {
                                CImg<unsigned char> image(path.c_str());

                                auto xRes = image.width();
                                auto yRes = image.height();

                                if (xRes != yRes) {
                                    ldLog() << LD_WARNING << "x and y resolution of icon are not equal:" << path;
                                }

                                resolution = std::to_string(xRes) + "x" + std::to_string(yRes);

                                // otherwise, test resolution against "known good" values, and reject invalid ones
                                const auto knownResolutions = {8, 16, 20, 22, 24, 32, 48, 64, 72, 96, 128, 192, 256, 512};

                                // assume invalid
                                bool invalidXRes = true, invalidYRes = true;

                                for (const auto res : knownResolutions) {
                                    if (xRes == res)
                                        invalidXRes = false;
                                    if (yRes == res)
                                        invalidYRes = false;
                                }

                                if (invalidXRes) {
                                    ldLog() << LD_ERROR << "Icon" << path << "has invalid x resolution:" << xRes;
                                    return false;
                                }

                                if (invalidYRes) {
                                    ldLog() << LD_ERROR << "Icon" << path << "has invalid x resolution:" << xRes;
                                    return false;
                                }
                            } catch (const CImgException& e) {
                                ldLog() << LD_ERROR << "CImg error: " << e.what() << std::endl;
                                return false;
                            }
                        }

                        // rename files like <appname>_*.ext to <appname>.ext
                        auto filename = path.filename().string();
                        if (!appName.empty() && util::stringStartsWith(path.string(), appName)) {
                            auto newFilename = appName + path.extension().string();
                            if (newFilename != filename) {
                                ldLog() << LD_WARNING << "Renaming icon" << path << "to" << newFilename << std::endl;
                                filename = newFilename;
                            }
                        }

                        deployFile(path, appDirPath / "usr/share/icons/hicolor" / resolution / "apps" / filename);
                        deployCopyrightFiles(path);

                        return true;
                    }
            };

            AppDir::AppDir(const bf::path& path) {
                d = new PrivateData();

                d->appDirPath = path;
            }

            AppDir::~AppDir() {
                delete d;
            }

            AppDir::AppDir(const std::string& path) : AppDir(bf::path(path)) {}

            bool AppDir::createBasicStructure() {
                std::vector<std::string> dirPaths = {
                    "usr/bin/",
                    "usr/lib/",
                    "usr/share/applications/",
                    "usr/share/icons/hicolor/",
                };

                for (const std::string& resolution : {"16x16", "32x32", "64x64", "128x128", "256x256", "scalable"}) {
                    auto iconPath = "usr/share/icons/hicolor/" + resolution + "/apps/";
                    dirPaths.push_back(iconPath);
                }

                for (const auto& dirPath : dirPaths) {
                    auto fullDirPath = d->appDirPath / dirPath;

                    ldLog() << "Creating directory" << fullDirPath << std::endl;

                    // skip directory if it exists
                    if (bf::is_directory(fullDirPath))
                        continue;

                    try {
                        bf::create_directories(fullDirPath);
                    } catch (const bf::filesystem_error&) {
                        ldLog() << LD_ERROR << "Failed to create directory" << fullDirPath;
                        return false;
                    }
                }

                return true;
            }

            bool AppDir::deployLibrary(const bf::path& path, const bf::path& destination) {
                return d->deployLibrary(path, 0, false, destination);
            }

            bool AppDir::forceDeployLibrary(const bf::path& path, const bf::path& destination) {
                return d->deployLibrary(path, 0, true, destination);
            }

            bool AppDir::deployExecutable(const bf::path& path, const boost::filesystem::path& destination) {
                return d->deployExecutable(path, destination);
            }

            bool AppDir::deployDesktopFile(const desktopfile::DesktopFile& desktopFile) {
                return d->deployDesktopFile(desktopFile);
            }
 
            bool AppDir::deployIcon(const bf::path& path) {
                return d->deployIcon(path);
            }

            bool AppDir::executeDeferredOperations() {
                return d->executeDeferredOperations();
            }

            boost::filesystem::path AppDir::path() {
                return d->appDirPath;
            }

            static std::vector<bf::path> listFilesInDirectory(const bf::path& path, const bool recursive = true) {
                std::vector<bf::path> foundPaths;

                // directory_iterators throw exceptions if the directory doesn't exist
                if (!bf::is_directory(path)) {
                    ldLog() << LD_DEBUG << "No such directory:" << path << std::endl;
                    return {};
                }

                if (recursive) {
                    for (bf::recursive_directory_iterator i(path); i != bf::recursive_directory_iterator(); ++i) {
                        if (bf::is_regular_file(*i)) {
                            foundPaths.push_back((*i).path());
                        }
                    }
                } else {
                    for (bf::directory_iterator i(path); i != bf::directory_iterator(); ++i) {
                        if (bf::is_regular_file(*i)) {
                            foundPaths.push_back((*i).path());
                        }
                    }
                }

                return foundPaths;
            }

            std::vector<bf::path> AppDir::deployedIconPaths() {
                auto icons = listFilesInDirectory(path() / "usr/share/icons/");
                auto pixmaps = listFilesInDirectory(path() / "usr/share/pixmaps/", false);
                icons.reserve(pixmaps.size());
                std::copy(pixmaps.begin(), pixmaps.end(), std::back_inserter(icons));
                return icons;
            }

            std::vector<bf::path> AppDir::deployedExecutablePaths() {
                return listFilesInDirectory(path() / "usr/bin/", false);
            }

            std::vector<desktopfile::DesktopFile> AppDir::deployedDesktopFiles() {
                std::vector<desktopfile::DesktopFile> desktopFiles;

                auto paths = listFilesInDirectory(path() / "usr/share/applications/", false);
                paths.erase(std::remove_if(paths.begin(), paths.end(), [](const bf::path& path) {
                    return path.extension() != ".desktop";
                }), paths.end());

                for (const auto& path : paths) {
                    desktopFiles.push_back(desktopfile::DesktopFile(path));
                }

                return desktopFiles;
            }

            bool AppDir::createLinksInAppDirRoot(const desktopfile::DesktopFile& desktopFile, boost::filesystem::path customAppRunPath) {
                ldLog() << "Deploying desktop file to AppDir root:" << desktopFile.path() << std::endl;

                // copy desktop file to root directory
                if (!d->symlinkFile(desktopFile.path(), path())) {
                    ldLog() << LD_ERROR << "Failed to create link to desktop file in AppDir root:" << desktopFile.path() << std::endl;
                    return false;
                }

                // look for suitable icon
                std::string iconName;

                if (!desktopFile.getEntry("Desktop Entry", "Icon", iconName)) {
                    ldLog() << LD_ERROR << "Icon entry missing in desktop file:" << desktopFile.path() << std::endl;
                    return false;
                }

                bool iconDeployed = false;

                const auto foundIconPaths = deployedIconPaths();

                if (foundIconPaths.empty()) {
                    ldLog() << LD_ERROR << "Could not find icon executable for Icon entry:" << iconName << std::endl;
                    return false;
                }

                for (const auto& iconPath : foundIconPaths) {
                    ldLog() << LD_DEBUG << "Icon found:" << iconPath << std::endl;

                    const bool matchesFilenameWithExtension = iconPath.filename() == iconName;

                    if (iconPath.stem() == iconName || matchesFilenameWithExtension) {
                        if (matchesFilenameWithExtension) {
                            ldLog() << LD_WARNING << "Icon= entry filename contains extension" << std::endl;
                        }

                        ldLog() << "Deploying icon to AppDir root:" << iconPath << std::endl;

                        if (!d->symlinkFile(iconPath, path())) {
                            ldLog() << LD_ERROR << "Failed to create symlink for icon in AppDir root:" << iconPath << std::endl;
                            return false;
                        }

                        iconDeployed = true;
                        break;
                    }
                }

                if (!iconDeployed) {
                    ldLog() << LD_ERROR << "Could not find suitable icon for Icon entry:" << iconName << std::endl;
                    return false;
                }

                if (!customAppRunPath.empty()) {
                    // copy custom AppRun executable
                    // FIXME: make sure this file is executable
                    ldLog() << "Deploying custom AppRun:" << customAppRunPath;

                    if (!d->copyFile(customAppRunPath, path() / "AppRun"))
                        return false;
                } else {
                    // check if there is a custom AppRun already
                    // in that case, skip deployment of symlink
                    if (bf::exists(path() / "AppRun")) {
                        ldLog() << LD_WARNING << "Custom AppRun detected, skipping deployment of symlink" << std::endl;
                    } else {
                        // look for suitable binary to create AppRun symlink
                        std::string executableName;

                        if (!desktopFile.getEntry("Desktop Entry", "Exec", executableName)) {
                            ldLog() << LD_ERROR << "Exec entry missing in desktop file:" << desktopFile.path()
                                    << std::endl;
                            return false;
                        }

                        executableName = util::split(executableName)[0];

                        const auto foundExecutablePaths = deployedExecutablePaths();

                        if (foundExecutablePaths.empty()) {
                            ldLog() << LD_ERROR << "Could not find suitable executable for Exec entry:" << executableName
                                    << std::endl;
                            return false;
                        }

                        bool deployedExecutable = false;

                        for (const auto& executablePath : foundExecutablePaths) {
                            ldLog() << LD_DEBUG << "Executable found:" << executablePath << std::endl;

                            if (executablePath.filename() == executableName) {
                                ldLog() << "Deploying AppRun symlink for executable in AppDir root:" << executablePath
                                        << std::endl;

                                if (!d->symlinkFile(executablePath, path() / "AppRun")) {
                                    ldLog() << LD_ERROR
                                            << "Failed to create AppRun symlink for executable in AppDir root:"
                                            << executablePath << std::endl;
                                    return false;
                                }

                                deployedExecutable = true;
                                break;
                            }
                        }

                        if (!deployedExecutable) {
                            ldLog() << LD_ERROR << "Could not deploy symlink for executable: could not find suitable executable for Exec entry:" << executableName << std::endl;
                            return false;
                        }
                    }
                }

                return true;
            }

            void AppDir::deployFile(const boost::filesystem::path& from, const boost::filesystem::path& to) {
                return d->deployFile(from, to, true);
            }

            bool AppDir::createRelativeSymlink(const bf::path& target, const bf::path& symlink) {
                d->symlinkFile(target, symlink, true);
            }

            std::vector<bf::path> AppDir::listExecutables() {
                std::vector<bf::path> executables;

                for (const auto& file : listFilesInDirectory(path() / "usr" / "bin", false)) {
                    // make sure it's an ELF file
                    try {
                        elf::ElfFile elfFile(file);
                    } catch (const elf::ElfFileParseError&) {
                        // FIXME: remove this workaround once the MIME check below works as intended
                        continue;
                    }

                    executables.push_back(file);
                }

                return executables;
            }

            std::vector<bf::path> AppDir::listSharedLibraries() {
                std::vector<bf::path> sharedLibraries;

                for (const auto& file : listFilesInDirectory(path() / "usr" / "lib", true)) {
                    // make sure it's an ELF file
                    try {
                        elf::ElfFile elfFile(file);
                    } catch (const elf::ElfFileParseError&) {
                        // FIXME: remove this workaround once the MIME check below works as intended
                        continue;
                    }

                    sharedLibraries.push_back(file);
                }

                return sharedLibraries;
            }

            bool AppDir::deployDependenciesForExistingFiles() {
                for (const auto& executable : listExecutables()) {
                    if (bf::is_symlink(executable))
                        continue;

                    if (!d->deployElfDependencies(executable))
                        return false;

                    std::string rpath = "$ORIGIN/../" + PrivateData::getLibraryDirName(executable);

                    d->setElfRPathOperations[executable] = rpath;
                }

                for (const auto& sharedLibrary : listSharedLibraries()) {
                    if (bf::is_symlink(sharedLibrary))
                        continue;

                    if (!d->deployElfDependencies(sharedLibrary))
                        return false;

                    d->setElfRPathOperations[sharedLibrary] = "$ORIGIN";
                }

                return true;
            }
        }
    }
}
