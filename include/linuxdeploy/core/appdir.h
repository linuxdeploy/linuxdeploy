// system includes
#include <memory>
#include <string>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "linuxdeploy/desktopfile/desktopfile.h"

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
                    std::shared_ptr<PrivateData> d;

                public:
                    // default constructor
                    // construct AppDir from given path
                    // the directory will be created if it doesn't exist
                    explicit AppDir(const boost::filesystem::path& path);

                    // we don't want any copy/move(-assignment) behavior, therefore we delete those operators/constructors
                    AppDir(const AppDir&) = delete;
                    AppDir(AppDir&&) = delete;
                    void operator=(const AppDir&) = delete;
                    void operator=(AppDir&&) = delete;

                    // alternative constructor
                    // shortcut for using a normal string instead of a path
                    explicit AppDir(const std::string& path);

                    // Set additional shared library name patterns to be excluded from deployment.
                    void setExcludeLibraryPatterns(const std::vector<std::string> &excludeLibraryPatterns);

                    // creates basic directory structure of an AppDir in "FHS" mode
                    bool createBasicStructure() const;

                    // deploy shared library
                    //
                    // if destination is specified, the library is copied to this location, and the rpath is adjusted accordingly
                    // the dependencies are copied to the normal destination, though
                    bool deployLibrary(const boost::filesystem::path& path, const boost::filesystem::path& destination = "");

                    // force deploy shared library
                    //
                    // works like deployLibrary, except that it doesn't check the excludelist
                    // this is useful to deploy libraries and their dependencies that are blacklisted and would otherwise not be deployed
                    // the excludelist check is only disabled for the current library, and will be enabled for the dependencies again
                    bool forceDeployLibrary(const boost::filesystem::path& path, const boost::filesystem::path& destination = "");

                    // deploy executable
                    bool deployExecutable(const boost::filesystem::path& path, const boost::filesystem::path& destination = "");

                    // deploy dependencies for ELF file in AppDir, without copying it into the library dir
                    //
                    // the dependencies end up in the regular location
                    bool deployDependenciesOnlyForElfFile(const boost::filesystem::path& elfFilePath, bool failSilentForNonElfFile = false);

                    // deploy desktop file
                    bool deployDesktopFile(const desktopfile::DesktopFile& desktopFile);

                    // deploy icon
                    bool deployIcon(const boost::filesystem::path& path);

                    // deploy icon, changing its name to <target filename>.<ext>
                    bool deployIcon(const boost::filesystem::path& path, const std::string& targetFilename);

                    // deploy arbitrary file
                    boost::filesystem::path deployFile(const boost::filesystem::path& from, const boost::filesystem::path& to);

                    // copy arbitrary file (immediately)
                    bool copyFile(const boost::filesystem::path& from, const boost::filesystem::path& to, bool overwrite = false) const;

                    // create an <AppDir> relative symlink to <target> at <symlink>.
                    bool createRelativeSymlink(const boost::filesystem::path& target, const boost::filesystem::path& symlink) const;

                    // execute deferred copy operations
                    bool executeDeferredOperations();

                    // return path to AppDir
                    boost::filesystem::path path() const;

                    // create a list of all icon paths in the AppDir
                    std::vector<boost::filesystem::path> deployedIconPaths() const;

                    // create a list of all executable paths in the AppDir
                    std::vector<boost::filesystem::path> deployedExecutablePaths() const;

                    // create a list of all desktop file paths in the AppDir
                    std::vector<desktopfile::DesktopFile> deployedDesktopFiles() const;

                    // create symlinks for AppRun, desktop file and icon in the AppDir root directory
                    bool setUpAppDirRoot(const desktopfile::DesktopFile& desktopFile, boost::filesystem::path customAppRunPath = "");

                    // list all executables in <AppDir>/usr/bin
                    // this function does not perform a recursive search, but only searches the bin directory
                    std::vector<boost::filesystem::path> listExecutables() const;

                    // list all shared libraries in <AppDir>/usr/lib
                    // this function recursively searches the entire lib directory for shared libraries
                    std::vector<boost::filesystem::path> listSharedLibraries() const;

                    // search for executables and libraries and deploy their dependencies
                    // calling this function can turn sure file trees created by make install commands into working
                    // AppDirs
                    bool deployDependenciesForExistingFiles() const;

                    // disable deployment of copyright files for this instance
                    void setDisableCopyrightFilesDeployment(bool disable);
            };
        }
    }
}
