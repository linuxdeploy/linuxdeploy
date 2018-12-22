#pragma once

#include <iostream>
#include <boost/filesystem/path.hpp>

#include "linuxdeploy/core/appdir.h"

namespace linuxdeploy {
    /**
     * Deploy the application ".desktop", icon, and runnable files in the AppDir root path. According to the
     * AppDir spec at: https://docs.appimage.org/reference/appdir.html
     *
     * @param desktopFilePaths to be deployed in the AppDir root
     * @param customAppRunPath AppRun to be used, if empty the application executable will be used instead
     * @param appDir
     * @return true on success otherwise false
     */
    bool deployAppDirRootFiles(std::vector<std::string> desktopFilePaths, std::string customAppRunPath,
                               linuxdeploy::core::appdir::AppDir& appDir);

    /**
     *
     * @param desktopFile
     * @param executableFileName
     * @return
     */
    bool addDefaultKeys(desktopfile::DesktopFile& desktopFile, const std::string& executableFileName);
}
