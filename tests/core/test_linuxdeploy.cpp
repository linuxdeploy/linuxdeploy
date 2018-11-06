#include "gtest/gtest.h"

#include "core.h"

using namespace std;
namespace bf = boost::filesystem;

namespace LinuxDeployTest {
    class LinuxDeployTestsFixture : public ::testing::Test {
    public:
        bf::path tmpAppDir;
        bf::path source_executable_path;
        bf::path target_executable_path;

        bf::path source_desktop_path;
        bf::path target_desktop_path;

        bf::path source_icon_path;
        bf::path target_icon_path;

        bf::path source_apprun_path;
        bf::path target_apprun_path;

        void SetUp() override {
            tmpAppDir = bf::temp_directory_path() / bf::unique_path("linuxdeploy-tests-%%%%-%%%%-%%%%");
            source_executable_path = SIMPLE_EXECUTABLE_PATH;
            target_executable_path = tmpAppDir / "usr/bin" / source_executable_path.filename();

            source_desktop_path = SIMPLE_DESKTOP_ENTRY_PATH;
            target_desktop_path = tmpAppDir / "usr/share/applications" / source_desktop_path.filename();
            source_icon_path = SIMPLE_ICON_PATH;
            target_icon_path = tmpAppDir / "usr/share/icons/hicolor/scalable/apps" / source_icon_path.filename();
            source_apprun_path = SIMPLE_FILE_PATH;
            target_apprun_path = tmpAppDir / "AppRun";

            create_directories(tmpAppDir);
        }

        void TearDown() override {
            remove_all(tmpAppDir);
        }

        ~LinuxDeployTestsFixture() override = default;

        void listDeployedFiles() {
            std::cout << "Files deployed in AppDir:" << std::endl;
            bf::recursive_directory_iterator end_itr; // default construction yields past-the-end
            for (bf::recursive_directory_iterator itr(tmpAppDir); itr != end_itr; itr++) {
                std::cout << relative(itr->path(), tmpAppDir).string() << std::endl;
            }
        }

        void fillRegularAppDir() {
            add_executable();
            add_desktop();
            add_icon();
        }

        bf::path add_executable() const {
            create_directories(target_executable_path.parent_path());
            copy_file(source_executable_path, target_executable_path);

            return target_executable_path;
        }

        void add_desktop() const {
            create_directories(target_desktop_path.parent_path());
            copy_file(source_desktop_path, target_desktop_path);
        }

        void add_icon() const {
            create_directories(target_icon_path.parent_path());
            copy_file(source_icon_path, target_icon_path);
        }

        void add_apprun() const {
            copy_file(source_apprun_path, target_apprun_path);
        }
    };

    TEST_F(LinuxDeployTestsFixture, deployAppDirRootFilesWithExistentAppRun) {
        fillRegularAppDir();
        add_apprun();

        linuxdeploy::core::appdir::AppDir appDir(tmpAppDir);
        linuxdeploy::deployAppDirRootFiles({}, "", appDir);

        ASSERT_TRUE(exists(tmpAppDir / source_desktop_path.filename()));
        ASSERT_TRUE(exists(tmpAppDir / source_icon_path.filename()));
        ASSERT_TRUE(exists(target_apprun_path));
    }

    TEST_F(LinuxDeployTestsFixture, deployAppDirRootFilesWithCustomAppRun) {
        linuxdeploy::core::appdir::AppDir appDir(tmpAppDir);
        linuxdeploy::deployAppDirRootFiles({}, source_apprun_path.string(), appDir);

        ASSERT_TRUE(exists(target_apprun_path));
    }
}
