// system includes
#include <string>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "linuxdeploy/core/desktopfile.h"

#pragma once

namespace linuxdeploy {
    namespace core {
        namespace appdir {
            /*
             * Base class for AppDirs.
             */
            class AppDir {
                private:
                    // private data class pattern
                    class PrivateData;
                    PrivateData* d;

                public:
                    // default constructor
                    // construct AppDir from given path
                    // the directory will be created if it doesn't exist
                    explicit AppDir(const boost::filesystem::path& path);

                    ~AppDir();

                    // alternative constructor
                    // shortcut for using a normal string instead of a path
                    explicit AppDir(const std::string& path);

                    // creates basic directory structure of an AppDir in "FHS" mode
                    bool createBasicStructure();

                    // deploy shared library
                    //
                    // if destination is specified, the library is copied to this location, and the rpath is adjusted accordingly
                    // the dependencies are copied to the normal destination, though
                    bool deployLibrary(const boost::filesystem::path& path, const boost::filesystem::path& destination = "");

                    // deploy executable
                    bool deployExecutable(const boost::filesystem::path& path, const boost::filesystem::path& destination = "");

                    // deploy desktop file
                    bool deployDesktopFile(const desktopfile::DesktopFile& desktopFile);

                    // deploy icon
                    bool deployIcon(const boost::filesystem::path& path);

                    // execute deferred copy operations
                    bool executeDeferredOperations();

                    // return path to AppDir
                    boost::filesystem::path path();

                    // create a list of all icon paths in the AppDir
                    std::vector<boost::filesystem::path> deployedIconPaths();

                    // create a list of all executable paths in the AppDir
                    std::vector<boost::filesystem::path> deployedExecutablePaths();

                    // create a list of all desktop file paths in the AppDir
                    std::vector<desktopfile::DesktopFile> deployedDesktopFiles();

                    // create symlinks for AppRun, desktop file and icon in the AppDir root directory
                    bool createLinksInAppDirRoot(const desktopfile::DesktopFile& desktopFile, boost::filesystem::path customAppRunPath = "");

                    // set application name of primary "entry point" application
                    // icons and other resources will then automatically be renamed using this value in order to
                    // make the deployment easier by not requiring special filenames
                    // resources' filenames should be prefixed with this value (example: linuxdeploy_48x48.png)
                    void setAppName(const std::string& appName);

                    // list all executables in <AppDir>/usr/bin
                    // this function does not perform a recursive search, but only searches the bin directory
                    std::vector<boost::filesystem::path> listExecutables();

                    // list all shared libraries in <AppDir>/usr/lib
                    // this function recursively searches the entire lib directory for shared libraries
                    std::vector<boost::filesystem::path> listSharedLibraries();

                    // search for executables and libraries and deploy their dependencies
                    // calling this function can turn sure file trees created by make install commands into working
                    // AppDirs
                    bool deployDependenciesForExistingFiles();
            };
        }
    }
}
