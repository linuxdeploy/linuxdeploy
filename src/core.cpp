#include <iostream>
#include <boost/filesystem/path.hpp>

#include <linuxdeploy/core/appdir.h>
#include <linuxdeploy/core/log.h>

#include "core.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;
namespace bf = boost::filesystem;

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

        auto firstDeployedDesktopFileName = boost::filesystem::path(
            desktopFilePaths.front()).filename().string();

        auto desktopFileMatchingName = find_if(
            deployedDesktopFiles.begin(),
            deployedDesktopFiles.end(),
            [&firstDeployedDesktopFileName](const desktopfile::DesktopFile& desktopFile) {
                auto fileName = desktopFile.path().filename().string();
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
            if (bf::exists(appRunPathInAppDir)) {
                ldLog() << LD_WARNING << "File exists, replacing with custom AppRun" << std::endl;
                bf::remove(appRunPathInAppDir);
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
            ldLog() << "Deploying desktop file:" << desktopFile.path() << std::endl;
            return appDir.createLinksInAppDirRoot(desktopFile, customAppRunPath);
        } catch (const DeployError& er) {
            return false;
        }
    }
}
