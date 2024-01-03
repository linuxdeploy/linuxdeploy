// system headers
#include <algorithm>
#include <assert.h>
#include <filesystem>
#include <iostream>

// local headers
#include <linuxdeploy/core/appdir.h>
#include <linuxdeploy/log/log.h>
#include "core.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::log;
using namespace linuxdeploy::desktopfile;

namespace fs = std::filesystem;

namespace linuxdeploy {
    class DeployError : public std::runtime_error {
    public:
        explicit DeployError(const std::string& what) : std::runtime_error(what) {};
    };

    /**
     * Resolve the 'MAIN' desktop file from all the available.
     *
     * @param desktopFilePaths
     * @param deployedDesktopFiles
     * @return the MAIN DesktopFile
     * @throw DeployError in case of 'deployed desktop file not found'
     */
    desktopfile::DesktopFile getMainDesktopFile(const std::vector<std::string>& desktopFilePaths,
                                                const std::vector<desktopfile::DesktopFile>& deployedDesktopFiles) {
        if (desktopFilePaths.empty()) {
            ldLog() << LD_WARNING << "No desktop file specified, using first desktop file found:"
                    << deployedDesktopFiles[0].path() << std::endl;
            return deployedDesktopFiles[0];
        }

        auto firstDeployedDesktopFileName = fs::path(desktopFilePaths.front()).filename().string();

        auto desktopFileMatchingName = find_if(
            deployedDesktopFiles.begin(),
            deployedDesktopFiles.end(),
            [&firstDeployedDesktopFileName](const desktopfile::DesktopFile& desktopFile) {
                auto fileName = fs::path(desktopFile.path()).filename().string();
                return fileName == firstDeployedDesktopFileName;
            }
        );

        if (desktopFileMatchingName != deployedDesktopFiles.end()) {
            return  *desktopFileMatchingName;
        } else {
            ldLog() << LD_ERROR << "Could not find desktop file deployed earlier any more:"
                    << firstDeployedDesktopFileName << std::endl;
            throw DeployError("Old desktop file is not reachable.");
        }
    }

    bool deployAppDirRootFiles(std::vector<std::string> desktopFilePaths,
                               std::string customAppRunPath, appdir::AppDir& appDir) {
        ldLog() << std::endl << "-- Deploying files into AppDir root directory --" << std::endl;

        if (!customAppRunPath.empty()) {
            ldLog() << LD_INFO << "Deploying custom AppRun: " << customAppRunPath << std::endl;

            const auto& appRunPathInAppDir = appDir.path() / "AppRun";
            if (fs::exists(appRunPathInAppDir)) {
                ldLog() << LD_WARNING << "File exists, replacing with custom AppRun" << std::endl;
                fs::remove(appRunPathInAppDir);
            }

            appDir.deployFile(customAppRunPath, appDir.path() / "AppRun");
            appDir.executeDeferredOperations();
        }

        auto deployedDesktopFiles = appDir.deployedDesktopFiles();
        if (deployedDesktopFiles.empty()) {
            ldLog() << LD_WARNING << "Could not find desktop file in AppDir, cannot create links for AppRun, "
                                     "desktop file and icon in AppDir root" << std::endl;
            return true;
        }

        try {
            desktopfile::DesktopFile desktopFile = getMainDesktopFile(desktopFilePaths, deployedDesktopFiles);
            ldLog() << "Deploying files to AppDir root using desktop file:" << desktopFile.path() << std::endl;
            return appDir.setUpAppDirRoot(desktopFile, customAppRunPath);
        } catch (const DeployError& er) {
            return false;
        }
    }

    bool addDefaultKeys(DesktopFile& desktopFile, const std::string& executableFileName) {
        ldLog() << "Adding default values to desktop file:" << desktopFile.path() << std::endl;

        auto rv = true;

        auto setDefault = [&rv, &desktopFile](const std::string& section, const std::string& key, const std::string& value) {
            if (desktopFile.entryExists(section, key)) {
                DesktopFileEntry entry;

                // this should never return false
                auto entryExists = desktopFile.getEntry(section, key, entry);
                assert(entryExists);

                ldLog() << LD_WARNING << "Key exists, not modified:" << key << "(current value:" << entry.value() << LD_NO_SPACE << ")" << std::endl;
                rv = false;
            } else {
                auto entryOverwritten = desktopFile.setEntry(section, DesktopFileEntry(key, value));
                assert(!entryOverwritten);
            }
        };

        setDefault("Desktop Entry", "Name", executableFileName);
        setDefault("Desktop Entry", "Exec", executableFileName);
        setDefault("Desktop Entry", "Icon", executableFileName);
        setDefault("Desktop Entry", "Type", "Application");
        setDefault("Desktop Entry", "Categories", "Utility;");

        return rv;
    }
}
