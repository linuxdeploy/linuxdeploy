#include "gtest/gtest.h"

#include "core.h"
#include "test_util.h"

using namespace std;
using namespace linuxdeploy::core;

namespace fs = std::filesystem;

namespace LinuxDeployTest {
    class IntegrationTests : public ::testing::Test {
    public:
        fs::path tmpAppDir;
        fs::path source_executable_path;
        fs::path target_executable_path;

        fs::path source_desktop_path;
        fs::path target_desktop_path;

        fs::path source_icon_path;
        std::vector<fs::path> target_icon_paths;

        fs::path source_apprun_path;
        fs::path target_apprun_path;

        void SetUp() override {
            tmpAppDir = make_temporary_directory();
            source_executable_path = SIMPLE_EXECUTABLE_PATH;
            target_executable_path = tmpAppDir / "usr/bin" / source_executable_path.filename();

            source_desktop_path = SIMPLE_DESKTOP_ENTRY_PATH;
            target_desktop_path = tmpAppDir / "usr/share/applications" / source_desktop_path.filename();
            source_icon_path = SIMPLE_ICON_PATH;
            if (source_icon_path.extension() == ".svg") {
                target_icon_paths.push_back(tmpAppDir / "usr/share/icons/hicolor/scalable/apps" / source_icon_path.filename());
            } else {
                target_icon_paths.push_back(tmpAppDir / "usr/share/icons/hicolor/32x32/apps" / source_icon_path.filename());
                target_icon_paths.push_back(tmpAppDir / "usr/share/icons/hicolor/128x128/apps" / source_icon_path.filename());
                target_icon_paths.push_back(tmpAppDir / "usr/share/pixmaps" / source_icon_path.filename());
            }
            source_apprun_path = SIMPLE_FILE_PATH;
            target_apprun_path = tmpAppDir / "AppRun";
        }

        void TearDown() override {
            remove_all(tmpAppDir);
        }

        ~IntegrationTests() override = default;

        void listDeployedFiles() {
            std::cout << "Files deployed in AppDir:" << std::endl;
            fs::recursive_directory_iterator end_itr; // default construction yields past-the-end
            for (fs::recursive_directory_iterator itr(tmpAppDir); itr != end_itr; itr++) {
                std::cout << relative(itr->path(), tmpAppDir).string() << std::endl;
            }
        }

        void fillRegularAppDir() {
            add_executable();
            add_desktop();
            add_icon();
        }

        fs::path add_executable() const {
            create_directories(target_executable_path.parent_path());
            copy_file(source_executable_path, target_executable_path);

            return target_executable_path;
        }

        void add_desktop() const {
            create_directories(target_desktop_path.parent_path());
            copy_file(source_desktop_path, target_desktop_path);
        }

        void add_icon() const {
            for (const auto &target_icon_path : target_icon_paths) {
                create_directories(target_icon_path.parent_path());
                // NB: In case of PNG, the same icon is installed to all paths
                copy_file(source_icon_path, target_icon_path);
            }
        }

        void add_apprun() const {
            copy_file(source_apprun_path, target_apprun_path);
        }
    };

    TEST_F(IntegrationTests, deployAppDirRootFilesWithExistentAppRun) {
        fillRegularAppDir();
        add_apprun();

        linuxdeploy::core::appdir::AppDir appDir(tmpAppDir);
        ASSERT_TRUE(linuxdeploy::deployAppDirRootFiles({}, "", appDir));

        EXPECT_TRUE(exists(tmpAppDir / source_desktop_path.filename()));
        EXPECT_TRUE(exists(tmpAppDir / source_icon_path.filename()));
        EXPECT_TRUE(exists(target_apprun_path));
    }

    TEST_F(IntegrationTests, deployAppDirRootFilesWithCustomAppRun) {
        linuxdeploy::core::appdir::AppDir appDir(tmpAppDir);
        linuxdeploy::deployAppDirRootFiles({}, source_apprun_path.string(), appDir);

        ASSERT_TRUE(exists(target_apprun_path));
    }
}
