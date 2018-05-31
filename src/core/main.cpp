// system headers
#include <glob.h>
#include <iostream>

// library headers
#include <args.hxx>

// local headers
#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/core/desktopfile.h"
#include "linuxdeploy/core/elf.h"
#include "linuxdeploy/core/log.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

int main(int argc, char** argv) {
    args::ArgumentParser parser(
        "linuxdeploy -- create AppDir bundles with ease"
    );

    args::HelpFlag help(parser, "help", "Display this help text.", {'h', "help"});
    args::Flag showVersion(parser, "", "Print version and exit", {'V', "version"});
    args::ValueFlag<int> verbosity(parser, "verbosity", "Verbosity of log output (0 = debug, 1 = info, 2 = warning, 3 = error)", {'v', "verbosity"});

    args::Flag initAppDir(parser, "", "Create basic AppDir structure", {"init-appdir"});
    args::ValueFlag<std::string> appDirPath(parser, "appdir", "Path to target AppDir", {"appdir"});
    args::ValueFlag<std::string> appName(parser, "app-name", "Application name (used to initialize desktop file and name icons etc.)", {'n', "app-name"});

    args::ValueFlagList<std::string> sharedLibraryPaths(parser, "library", "Shared library to deploy", {'l', "lib", "library"});

    args::ValueFlagList<std::string> executablePaths(parser, "executable", "Executable to deploy", {'e', "executable"});

    args::ValueFlagList<std::string> desktopFilePaths(parser, "desktop file", "Desktop file to deploy", {'d', "desktop-file"});
    args::Flag createDesktopFile(parser, "", "Create basic desktop file that is good enough for some tests", {"create-desktop-file"});

    args::ValueFlagList<std::string> iconPaths(parser, "icon file", "Icon to deploy", {'i', "icon-file"});

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help&) {
        std::cerr << parser;
        return 0;
    } catch (args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    // always show version statement
    std::cerr << "linuxdeploy version " << LINUXDEPLOY_VERSION << std::endl;

    // set verbosity
    if (verbosity) {
        ldLog::setVerbosity((LD_LOGLEVEL) verbosity.Get());
    }

    if (showVersion)
        return 0;

    if (!appDirPath) {
        ldLog() << LD_ERROR << "--appdir parameter required" << std::endl;
        std::cerr << std::endl << parser;
        return 1;
    }

    appdir::AppDir appDir(appDirPath.Get());

    if (appName) {
        ldLog() << std::endl << "-- Deploying application \"" << LD_NO_SPACE << appName.Get() << LD_NO_SPACE << "\" --" << std::endl;
    }

    // initialize AppDir with common directories on request
    if (initAppDir) {
        ldLog() << std::endl << "-- Creating basic AppDir structure --" << std::endl;

        if (!appDir.createBasicStructure())
            return 1;
    }

    // deploy shared libraries to usr/lib, and deploy their dependencies to usr/lib
    if (sharedLibraryPaths) {
        ldLog() << std::endl << "-- Deploying shared libraries --" << std::endl;

        for (const auto& libraryPath : sharedLibraryPaths.Get()) {
            if (!bf::exists(libraryPath)) {
                std::cerr << "No such file or directory: " << libraryPath << std::endl;
                return 1;
            }

            if (!appDir.deployLibrary(libraryPath)) {
                std::cerr << "Failed to deploy library: " << libraryPath << std::endl;
                return 1;
            }
        }
    }

    // deploy executables to usr/bin, and deploy their dependencies to usr/lib
    if (executablePaths) {
        ldLog() << std::endl << "-- Deploying executables --" << std::endl;

        for (const auto& executablePath : executablePaths.Get()) {
            if (!bf::exists(executablePath)) {
                std::cerr << "No such file or directory: " << executablePath << std::endl;
                return 1;
            }

            if (!appDir.deployExecutable(executablePath)) {
                std::cerr << "Failed to deploy executable: " << executablePath << std::endl;
                return 1;
            }
        }
    }

    if (iconPaths) {
        ldLog() << std::endl << "-- Deploying icons --" << std::endl;

        for (const auto& iconPath : iconPaths.Get()) {
            if (!bf::exists(iconPath)) {
                std::cerr << "No such file or directory: " << iconPath << std::endl;
                return 1;
            }

            if (!appDir.deployIcon(iconPath)) {
                std::cerr << "Failed to deploy desktop file: " << iconPath << std::endl;
                return 1;
            }
        }
    }

    if (desktopFilePaths) {
        ldLog() << std::endl << "-- Deploying desktop files --" << std::endl;

        for (const auto& desktopFilePath : desktopFilePaths.Get()) {
            if (!bf::exists(desktopFilePath)) {
                std::cerr << "No such file or directory: " << desktopFilePath << std::endl;
                return 1;
            }

            desktopfile::DesktopFile desktopFile(desktopFilePath);

            if (!appDir.deployDesktopFile(desktopFile)) {
                std::cerr << "Failed to deploy desktop file: " << desktopFilePath << std::endl;
                return 1;
            }
        }
    }

    // perform deferred copy operations before creating other files here or trying to copy the files to the AppDir root
    ldLog() << std::endl << "-- Copying files into AppDir --" << std::endl;
    if (!appDir.executeDeferredOperations()) {
        return 1;
    }

    if (createDesktopFile) {
        if (!executablePaths) {
            ldLog() << LD_ERROR << "--create-desktop-file requires at least one executable to be passed" << std::endl;
            return 1;
        }

        ldLog() << std::endl << "-- Creating desktop file --" << std::endl;

        auto executableName = bf::path(executablePaths.Get().front()).filename().string();

        auto desktopFilePath = appDir.path() / "usr/share/applications" / (executableName + ".desktop");

        if (bf::exists(desktopFilePath)) {
            ldLog() << LD_WARNING << "Working on existing desktop file:" << desktopFilePath << std::endl;
        } else {
            ldLog() << "Creating new desktop file:" << desktopFilePath << std::endl;
        }

        desktopfile::DesktopFile desktopFile(desktopFilePath);
        if (!desktopFile.addDefaultKeys(executableName)) {
            ldLog() << LD_WARNING << "Tried to overwrite existing entries in desktop file" << std::endl;
        }

        if (!desktopFile.save()) {
            ldLog() << LD_ERROR << "Failed to save desktop file:" << desktopFilePath << std::endl;
            return 1;
        }
    }

    // search for desktop file and deploy it to AppDir root
    {
        ldLog() << std::endl << "-- Deploying files into AppDir root directory --" << std::endl;

        auto deployedDesktopFiles = appDir.deployedDesktopFiles();

        if (deployedDesktopFiles.empty()) {
            ldLog() << LD_WARNING << "Could not find desktop file in AppDir, cannot create links for AppRun, desktop file and icon in AppDir root" << std::endl;
        } else {
            auto& desktopFile = deployedDesktopFiles[0];

            ldLog() << "Deploying desktop file:" << desktopFile.path();

            if (!appDir.createLinksInAppDirRoot(desktopFile))
                return 1;
        }
    }

    return 0;
}
