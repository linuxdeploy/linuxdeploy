#include "gtest/gtest.h"

#include "linuxdeploy.h"

using namespace boost::filesystem;

namespace LinuxDeployTest {
    class LinuxDeployTestsFixture : public ::testing::Test {
    public:
        path tmpAppDir;

    public:
        LinuxDeployTestsFixture() = default;

        void SetUp() override {
            tmpAppDir = temp_directory_path() / unique_path("linuxdeploy-tests-%%%%-%%%%-%%%%");
            create_directories(tmpAppDir);
        }

        void TearDown() override {
            remove_all(tmpAppDir);
        }

        ~LinuxDeployTestsFixture() override = default;

        void listDeployedFiles() {
            std::cout << "Files deployed in AppDir:" << std::endl;
            recursive_directory_iterator end_itr; // default construction yields past-the-end
            for (recursive_directory_iterator itr(tmpAppDir); itr != end_itr; itr++) {
                std::cout << relative(itr->path(), tmpAppDir).string() << std::endl;
            }
        }

        void fillRegularAppDir() {
            add_executable();
            add_desktop();
            add_icon();
        }

        void add_executable() const {
            path source_executable_path = SIMPLE_EXECUTABLE_PATH;
            path target_executable_path = tmpAppDir / "usr/bin" / source_executable_path.filename();
            create_directories(target_executable_path.parent_path());
            copy_file(source_executable_path, target_executable_path);
        }

        void add_desktop() const {
            path source_desktop_path = SIMPLE_DESKTOP_ENTRY_PATH;
            path target_desktop_path = tmpAppDir / "usr/share/applications" / source_desktop_path.filename();
            create_directories(target_desktop_path.parent_path());
            copy_file(source_desktop_path, target_desktop_path);
        }

        void add_icon() const {
            path source_icon_path = SIMPLE_ICON_PATH;
            path target_icon_path = tmpAppDir / "usr/share/icons/hicolor/scalable/apps" / source_icon_path.filename();
            create_directories(target_icon_path.parent_path());
            copy_file(source_icon_path, target_icon_path);
        }

        void add_apprun() const {
            path source_apprun_path = SIMPLE_FILE_PATH;
            path target_apprun_path = tmpAppDir / "AppRun";
            copy_file(source_apprun_path, target_apprun_path);
        }
    };

    TEST_F(LinuxDeployTestsFixture, deployAppDirRootFilesWithExistentAppRun) {
        fillRegularAppDir();
        add_apprun();

        listDeployedFiles();
    }
}
