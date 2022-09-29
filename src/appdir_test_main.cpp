// system headers
#include <filesystem>
#include <iostream>

// library headers
#include <args.hxx>

// local headers
#include <linuxdeploy/core/appdir.h>
#include "core/appdir.cpp"

namespace fs = std::filesystem;

using namespace linuxdeploy::core;

int main(const int argc, const char* const* const argv) {
    args::ArgumentParser parser("AppDir test");

    args::ValueFlag<std::string> appdir(parser, "appdir", "AppDir to use", {"appdir"});

    args::Flag listExecutables(parser, "", "List executables in AppDir", {"list-executables"});
    args::Flag listSharedLibraries(parser, "", "List shared libraries in AppDir", {"list-shared-libraries"});
    args::ValueFlag<std::string> listFilesInDirectory(parser, "", "List files in directory relative to AppDir", {"list-files-in-directory"});
    args::ValueFlag<std::string> listFilesInDirectoryRecursively(parser, "", "List files in directory relative to AppDir", {"list-files-in-directory-recursively"});

    if (listFilesInDirectory) {
        for (const auto& i : appdir::listFilesInDirectory(listFilesInDirectory.Get(), false)) {
            std::cout << i.string() << std::endl;
        }
        return 1;
    }

    if (listFilesInDirectoryRecursively) {
        for (const auto& i : appdir::listFilesInDirectory(listFilesInDirectoryRecursively.Get(), true)) {
            std::cout << i.string() << std::endl;
        }
        return 1;
    }

    if (!appdir) {
        std::cout << "--appdir required" << std::endl;
        std::cout << std::endl << parser;
        return 1;
    }

    appdir::AppDir appDir(appdir.Get());

    if (listExecutables) {
        for (const auto& i : appDir.listExecutables()) {
            std::cout << i.string() << std::endl;
        }
        return 0;
    }

    if (listSharedLibraries) {
        for (const auto& i : appDir.listSharedLibraries()) {
            std::cout << i.string() << std::endl;
        }
        return 0;
    }

    return 0;
}
