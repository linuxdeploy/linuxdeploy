// local headers
#include <linuxdeploy/util/util.h>
#include <linuxdeploy/core/log.h>
#include "appdir_root_setup.h"


namespace bf = boost::filesystem;

namespace linuxdeploy {
    using namespace desktopfile;

    namespace core {
        using namespace appdir;
        using namespace log;


        class AppDirRootSetup::Private {
        public:
            const AppDir& appDir;

        public:
            explicit Private(const AppDir& appDir) : appDir(appDir) {}

        public:
            bool deployDesktopFileAndIcon(const DesktopFile& desktopFile) const {
                ldLog() << "Deploying desktop file to AppDir root:" << desktopFile.path() << std::endl;

                // copy desktop file to root directory
                if (!appDir.createRelativeSymlink(desktopFile.path(), appDir.path())) {
                    ldLog() << LD_ERROR << "Failed to create link to desktop file in AppDir root:" << desktopFile.path() << std::endl;
                    return false;
                }

                // look for suitable icon
                DesktopFileEntry iconEntry;

                if (!desktopFile.getEntry("Desktop Entry", "Icon", iconEntry)) {
                    ldLog() << LD_ERROR << "Icon entry missing in desktop file:" << desktopFile.path() << std::endl;
                    return false;
                }

                bool iconDeployed = false;

                const auto foundIconPaths = appDir.deployedIconPaths();

                if (foundIconPaths.empty()) {
                    ldLog() << LD_ERROR << "Could not find icon executable for Icon entry:" << iconEntry.value() << std::endl;
                    return false;
                }

                for (const auto& iconPath : foundIconPaths) {
                    ldLog() << LD_DEBUG << "Icon found:" << iconPath << std::endl;

                    const bool matchesFilenameWithExtension = iconPath.filename() == iconEntry.value();

                    if (iconPath.stem() == iconEntry.value() || matchesFilenameWithExtension) {
                        if (matchesFilenameWithExtension) {
                            ldLog() << LD_WARNING << "Icon= entry filename contains extension" << std::endl;
                        }

                        ldLog() << "Deploying icon to AppDir root:" << iconPath << std::endl;

                        if (!appDir.createRelativeSymlink(iconPath, appDir.path())) {
                            ldLog() << LD_ERROR << "Failed to create symlink for icon in AppDir root:" << iconPath << std::endl;
                            return false;
                        }

                        iconDeployed = true;
                        break;
                    }
                }

                if (!iconDeployed) {
                    ldLog() << LD_ERROR << "Could not find suitable icon for Icon entry:" << iconEntry.value() << std::endl;
                    return false;
                }
            }

            bool deployCustomAppRunFile(const bf::path& customAppRunPath) {
                // copy custom AppRun executable
                // FIXME: make sure this file is executable
                ldLog() << "Deploying custom AppRun:" << customAppRunPath;

                if (!appDir.copyFile(customAppRunPath, appDir.path() / "AppRun"))
                    return false;
            }

            bool deployStandardAppRunFromDesktopFile(const DesktopFile& desktopFile, const bf::path& customAppRunPath) {
                // check if there is a custom AppRun already
                // in that case, skip deployment of symlink
                if (bf::exists(appDir.path() / "AppRun")) {
                    ldLog() << LD_WARNING << "Existing AppRun detected, skipping deployment of symlink" << std::endl;
                } else {
                    // look for suitable binary to create AppRun symlink
                    DesktopFileEntry executableEntry;

                    if (!desktopFile.getEntry("Desktop Entry", "Exec", executableEntry)) {
                        ldLog() << LD_ERROR << "Exec entry missing in desktop file:" << desktopFile.path()
                                << std::endl;
                        return false;
                    }

                    auto executableName = util::split(executableEntry.value())[0];

                    const auto foundExecutablePaths = appDir.deployedExecutablePaths();

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

                            if (!appDir.createRelativeSymlink(executablePath, appDir.path() / "AppRun")) {
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
        };

        AppDirRootSetup::AppDirRootSetup(const AppDir& appDir) : d(new Private(appDir)) {}

        bool AppDirRootSetup::run(const DesktopFile& desktopFile, const bf::path& customAppRunPath) const {
            // first step that is always required is to deploy the desktop file and the corresponding icon
            if (!d->deployDesktopFileAndIcon(desktopFile))
                return false;

            // the algorithm depends on whether the user wishes to deploy their own AppRun file
            // in case they do, the algorithm shall deploy that file
            // otherwise, the standard algorithm shall be run which takes information from the desktop file to
            // deploy a symlink pointing to the AppImage's main binary
            // this allows power users to define their own AppImage initialization steps or run different binaries
            // based on parameters etc.
            if (!customAppRunPath.empty()) {
                if (!d->deployCustomAppRunFile(customAppRunPath))
                    return false;
            } else {
                if (!d->deployStandardAppRunFromDesktopFile(desktopFile, customAppRunPath))
                    return false;
            }

            return true;
        }
    }
}



