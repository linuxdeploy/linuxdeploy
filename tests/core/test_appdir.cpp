// library headers
#include "gtest/gtest.h"

// local headers
#include  "linuxdeploy/core/appdir.h"
#include  "test_util.h"

using namespace linuxdeploy::core::appdir;
using namespace linuxdeploy::desktopfile;
using namespace std::filesystem;

namespace {
    void assertIsRegularFile(const path& pathToCheck) {
        ASSERT_TRUE(is_regular_file(pathToCheck));
        EXPECT_TRUE(static_cast<unsigned int>(status(pathToCheck).permissions()) >= 0644);
    }

    void assertIsExecutableFile(const path& pathToCheck) {
        assertIsRegularFile(pathToCheck);
        EXPECT_TRUE(static_cast<unsigned int>(status(pathToCheck).permissions()) >= 0755);
    }

    void assertIsSymlink(const path& targetPath, const path& symlinkPath) {
        const auto resolvedPath = read_symlink(symlinkPath);
        EXPECT_TRUE(resolvedPath == targetPath) << resolvedPath << " " << targetPath;
    }
}

namespace AppDirTest {
    class AppDirUnitTestsFixture : public ::testing::Test {
    public:
        path tmpAppDir;
        AppDir appDir;

    public:
        AppDirUnitTestsFixture() :
            tmpAppDir(make_temporary_directory()),
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
        ASSERT_TRUE(appDir.executeDeferredOperations());

        ASSERT_TRUE(is_regular_file(tmpAppDir / "usr/lib" / path(SIMPLE_LIBRARY_PATH).filename()));
    }

    TEST_F(AppDirUnitTestsFixture, deployExecutable) {
        appDir.deployExecutable(SIMPLE_EXECUTABLE_PATH);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        const auto binaryTargetPath = tmpAppDir / "usr/bin" / path(SIMPLE_EXECUTABLE_PATH).filename();
        const auto libTargetPath = tmpAppDir / "usr/lib" / path(SIMPLE_LIBRARY_PATH).filename();

        assertIsExecutableFile(binaryTargetPath);
        assertIsRegularFile(libTargetPath);
    }

    TEST_F(AppDirUnitTestsFixture, deployDesktopFile) {
        const DesktopFile desktopFile{SIMPLE_DESKTOP_ENTRY_PATH};
        appDir.deployDesktopFile(desktopFile);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        const auto targetPath = tmpAppDir / "usr/share/applications" / path(SIMPLE_DESKTOP_ENTRY_PATH).filename();

        assertIsRegularFile(targetPath);
    }


    TEST_F(AppDirUnitTestsFixture, deployIcon) {
        appDir.deployIcon(SIMPLE_ICON_PATH);
        appDir.deployIcon(SIMPLE_ICON_PATH2);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        const auto targetPath = tmpAppDir / "usr/share/icons/hicolor/16x16/apps" / path(SIMPLE_ICON_PATH).filename();
        const auto targetPath2 = tmpAppDir / "usr/share/icons/hicolor/scalable/apps" / path(SIMPLE_ICON_PATH2).filename();
        assertIsRegularFile(targetPath);
        assertIsRegularFile(targetPath2);
    }

    TEST_F(AppDirUnitTestsFixture, deployFileToDirectory) {
        const auto destination = tmpAppDir / "usr/share/doc/simple_application/";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        const auto targetPath = destination / path(SIMPLE_FILE_PATH).filename();
        assertIsRegularFile(targetPath);
    }

    TEST_F(AppDirUnitTestsFixture, deployFileToAbsoluteFilePath) {
        const auto destination = tmpAppDir / "usr/share/doc/simple_application/test123";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        assertIsRegularFile(destination);
    }

    TEST_F(AppDirUnitTestsFixture, createSymlink) {
        const auto destination = tmpAppDir / "usr/share/doc/simple_application/test123";
        appDir.deployFile(SIMPLE_FILE_PATH, destination);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        assertIsRegularFile(destination);

        const auto symlinkDestination = tmpAppDir / "relative_link";
        appDir.createRelativeSymlink(destination, symlinkDestination);

        assertIsSymlink(relative(destination, tmpAppDir), symlinkDestination);
    }

    TEST_F(AppDirUnitTestsFixture, testAddingMinimumPermissionsToRegularFile) {
        const auto destination = tmpAppDir / "usr/share/doc/simple_application/";
        appDir.deployFile(READONLY_FILE_PATH, destination);
        ASSERT_TRUE(appDir.executeDeferredOperations());

        const auto targetPath = destination / path(READONLY_FILE_PATH).filename();
        assertIsRegularFile(targetPath);
    }

    TEST_F(AppDirUnitTestsFixture, testDeployingNonexistingFile) {
        const auto nonexistingFilePath = "/i/am/sure/this/file/does/not/exist";
        // it is very unlikely that this file does not exist, but we should probably check that...
        ASSERT_FALSE(exists(nonexistingFilePath));

        const auto destination = tmpAppDir / "usr/share/doc/simple_application/";
        appDir.deployFile(nonexistingFilePath, destination);
        ASSERT_FALSE(appDir.executeDeferredOperations());
    }
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
