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
            static constexpr auto APPRUN_HOOKS_DIRNAME = "apprun-hooks";

        public:
            const AppDir& appDir;

        public:
            explicit Private(const AppDir& appDir) : appDir(appDir) {}

        public:
            static void makeFileExecutable(const bf::path& path) {
                bf::permissions(path,
                    bf::perms::owner_all | bf::perms::group_read | bf::perms::others_read |
                    bf::perms::group_exe | bf::perms::others_exe
                );
            }

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

                return true;
            }

            bool deployCustomAppRunFile(const bf::path& customAppRunPath) const {
                // copy custom AppRun executable
                ldLog() << "Deploying custom AppRun:" << customAppRunPath << std::endl;

                const auto appRunPath = appDir.path() / "AppRun";

                if (!appDir.copyFile(customAppRunPath, appRunPath))
                    return false;

                ldLog() << "Making AppRun file executable: " << appRunPath << std::endl;
                makeFileExecutable(appRunPath);

                return true;
            }

            bool deployStandardAppRunFromDesktopFile(const DesktopFile& desktopFile, const bf::path& customAppRunPath) const {
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

                return true;
            }

            bool deployAppRunWrapperIfNecessary() const {
                const bf::path appRunPath(appDir.path() / "AppRun");
                const bf::path wrappedAppRunPath(appRunPath.string() + ".wrapped");

                const bf::path appRunHooksPath(appDir.path() / APPRUN_HOOKS_DIRNAME);

                // first, we check whether there's that special directory containing hooks
                if (!bf::is_directory(appRunHooksPath)) {
                    ldLog() << LD_DEBUG << "Could not find apprun-hooks dir, no need to deploy the AppRun wrapper" << std::endl;
                    return true;
                }

                // if there's no files in there we don't have to do anything
                bf::directory_iterator firstRegularFile = std::find_if(
                    bf::directory_iterator(appRunHooksPath),
                    bf::directory_iterator{},
                    [](const bf::path& p) {
                        return bf::is_regular_file(p);
                    }
                );
                if (firstRegularFile == bf::directory_iterator{}) {
                    ldLog() << LD_WARNING << "Found an empty apprun-hooks directory, assuming there is no need to deploy the AppRun wrapper" << std::endl;
                    return true;
                }

                // any file within that directory is considered to be a script
                // we can't perform any validity checks, that would be way too much complexity and even tools which
                // claim they can, like e.g., shellcheck, aren't perfect, they only aid in avoiding bugs but cannot
                // prevent them completely

                // let's put together the wrapper script's contents
                std::ostringstream oss;

                oss << "#! /usr/bin/env bash" << std::endl
                    << std::endl
                    << "# autogenerated by linuxdeploy" << std::endl
                    << std::endl
                    << "# make sure errors in sourced scripts will cause this script to stop" << std::endl
                    << "set -e" << std::endl
                    << std::endl
                    << "this_dir=$(readlink -f $(dirname \"$0\"))" << std::endl
                    << std::endl;

                std::for_each(bf::directory_iterator(appRunHooksPath), bf::directory_iterator{}, [&oss](const bf::path& p) {
                    if (!bf::is_regular_file(p))
                        return;

                    oss << "source \"$this_dir\"/" << APPRUN_HOOKS_DIRNAME << "/" << p.filename() << std::endl;
                });

                oss << std::endl
                    << "exec \"$this_dir\"/AppRun.wrapped \"$@\"" << std::endl;

                // first we need to make sure we're not running this more than once
                // this might cause more harm than good
                // we require the user to clean up the mess at first
                // FIXME: try to find a way how to rewrap AppRun on subsequent runs or, even better, become idempotent
                if (bf::exists(wrappedAppRunPath)) {
                    ldLog() << LD_WARNING << "Already found wrapped AppRun, using existing file/symlink" << std::endl;
                } else {
                    // backup original AppRun
                    bf::rename(appRunPath, wrappedAppRunPath);
                }

                // in case the above check triggered a warning, it's possible that there is another AppRun in the AppDir
                // this one has to be cleaned up in that case
                if (bf::exists(appRunPath)) {
                    ldLog() << LD_WARNING << "Found an AppRun file/symlink, possibly due to re-run of linuxdeploy, "
                                            "overwriting" << std::endl;
                    bf::remove(appRunPath);
                }


                // install new script
                std::ofstream ofs(appRunPath.string());
                ofs << oss.str();

                // make sure data is written to disk
                ofs.flush();
                ofs.close();

                // make new file executable
                makeFileExecutable(appRunPath);

                // we're done!
                return true;
            }
        };

        AppDirRootSetup::AppDirRootSetup(const AppDir& appDir) : d(new Private(appDir)) {}

        bool AppDirRootSetup::run(const DesktopFile& desktopFile, const bf::path& customAppRunPath) const {
            // first step that is always required is to deploy the desktop file and the corresponding icon
            if (!d->deployDesktopFileAndIcon(desktopFile)) {
                ldLog() << LD_DEBUG << "deployDesktopFileAndIcon returned false" << std::endl;
                return false;
            }

            // the algorithm depends on whether the user wishes to deploy their own AppRun file
            // in case they do, the algorithm shall deploy that file
            // otherwise, the standard algorithm shall be run which takes information from the desktop file to
            // deploy a symlink pointing to the AppImage's main binary
            // this allows power users to define their own AppImage initialization steps or run different binaries
            // based on parameters etc.
            if (!customAppRunPath.empty()) {
                if (!d->deployCustomAppRunFile(customAppRunPath)) {
                    ldLog() << LD_DEBUG << "deployCustomAppRunFile returned false" << std::endl;
                    return false;
                }
            } else {
                if (!d->deployStandardAppRunFromDesktopFile(desktopFile, customAppRunPath)) {
                    ldLog() << LD_DEBUG << "deployStandardAppRunFromDesktopFile returned false" << std::endl;
                    return false;
                }
            }

            // plugins might need to run some initializing code to make certain features work
            // these involve setting environment variables because libraries or frameworks don't support any other
            // way of pointing them to resources inside the AppDir instead of looking into config files in locations
            // inside the AppImage, etc.
            // the linuxdeploy plugin specification states that if plugins put files into a specified directory in
            // the AppImage, linuxdeploy will make sure they're run before running the regular AppRun
            if (!d->deployAppRunWrapperIfNecessary()) {
                ldLog() << LD_DEBUG << "deployAppRunWrapperIfNecessary returned false" << std::endl;
                return false;
            }

            return true;
        }
    }
}



