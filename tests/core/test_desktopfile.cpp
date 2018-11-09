// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "linuxdeploy/core/desktopfile.h"
#include "../../src/core/desktopfilereader.h"

using boost::bad_lexical_cast;
using namespace linuxdeploy::core::desktopfile;

namespace bf = boost::filesystem;

class DesktopFileFixture : public ::testing::Test {
public:
    std::string testType;
    std::string testName;
    std::string testExec;
    std::string testIcon;
    std::string testDesktopFile;

private:
    void SetUp() override {
        testType = "Application";
        testName = "Simple Application";
        testExec = "simple_app";
        testIcon = "simple_app";

        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "Type=Application" << std::endl
           << "Name=Simple Application" << std::endl
           << "Exec=" << testExec << std::endl
           << "Icon=" << testIcon << std::endl;

        testDesktopFile = ss.str();
    }

    void TearDown() override {}

public:
    void assertIsTestDesktopFile(const DesktopFile& file, ssize_t expectedKeys = -1) {
        std::stringstream ss;
        file.save(ss);

        assertHasTestDesktopFileKeys(ss, expectedKeys);
    }

    void assertHasTestDesktopFileKeys(std::stringstream& ss, ssize_t expectedKeys = -1) const {
        DesktopFileReader reader(ss);

        if (expectedKeys < 0)
            expectedKeys = 4;

        EXPECT_EQ(reader["Desktop Entry"]["Name"].value(), testName);
        EXPECT_EQ(reader["Desktop Entry"]["Exec"].value(), testExec);
        EXPECT_EQ(reader["Desktop Entry"]["Icon"].value(), testIcon);
        EXPECT_EQ(reader["Desktop Entry"]["Type"].value(), testType);
        EXPECT_EQ(reader["Desktop Entry"].size(), expectedKeys);
    }
};

TEST_F(DesktopFileFixture, testDefaultConstructor) {
    DesktopFile file;
    EXPECT_TRUE(file.isEmpty());
}

TEST_F(DesktopFileFixture, testPathConstructor) {
    DesktopFile nonExistingPath("/a/b/c/d/e/f/g/h/1/2/3/4/5/6/7/8");
    EXPECT_TRUE(nonExistingPath.isEmpty());

    DesktopFile emptyFile("/dev/null");
    EXPECT_TRUE(emptyFile.isEmpty());

    DesktopFile file(DESKTOP_FILE_PATH);
    EXPECT_FALSE(file.isEmpty());
}

TEST_F(DesktopFileFixture, testStreamConstructor) {
    std::stringstream emptyString;
    DesktopFile emptyFile(emptyString);
    EXPECT_TRUE(emptyFile.isEmpty());

    std::stringstream ss;
    ss << testDesktopFile;
    DesktopFile file(ss);
    EXPECT_FALSE(file.isEmpty());

    assertIsTestDesktopFile(file);
}

TEST_F(DesktopFileFixture, testCopyConstructor) {
    DesktopFile empty;
    EXPECT_TRUE(empty == empty);
    EXPECT_FALSE(empty != empty);

    DesktopFile copyOfEmpty(empty);
    EXPECT_TRUE(empty == copyOfEmpty);
    EXPECT_FALSE(empty != copyOfEmpty);

    std::stringstream ss;
    ss << testDesktopFile;
    DesktopFile file(ss);

    DesktopFile copy(file);

    EXPECT_TRUE(file == copy);
    EXPECT_FALSE(file != copy);

    assertIsTestDesktopFile(file);
    assertIsTestDesktopFile(copy);
}

TEST_F(DesktopFileFixture, testCopyAssignmentConstructor) {
    std::stringstream ss;
    ss << testDesktopFile;
    DesktopFile file(ss);

    DesktopFile copy;
    copy = file;

    EXPECT_TRUE(file == copy);
    EXPECT_FALSE(file != copy);

    assertIsTestDesktopFile(file);
    assertIsTestDesktopFile(copy);
}

TEST_F(DesktopFileFixture, testMoveAssignmentConstructor) {
    std::stringstream ss;
    ss << testDesktopFile;
    DesktopFile file(ss);

    DesktopFile copy;
    copy = std::move(file);
    EXPECT_FALSE(copy.isEmpty());

    assertIsTestDesktopFile(copy);
}

TEST_F(DesktopFileFixture, testAddDefaultValues) {
    const auto& value = "testExecutable";

    DesktopFile file;
    file.addDefaultKeys(value);

    std::stringstream ss;

    file.save(ss);

    DesktopFileReader reader(ss);

    EXPECT_EQ(reader["Desktop Entry"]["Name"].value(), value);
    EXPECT_EQ(reader["Desktop Entry"]["Exec"].value(), value);
    EXPECT_EQ(reader["Desktop Entry"]["Icon"].value(), value);
    EXPECT_EQ(reader["Desktop Entry"]["Type"].value(), "Application");
    EXPECT_EQ(reader["Desktop Entry"]["Categories"].value(), "Utility;");
}

TEST_F(DesktopFileFixture, testAddDefaultValuesNoOverwrite) {
    const auto& value = "testExecutable";

    std::stringstream iss;
    iss << testDesktopFile;
    DesktopFile file(iss);

    file.addDefaultKeys(value);

    {
        std::stringstream oss;
        file.save(oss);

        // keys should not have been overwritten, and should still have the original values
        // however, there should be 5 keys, as the Categories one is coming from the testDesktopFile string
        assertHasTestDesktopFileKeys(oss, 5);
    }

    {
        std::stringstream oss;
        file.save(oss);

        DesktopFileReader reader(oss);
        EXPECT_EQ(reader["Desktop Entry"]["Categories"].parseStringList(), std::vector<std::string>({"Utility"}));
    }
}

TEST_F(DesktopFileFixture, testSaveToPath) {
    std::stringstream ins;
    ins << testDesktopFile;

    DesktopFile file(ins);

    EXPECT_NO_THROW(file.save("/dev/null"));
}

TEST_F(DesktopFileFixture, testSave) {
    DesktopFile file("/dev/null");
    EXPECT_NO_THROW(file.save());
}

TEST_F(DesktopFileFixture, testSaveToStream) {
    std::stringstream ins;
    ins << testDesktopFile;

    DesktopFile file(ins);

    std::stringstream outs;
    file.save(outs);

    assertHasTestDesktopFileKeys(outs);
}
