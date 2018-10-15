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
}
