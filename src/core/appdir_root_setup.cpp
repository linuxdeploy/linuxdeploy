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
        };

        AppDirRootSetup::AppDirRootSetup(const AppDir& appDir) : d(new Private(appDir)) {}

        bool AppDirRootSetup::run(const DesktopFile& desktopFile, const bf::path& customAppRunPath) const {
            ldLog() << "Deploying desktop file to AppDir root:" << desktopFile.path() << std::endl;

            // copy desktop file to root directory
            if (!d->appDir.createRelativeSymlink(desktopFile.path(), d->appDir.path())) {
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

            const auto foundIconPaths = d->appDir.deployedIconPaths();

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

                    if (!d->appDir.createRelativeSymlink(iconPath, d->appDir.path())) {
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

            if (!customAppRunPath.empty()) {
                // copy custom AppRun executable
                // FIXME: make sure this file is executable
                ldLog() << "Deploying custom AppRun:" << customAppRunPath;

                if (!d->appDir.copyFile(customAppRunPath, d->appDir.path() / "AppRun"))
                    return false;
            } else {
                // check if there is a custom AppRun already
                // in that case, skip deployment of symlink
                if (bf::exists(d->appDir.path() / "AppRun")) {
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

                    const auto foundExecutablePaths = d->appDir.deployedExecutablePaths();

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

                            if (!d->appDir.createRelativeSymlink(executablePath, d->appDir.path() / "AppRun")) {
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
    }
}



