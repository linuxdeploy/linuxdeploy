#include "gtest/gtest.h"
#include  "linuxdeploy/core/appdir.h"

using namespace linuxdeploy::core::appdir;
using namespace boost::filesystem;
namespace AppDirUnitTests {
    class AppDirUnitTestsFixture : public ::testing::Test {
    public:
        AppDirUnitTestsFixture() :
            tmpAppDir(temp_directory_path() / unique_path("linuxdeploy-tests-%%%%-%%%%-%%%%")),
            appDir(tmpAppDir) {
        }

        void SetUp() override {
        }

        void TearDown() override {
            remove_all(tmpAppDir);
        }

        ~AppDirUnitTestsFixture() override = default;

        void listDeployedFiles() {
            std::cout << "Files deployed in AppDir:";
            recursive_directory_iterator end_itr; // default construction yields past-the-end
            for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr; itr++) {
                std::cout << relative(itr->path(), tmpAppDir).string() << std::endl;
            }
        }

        path tmpAppDir;
        AppDir appDir;
    };

    TEST_F(AppDirUnitTestsFixture, createBasicStructure) {
        std::set<std::string> expected = {
            "usr",
            "usr/bin",
            "usr/share",
            "usr/share/icons",
            "usr/share/icons/hicolor",
            "usr/share/icons/hicolor/scalable",
            "usr/share/icons/hicolor/scalable/apps",
            "usr/share/icons/hicolor/32x32",
            "usr/share/icons/hicolor/32x32/apps",
            "usr/share/icons/hicolor/256x256",
            "usr/share/icons/hicolor/256x256/apps",
            "usr/share/icons/hicolor/16x16",
            "usr/share/icons/hicolor/16x16/apps",
            "usr/share/icons/hicolor/128x128",
            "usr/share/icons/hicolor/128x128/apps",
            "usr/share/icons/hicolor/64x64",
            "usr/share/icons/hicolor/64x64/apps",
            "usr/share/applications",
            "usr/lib",
        };

        appDir.createBasicStructure();

        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr; itr++) {
            std::string path = relative(itr->path(), tmpAppDir).string();
            if (expected.find(path) == expected.end())
                FAIL();
            else
                expected.erase(path);
        }
        if (!expected.empty())
            FAIL();

    }

    TEST_F(AppDirUnitTestsFixture, depoloyLibrary) {
        path libPath = SIMPLE_LIBRARY_PATH;
        appDir.deployLibrary(libPath);
        appDir.executeDeferredOperations();

        bool libsimple_library_found = false;
        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr && (!libsimple_library_found); itr++) {
            const auto path = relative(itr->path(), tmpAppDir).filename().string();

            if (path.find("libsimple_library") != path.npos)
                libsimple_library_found = true;
        }

        if (!libsimple_library_found)
            FAIL();
    }

    TEST_F(AppDirUnitTestsFixture, deployExecutable) {
        path exePath = SIMPLE_EXECUTABLE_PATH;
        appDir.deployExecutable(exePath);
        appDir.executeDeferredOperations();

        bool libsimple_library_found = false;
        bool simple_executable_found = false;
        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir);
             itr != end_itr && (!libsimple_library_found || !simple_executable_found);
             itr++) {
            const auto path = relative(itr->path(), tmpAppDir).filename().string();

            if (path.find("libsimple_library") != std::string::npos)
                libsimple_library_found = true;

            if (path.find("simple_executable") != std::string::npos)
                simple_executable_found = true;
        }

        if (!libsimple_library_found || !simple_executable_found)
            FAIL();
    }
}
