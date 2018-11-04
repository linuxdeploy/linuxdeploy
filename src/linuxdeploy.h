#pragma once

#include <iostream>
#include <boost/filesystem/path.hpp>

#include <linuxdeploy/core/appdir.h>
#include <linuxdeploy/core/log.h>
#include <linuxdeploy/util/util.h>
#include <args.hxx>
namespace linuxdeploy {
    int deployAppDirRootFiles(args::ValueFlagList<std::string>& desktopFilePaths,
                              args::ValueFlag<std::string>& customAppRunPath,
                              linuxdeploy::core::appdir::AppDir& appDir);
}
