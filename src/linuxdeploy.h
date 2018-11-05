#pragma once

#include <iostream>
#include <boost/filesystem/path.hpp>

#include "linuxdeploy/core/appdir.h"

namespace linuxdeploy {
    bool deployAppDirRootFiles(std::vector<std::string> desktopFilePaths, std::string customAppRunPath,
                               linuxdeploy::core::appdir::AppDir& appDir);
}
