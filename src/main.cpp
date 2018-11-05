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
#include "linuxdeploy/plugin/plugin.h"
#include "linuxdeploy/util/util.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;
using namespace linuxdeploy::util;

namespace bf = boost::filesystem;

int main(int argc, char** argv) {
    args::ArgumentParser parser(
        "linuxdeploy -- create AppDir bundles with ease"
    );

    args::HelpFlag help(parser, "help", "Display this help text", {'h', "help"});
    args::Flag showVersion(parser, "", "Print version and exit", {'V', "version"});
    args::ValueFlag<int> verbosity(parser, "verbosity", "Verbosity of log output (0 = debug, 1 = info (default), 2 = warning, 3 = error)", {'v', "verbosity"});

    args::ValueFlag<std::string> appDirPath(parser, "appdir", "Path to target AppDir", {"appdir"});

    args::ValueFlagList<std::string> sharedLibraryPaths(parser, "library", "Shared library to deploy", {'l', "library"});

    args::ValueFlagList<std::string> executablePaths(parser, "executable", "Executable to deploy", {'e', "executable"});

    args::ValueFlagList<std::string> desktopFilePaths(parser, "desktop file", "Desktop file to deploy", {'d', "desktop-file"});
    args::Flag createDesktopFile(parser, "", "Create basic desktop file that is good enough for some tests", {"create-desktop-file"});

    args::ValueFlagList<std::string> iconPaths(parser, "icon file", "Icon to deploy", {'i', "icon-file"});

    args::ValueFlag<std::string> customAppRunPath(parser, "AppRun path", "Path to custom AppRun script (linuxdeploy will not create a symlink but copy this file instead)", {"custom-apprun"});

    args::Flag listPlugins(parser, "", "Search for plugins, print them to stdout and exit", {"list-plugins"});
    args::ValueFlagList<std::string> inputPlugins(parser, "name", "Input plugins to run (check whether they are available with --list-plugins)", {'p', "plugin"});
    args::ValueFlagList<std::string> outputPlugins(parser, "name", "Output plugins to run (check whether they are available with --list-plugins)", {'o', "output"});

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
    std::cerr << "linuxdeploy version " << LD_VERSION
              << " (git commit ID " << LD_GIT_COMMIT << "), "
              << LD_BUILD_NUMBER << " built on " << LD_BUILD_DATE << std::endl;

    // if only the version should be shown, we can exit now
    if (showVersion)
        return 0;

    // set verbosity
    if (verbosity) {
        ldLog::setVerbosity((LD_LOGLEVEL) verbosity.Get());
    }

    auto foundPlugins = linuxdeploy::plugin::findPlugins();

    if (listPlugins) {
        ldLog() << "Available plugins:" << std::endl;
        for (const auto& plugin : foundPlugins) {
            ldLog() << plugin.first << LD_NO_SPACE << ":" << plugin.second->path()
                    << "(type:" << plugin.second->pluginTypeString() << LD_NO_SPACE << ","
                    << "API level:" << plugin.second->apiLevel()
                    << LD_NO_SPACE << ")" << std::endl;
        }
        return 0;
    }

    if (!appDirPath) {
        ldLog() << LD_ERROR << "--appdir parameter required" << std::endl;
        std::cerr << std::endl << parser;
        return 1;
    }

    appdir::AppDir appDir(appDirPath.Get());

    // allow disabling copyright files deployment via environment variable
    if (getenv("DISABLE_COPYRIGHT_FILES_DEPLOYMENT") != nullptr) {
        ldLog() << std::endl << LD_WARNING << "Copyright files deployment disabled" << std::endl;
        appDir.setDisableCopyrightFilesDeployment(true);
    }

    // initialize AppDir with common directories
    ldLog() << std::endl << "-- Creating basic AppDir structure --" << std::endl;
    if (!appDir.createBasicStructure()) {
        ldLog() << LD_ERROR << "Failed to create basic AppDir structure" << std::endl;
        return 1;
    }

    ldLog() << std::endl << "-- Deploying dependencies for existing files in AppDir --" << std::endl;
    if (!appDir.deployDependenciesForExistingFiles()) {
        ldLog() << LD_ERROR << "Failed to deploy dependencies for existing files" << std::endl;
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

            if (!appDir.forceDeployLibrary(libraryPath)) {
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

    // perform deferred copy operations before running input plugins to make sure all files the plugins might expect
    // are in place
    ldLog() << std::endl << "-- Copying files into AppDir --" << std::endl;
    if (!appDir.executeDeferredOperations()) {
        return 1;
    }

    // run input plugins before deploying icons and desktop files
    // the input plugins might even fetch these resources somewhere into the AppDir, and this way, the user can make use of that
    if (inputPlugins) {
        for (const auto& pluginName : inputPlugins.Get()) {
            auto it = foundPlugins.find(std::string(pluginName));

            ldLog() << std::endl << "-- Running input plugin:" << pluginName << "--" << std::endl;

            if (it == foundPlugins.end()) {
                ldLog() << LD_ERROR << "Could not find plugin:" << pluginName;
                return 1;
            }

            auto plugin = it->second;

            if (plugin->pluginType() != linuxdeploy::plugin::INPUT_TYPE) {
                if (plugin->pluginType() == linuxdeploy::plugin::OUTPUT_TYPE) {
                    ldLog() << LD_ERROR << "Plugin" << pluginName << "is an output plugin, please use like --output" << pluginName << std::endl;
                } else {
                    ldLog() << LD_ERROR << "Plugin" << pluginName << "has unkown type:" << plugin->pluginType() << std::endl;
                }
                return 1;
            }

            auto retcode = plugin->run(appDir.path());

            if (retcode != 0) {
                ldLog() << LD_ERROR << "Failed to run plugin:" << pluginName << "(exit code:" << retcode << LD_NO_SPACE << ")" << std::endl;
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
                std::cerr << "Failed to deploy icon: " << iconPath << std::endl;
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

    // perform deferred copy operations before creating other files here before trying to copy the files to the AppDir root
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
        ldLog() << LD_WARNING << "Please beware the created desktop file is of low quality and should be edited or replaced before using it for production releases!" << std::endl;

        auto executableName = bf::path(executablePaths.Get().front()).filename().string();

        auto desktopFilePath = appDir.path() / "usr/share/applications" / (executableName + ".desktop");

        if (bf::exists(desktopFilePath)) {
            ldLog() << LD_WARNING << "Working on existing desktop file:" << desktopFilePath << std::endl;
        } else {
            ldLog() << "Creating new desktop file:" << desktopFilePath << std::endl;
        }

        desktopfile::DesktopFile desktopFile(desktopFilePath);
        if (!desktopFile.addDefaultKeys(executableName)) {
            ldLog() << LD_WARNING << "Tried to overwrite existing entries in desktop file:" << desktopFilePath << std::endl;
        }

        if (!desktopFile.save()) {
            ldLog() << LD_ERROR << "Failed to save desktop file:" << desktopFilePath << std::endl;
            return 1;
        }
    }

    // search for desktop file and deploy it to AppDir root
    ldLog() << std::endl << "-- Deploying files into AppDir root directory --" << std::endl;

    if (bf::is_regular_file(appDir.path() / "AppRun")) {
        if (customAppRunPath)
            ldLog() << LD_WARNING << "AppRun exists but custom AppRun specified, overwriting existing AppRun" << std::endl;
        else
            ldLog() << LD_WARNING << "AppRun exists, skipping deployment" << std::endl;
    } else {
        auto deployedDesktopFiles = appDir.deployedDesktopFiles();

        desktopfile::DesktopFile desktopFile;

        if (deployedDesktopFiles.empty()) {
            ldLog() << LD_WARNING << "Could not find desktop file in AppDir, cannot create links for AppRun, desktop file and icon in AppDir root" << std::endl;
        } else {
            if (!desktopFilePaths.Get().empty()) {
                auto firstDeployedDesktopFileName = bf::path(desktopFilePaths.Get().front()).filename().string();

                auto desktopFileMatchingName = std::find_if(
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
                    ldLog() << LD_ERROR << "Could not find desktop file deployed earlier any more:" << firstDeployedDesktopFileName << std::endl;
                    return 1;
                }
            } else {
                desktopFile = deployedDesktopFiles[0];
                ldLog() << LD_WARNING << "No desktop file specified, using first desktop file found:" << desktopFile.path() << std::endl;
            }

            ldLog() << "Deploying desktop file:" << desktopFile.path() << std::endl;

            bool rv;

            if (customAppRunPath) {
                rv = appDir.createLinksInAppDirRoot(desktopFile, customAppRunPath.Get());
            } else {
                rv = appDir.createLinksInAppDirRoot(desktopFile);
            }

            if (!rv)
                return 1;
        }
    }

    if (outputPlugins) {
        for (const auto& pluginName : outputPlugins.Get()) {
            auto it = foundPlugins.find(std::string(pluginName));

            ldLog() << std::endl << "-- Running output plugin:" << pluginName << "--" << std::endl;

            if (it == foundPlugins.end()) {
                ldLog() << LD_ERROR << "Could not find plugin:" << pluginName;
                return 1;
            }

            auto plugin = it->second;

            if (plugin->pluginType() != linuxdeploy::plugin::OUTPUT_TYPE) {
                if (plugin->pluginType() == linuxdeploy::plugin::INPUT_TYPE) {
                    ldLog() << LD_ERROR << "Plugin" << pluginName << "is an input plugin, please use like --plugin" << pluginName << std::endl;
                } else {
                    ldLog() << LD_ERROR << "Plugin" << pluginName << "has unknown type:" << plugin->pluginType() << std::endl;
                }
                return 1;
            }

            auto retcode = plugin->run(appDir.path());

            if (retcode != 0) {
                ldLog() << LD_ERROR << "Failed to run plugin:" << pluginName << std::endl;
                ldLog() << LD_DEBUG << "Exited with return code:" << retcode << std::endl;
                return 1;
            }
        }
    }

    return 0;
}
