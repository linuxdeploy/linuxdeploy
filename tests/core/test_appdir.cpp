#include "gtest/gtest.h"
#include  "linuxdeploy/core/appdir.h"

using namespace linuxdeploy::core::appdir;
using namespace linuxdeploy::desktopfile;
using namespace boost::filesystem;

namespace AppDirTest {
    class AppDirUnitTestsFixture : public ::testing::Test {
    public:
        path tmpAppDir;
        AppDir appDir;

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
    };

    TEST_F(AppDirUnitTestsFixture, createBasicStructure) {
        // in this test case we expect the following exact directory set to be created in the AppDir
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


    TEST_F(AppDirUnitTestsFixture, deployLibraryWrongPath) {
        ASSERT_TRUE(!appDir.deployLibrary("/lib/fakelib.so"));
    }

    TEST_F(AppDirUnitTestsFixture, deployLibrary) {
        appDir.deployLibrary(SIMPLE_LIBRARY_PATH);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/lib" / path(SIMPLE_LIBRARY_PATH).filename()));
    }

    TEST_F(AppDirUnitTestsFixture, deployExecutable) {
        appDir.deployExecutable(SIMPLE_EXECUTABLE_PATH);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/bin" / path(SIMPLE_EXECUTABLE_PATH).filename()));
        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/lib" / path(SIMPLE_LIBRARY_PATH).filename()));
    }

    TEST_F(AppDirUnitTestsFixture, deployDesktopFile) {
        DesktopFile desktopFile{SIMPLE_DESKTOP_ENTRY_PATH};
        appDir.deployDesktopFile(desktopFile);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/share/applications" / path(SIMPLE_DESKTOP_ENTRY_PATH).filename()));
    }


    TEST_F(AppDirUnitTestsFixture, deployIcon) {
        appDir.deployIcon(SIMPLE_PNG_ICON_PATH);
        appDir.deployIcon(SIMPLE_SVG_ICON_PATH);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/share/icons/hicolor/16x16/apps" / path(SIMPLE_PNG_ICON_PATH).filename()));
        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/share/icons/hicolor/scalable/apps" / path(SIMPLE_SVG_ICON_PATH).filename()));
    }

    TEST_F(AppDirUnitTestsFixture, deployFileToDirectory) {
        auto destination = tmpAppDir / "usr/share/doc/simple_application/";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(destination / path(SIMPLE_FILE_PATH).filename()));
    }

    TEST_F(AppDirUnitTestsFixture, deployFileToAbsoluteFilePath) {
        auto destination = tmpAppDir / "usr/share/doc/simple_application/test123";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(destination));
    }

    TEST_F(AppDirUnitTestsFixture, createSymlink) {
        auto destination = tmpAppDir / "usr/share/doc/simple_application/test123";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        appDir.executeDeferredOperations();

        ASSERT_TRUE(is_regular_file(destination));

        appDir.createRelativeSymlink(destination, tmpAppDir / "relative_link");

        auto res = read_symlink(tmpAppDir / "relative_link");
        auto expected = relative(destination, tmpAppDir);
        ASSERT_TRUE(res == expected);
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
