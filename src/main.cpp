// system headers
#include <iostream>

// library headers
#include <args.hxx>

// local headers
#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/desktopfile/desktopfile.h"
#include "linuxdeploy/log/log.h"
#include "linuxdeploy/plugin/plugin.h"
#include "linuxdeploy/util/util.h"
#include "core.h"

using namespace linuxdeploy;
using namespace linuxdeploy::core;
using namespace linuxdeploy::log;
using namespace linuxdeploy::util;

namespace fs = std::filesystem;

int main(int argc, char** argv) {
    args::ArgumentParser parser(
        "linuxdeploy -- create AppDir bundles with ease"
    );

    args::HelpFlag help(parser, "help", "Display this help text", {'h', "help"});
    args::Flag showVersion(parser, "", "Print version and exit", {'V', "version"});
    args::ValueFlag<int> verbosity(parser, "verbosity", "Verbosity of log output (0 = debug, 1 = info (default), 2 = warning, 3 = error)", {'v', "verbosity"});

    args::ValueFlag<std::string> appDirPath(parser, "appdir", "Path to target AppDir", {"appdir"});

    args::ValueFlagList<std::string> sharedLibraryPaths(parser, "library", "Shared library to deploy", {'l', "library"});
    args::ValueFlagList<std::string> excludeLibraryPatterns(parser, "pattern", "Shared library to exclude from deployment (glob pattern)", {"exclude-library"});

    args::ValueFlagList<std::string> executablePaths(parser, "executable", "Executable to deploy", {'e', "executable"});

    args::ValueFlagList<std::string> deployDepsOnlyPaths(parser, "path", "Path to ELF file or directory containing such files (libraries or executables) in the AppDir whose dependencies shall be deployed by linuxdeploy without copying them into the AppDir", {"deploy-deps-only"});

    args::ValueFlagList<std::string> desktopFilePaths(parser, "desktop file", "Desktop file to deploy", {'d', "desktop-file"});
    args::Flag createDesktopFile(parser, "", "Create basic desktop file that is good enough for some tests", {"create-desktop-file"});

    args::ValueFlagList<std::string> iconPaths(parser, "icon file", "Icon to deploy", {'i', "icon-file"});
    args::ValueFlag<std::string> iconTargetFilename(parser, "filename", "Filename all icons passed via -i should be renamed to", {"icon-filename"});

    args::ValueFlag<std::string> customAppRunPath(parser, "AppRun path", "Path to custom AppRun script (linuxdeploy will not create a symlink but copy this file instead)", {"custom-apprun"});

    args::Flag listPlugins(parser, "", "Search for plugins, print them to stdout and exit", {"list-plugins"});
    args::ValueFlagList<std::string> inputPlugins(parser, "name", "Input plugins to run (check whether they are available with --list-plugins)", {'p', "plugin"});
    args::ValueFlagList<std::string> outputPlugins(parser, "name", "Output plugins to run (check whether they are available with --list-plugins)", {'o', "output"});

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help&) {
        std::cerr << parser;

        // license information
        std::cerr << std::endl
                  << "===== library information =====" << std::endl
                  << std::endl
                  << "This software uses the great CImg library, as well as libjpeg and libpng." << std::endl
                  << std::endl
                  << "libjpeg license information: this software is based in part on the work of the Independent JPEG Group" << std::endl
                  << std::endl
                  << "CImg license information: This software is governed either by the CeCILL or the CeCILL-C "
                     "license under French law and abiding by the rules of distribution of free software. You can "
                     "use, modify and or redistribute the software under the terms of the CeCILL or CeCILL-C "
                     "licenses as circulated by CEA, CNRS and INRIA at the following URL: "
                     "\"http://cecill.info\"." << std::endl;

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
    appDir.setExcludeLibraryPatterns(excludeLibraryPatterns.Get());

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
            if (!fs::exists(libraryPath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << libraryPath << std::endl;
                return 1;
            }

            if (!appDir.forceDeployLibrary(libraryPath)) {
                ldLog() << LD_ERROR << "Failed to deploy library: " << libraryPath << std::endl;
                return 1;
            }
        }
    }

    // deploy executables to usr/bin, and deploy their dependencies to usr/lib
    if (executablePaths) {
        ldLog() << std::endl << "-- Deploying executables --" << std::endl;

        for (const auto& executablePath : executablePaths.Get()) {
            if (!fs::exists(executablePath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << executablePath << std::endl;
                return 1;
            }

            if (!appDir.deployExecutable(executablePath)) {
                ldLog() << LD_ERROR << "Failed to deploy executable: " << executablePath << std::endl;
                return 1;
            }
        }
    }

    // deploy executables to usr/bin, and deploy their dependencies to usr/lib
    if (deployDepsOnlyPaths) {
        ldLog() << std::endl << "-- Deploying dependencies only for ELF files --" << std::endl;

        for (const auto& path : deployDepsOnlyPaths.Get()) {
            if (fs::is_directory(path)) {
                ldLog() << "Deploying files in directory" << path << std::endl;

                for (auto it = fs::directory_iterator{path}; it != fs::directory_iterator{}; ++it) {
                    if (!fs::is_regular_file(*it)) {
                        continue;
                    }

                    if (!appDir.deployDependenciesOnlyForElfFile(*it, true)) {
                        ldLog() << LD_WARNING << "Failed to deploy dependencies for ELF file" << *it << LD_NO_SPACE << ", skipping" << std::endl;
                        continue;
                    }
                }
            } else if (fs::is_regular_file(path)) {
                if (!appDir.deployDependenciesOnlyForElfFile(path)) {
                    ldLog() << LD_ERROR << "Failed to deploy dependencies for ELF file: " << path << std::endl;
                    return 1;
                }
            } else {
                ldLog() << LD_ERROR << "No such file or directory: " << path << std::endl;
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
                ldLog() << LD_ERROR << "Could not find plugin:" << pluginName << std::endl;
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
            if (!fs::exists(iconPath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << iconPath << std::endl;
                return 1;
            }

            bool iconDeployedSuccessfully;

            if (iconTargetFilename) {
                iconDeployedSuccessfully = appDir.deployIcon(iconPath, iconTargetFilename.Get());
            } else {
                iconDeployedSuccessfully = appDir.deployIcon(iconPath);
            }

            if (!iconDeployedSuccessfully) {
                ldLog() << LD_ERROR << "Failed to deploy icon: " << iconPath << std::endl;
                return 1;
            }
        }
    }

    if (desktopFilePaths) {
        ldLog() << std::endl << "-- Deploying desktop files --" << std::endl;

        for (const auto& desktopFilePath : desktopFilePaths.Get()) {
            if (!fs::exists(desktopFilePath)) {
                ldLog() << LD_ERROR << "No such file or directory: " << desktopFilePath << std::endl;
                return 1;
            }

            desktopfile::DesktopFile desktopFile(desktopFilePath);

            if (!appDir.deployDesktopFile(desktopFile)) {
                ldLog() << LD_ERROR << "Failed to deploy desktop file: " << desktopFilePath << std::endl;
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

        auto executableName = fs::path(executablePaths.Get().front()).filename().string();

        auto desktopFilePath = appDir.path() / "usr/share/applications" / (executableName + ".desktop");

        if (fs::exists(desktopFilePath)) {
            ldLog() << LD_WARNING << "Working on existing desktop file:" << desktopFilePath << std::endl;
        } else {
            ldLog() << "Creating new desktop file:" << desktopFilePath << std::endl;
        }

        desktopfile::DesktopFile desktopFile;
        if (!addDefaultKeys(desktopFile, executableName)) {
            ldLog() << LD_WARNING << "Tried to overwrite existing entries in desktop file:" << desktopFilePath << std::endl;
        }

        if (!desktopFile.save(desktopFilePath.string())) {
            ldLog() << LD_ERROR << "Failed to save desktop file:" << desktopFilePath << std::endl;
            return 1;
        }
    }

    // linuxdeploy offers a special "plugin mode" where plugins can run linuxdeploy again to deploy dependencies
    // this way, they don't have to use liblinuxdeploy (like, e.g., the Qt plugin), but can just be, e.g., shell scripts
    // copying .so files into the AppDir
    // as linuxdeploy aims to be idempotent, so this just costs some time/performance/file I/O, but should not change
    // the outcome
    // the AppDir root deployment, however, does make some assumptions that are only valid when not being run from a
    // plugin
    // therefore, we let plugins signalize that they are calling linuxdeploy, and skip those steps
    // TODO: eliminate the need for this mode in the AppDir root deployment
    if (getenv("LINUXDEPLOY_PLUGIN_MODE") != nullptr) {
        ldLog() << LD_WARNING << "Running in plugin mode, exiting" << std::endl;
        return 0;
    }

    if (!linuxdeploy::deployAppDirRootFiles(desktopFilePaths.Get(), customAppRunPath.Get(), appDir))
        return 1;

    if (outputPlugins) {
        for (const auto& pluginName : outputPlugins.Get()) {
            auto it = foundPlugins.find(std::string(pluginName));

            ldLog() << std::endl << "-- Running output plugin:" << pluginName << "--" << std::endl;

            if (it == foundPlugins.end()) {
                ldLog() << LD_ERROR << "Could not find plugin:" << pluginName << std::endl;
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
                ldLog() << LD_ERROR << "Failed to run plugin:" << pluginName << "(exit code:" << retcode << LD_NO_SPACE << ")" << std::endl;
                return 1;
            }
        }
    }

    return 0;
}

