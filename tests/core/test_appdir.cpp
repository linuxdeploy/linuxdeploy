#include "gtest/gtest.h"
#include  "linuxdeploy/core/appdir.h"

using namespace linuxdeploy::core::appdir;
using namespace linuxdeploy::core::desktopfile;
using namespace boost::filesystem;

namespace AppDirTest {
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
            ASSERT_NE(expected.find(path), expected.end());
            expected.erase(path);
        }
        ASSERT_TRUE(expected.empty());
    }


    TEST_F(AppDirUnitTestsFixture, depoloyLibraryWrongPath) {
        path libPath = "/lib/fakelib.so";
        ASSERT_THROW(appDir.deployLibrary(libPath), std::exception);
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

        ASSERT_TRUE(libsimple_library_found);
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

        ASSERT_TRUE(libsimple_library_found && !simple_executable_found);
    }

    TEST_F(AppDirUnitTestsFixture, deployDesktopFile) {
        DesktopFile desktopFile{SIMPLE_DESKTOP_ENTRY_PATH};
        appDir.deployDesktopFile(desktopFile);
        appDir.executeDeferredOperations();

        bool simple_app_desktop_found = false;
        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr && (!simple_app_desktop_found); itr++) {
            const auto path = relative(itr->path(), tmpAppDir).filename().string();

            if (path.find("simple_app.desktop") != std::string::npos)
                simple_app_desktop_found = true;
        }

        ASSERT_TRUE(simple_app_desktop_found);
    }


    TEST_F(AppDirUnitTestsFixture, deployIcon) {
        path iconPath = SIMPLE_ICON_PATH;
        appDir.deployIcon(iconPath);
        appDir.executeDeferredOperations();

        bool simple_icon_found = false;
        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr && (!simple_icon_found); itr++) {
            const auto path = relative(itr->path(), tmpAppDir).filename().string();

            if (path.find("simple_icon.svg") != std::string::npos)
                simple_icon_found = true;
        }

        ASSERT_TRUE(simple_icon_found);
    }


    TEST_F(AppDirUnitTestsFixture, deployFile) {
        path filePath = SIMPLE_FILE_PATH;
        appDir.deployFile(filePath, tmpAppDir / "usr/share/doc/simple_application/");
        appDir.executeDeferredOperations();

        bool simple_file_found = false;
        recursive_directory_iterator end_itr; // default construction yields past-the-end
        for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr && (!simple_file_found); itr++) {
            const auto path = relative(itr->path(), tmpAppDir).filename().string();

            if (path.find("simple_file.txt") != std::string::npos)
                simple_file_found = true;
        }

        ASSERT_TRUE(simple_file_found);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
