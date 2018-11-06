#include <iostream>
#include <boost/filesystem/path.hpp>

#include <linuxdeploy/core/appdir.h>
#include <linuxdeploy/core/log.h>

#include "core.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;
namespace bf = boost::filesystem;

namespace linuxdeploy {
    /**
     * Resolved the 'MAIN' desktop file from all the available.
     *
     * @param desktopFilePaths
     * @param deployedDesktopFiles
     * @return the MAIN DesktopFile
     * @throw std::runtime_error in case of error
     */
    desktopfile::DesktopFile getMainDesktopFile(const std::vector<std::string>& desktopFilePaths,
                                                const std::vector<desktopfile::DesktopFile>& deployedDesktopFiles) {
        desktopfile::DesktopFile desktopFile;

        if (deployedDesktopFiles.empty()) {
            ldLog() << LD_WARNING
                    << "Could not find desktop file in AppDir, cannot create links for AppRun, desktop file and icon in AppDir root"
                    << std::endl;
            throw std::runtime_error("Can't find desktop files in AppDir.");
        } else {
            if (!desktopFilePaths.empty()) {
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
                    desktopFile = *desktopFileMatchingName;
                } else {
                    ldLog() << LD_ERROR << "Could not find desktop file deployed earlier any more:"
                            << firstDeployedDesktopFileName << std::endl;
                    throw std::runtime_error("Old desktop file is not reachable.");
                }
            } else {
                desktopFile = deployedDesktopFiles[0];
                ldLog() << LD_WARNING << "No desktop file specified, using first desktop file found:"
                        << desktopFile.path() << std::endl;
            }

        }
        return desktopFile;
    }

    bool deployAppDirRootFiles(std::vector<std::string> desktopFilePaths,
                               std::string customAppRunPath, appdir::AppDir& appDir) {
        // search for desktop file and deploy it to AppDir root
        ldLog() << std::endl << "-- Deploying files into AppDir root directory --" << std::endl;

        auto deployedDesktopFiles = appDir.deployedDesktopFiles();

        try {
            desktopfile::DesktopFile desktopFile = getMainDesktopFile(desktopFilePaths, deployedDesktopFiles);
            ldLog() << "Deploying desktop file:" << desktopFile.path() << std::endl;
            if (!appDir.createLinksInAppDirRoot(desktopFile, customAppRunPath))
                return false;

        } catch (const std::runtime_error& er) {
            return false;
        }

        return true;
    }
}
