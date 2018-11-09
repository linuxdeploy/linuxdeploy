// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "../../src/core/desktopfilereader.h"

using namespace linuxdeploy::core::desktopfile;
namespace bf = boost::filesystem;

class DesktopFileReaderFixture : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DesktopFileReaderFixture, testDefaultConstructor) {
    DesktopFileReader reader;
    EXPECT_TRUE(reader.isEmpty());
}

TEST_F(DesktopFileReaderFixture, testPathConstructor) {
    bf::path path = "/dev/null";

    DesktopFileReader reader(path);
    EXPECT_TRUE(reader.isEmpty());

    ASSERT_THROW(DesktopFileReader("/no/such/file/or/directory"), std::invalid_argument);
}

TEST_F(DesktopFileReaderFixture, testStreamConstructor) {
    std::stringstream ss;
    DesktopFileReader reader(ss);

    EXPECT_TRUE(reader.isEmpty());
}

TEST_F(DesktopFileReaderFixture, testPathConstructorWithEmptyPath) {
    ASSERT_THROW(DesktopFileReader(""), std::invalid_argument);
}

TEST_F(DesktopFileReaderFixture, testPathConstructorWithNonExistingPath) {
    ASSERT_THROW(DesktopFileReader("/no/such/path/42"), std::invalid_argument);
}

TEST_F(DesktopFileReaderFixture, testEqualityAndInequalityOperators) {
    DesktopFileReader emptyReader;
    EXPECT_TRUE(emptyReader == emptyReader);
    EXPECT_FALSE(emptyReader != emptyReader);
}

TEST_F(DesktopFileReaderFixture, testCopyConstructor) {
    bf::path path = "/dev/null";

    DesktopFileReader reader(path);
    EXPECT_TRUE(reader.isEmpty());

    DesktopFileReader copy = reader;
    EXPECT_TRUE(copy.isEmpty());

    EXPECT_EQ(reader, copy);
}

TEST_F(DesktopFileReaderFixture, testCopyAssignmentConstructor) {
    bf::path path = "/dev/null";

    DesktopFileReader reader;
    EXPECT_TRUE(reader.isEmpty());

    DesktopFileReader otherReader(path);
    EXPECT_TRUE(otherReader.isEmpty());

    reader = otherReader;
    EXPECT_EQ(reader.path(), path);

    // test self-assignment
    reader = reader;
}

TEST_F(DesktopFileReaderFixture, testMoveAssignmentConstructor) {
    bf::path path = "/dev/null";

    DesktopFileReader reader;
    EXPECT_TRUE(reader.isEmpty());

    DesktopFileReader otherReader(path);
    EXPECT_TRUE(otherReader.isEmpty());

    reader = std::move(otherReader);
    EXPECT_EQ(reader.path(), path);

    // test self-assignment
    reader = std::move(reader);
}

TEST_F(DesktopFileReaderFixture, testParseSimpleDesktopFile) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl
       << "Version=1.0" << std::endl
       << "Name=name" << std::endl
       << "Exec=exec" << std::endl
       << "Icon=icon" << std::endl
       << "Type=Application" << std::endl
       << "Categories=Utility;Multimedia;" << std::endl;

    DesktopFileReader reader;
    reader = DesktopFileReader(ss);

    auto section = reader["Desktop File"];
    EXPECT_FALSE(section.empty());

    EXPECT_NEAR(section["Version"].asDouble(), 1.0f, 0.000001);
    EXPECT_EQ(section["Name"].value(), "name");
    EXPECT_EQ(section["Exec"].value(), "exec");
    EXPECT_EQ(section["Icon"].value(), "icon");
    EXPECT_EQ(section["Type"].value(), "Application");
    EXPECT_EQ(section["Name"].value(), "name");
    EXPECT_EQ(section["Categories"].parseStringList(), std::vector<std::string>({"Utility", "Multimedia"}));
}

TEST_F(DesktopFileReaderFixture, testParseFileGetNonExistingSection) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl;

    DesktopFileReader reader;
    reader = DesktopFileReader(ss);

    ASSERT_THROW(reader["Non-existing Section"], std::range_error);
}

TEST_F(DesktopFileReaderFixture, testParseFileMissingSectionHeader) {
    std::stringstream ss;
    ss << "Name=name" << std::endl
       << "Exec=exec" << std::endl
       << "Icon=icon" << std::endl
       << "Type=Application" << std::endl
       << "Categories=Utility;Multimedia;" << std::endl;

    DesktopFileReader reader;
    ASSERT_THROW(reader = DesktopFileReader(ss), std::invalid_argument);
}

TEST_F(DesktopFileReaderFixture, testParseFileEmptyKey) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl
       << "=name" << std::endl
       << "Exec=exec" << std::endl
       << "Icon=icon" << std::endl
       << "Type=Application" << std::endl
       << "Categories=Utility;Multimedia;" << std::endl;

    DesktopFileReader reader;
    ASSERT_THROW(reader = DesktopFileReader(ss), std::invalid_argument);
}

TEST_F(DesktopFileReaderFixture, testParseFileWithLeadingAndTrailingWhitespaceInLines) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl
       << "Name= name" << std::endl
       << "Exec =exec" << std::endl;

    DesktopFileReader reader(ss);

    auto section = reader["Desktop File"];
    EXPECT_FALSE(section.empty());

    EXPECT_EQ(section["Name"].value(), "name");
    EXPECT_EQ(section["Exec"].value(), "exec");
}

TEST_F(DesktopFileReaderFixture, testDataGetter) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl
       << "Name= name" << std::endl
       << "Exec =exec" << std::endl;

    DesktopFileReader reader(ss);

    auto section = reader["Desktop File"];
    EXPECT_FALSE(section.empty());

    auto data = reader.data();

    auto expected = DesktopFile::section_t({
        {"Name", DesktopFileEntry("Name", "name")},
        {"Exec", DesktopFileEntry("Exec", "exec")},
    });

    EXPECT_EQ(data["Desktop File"], expected);
}

TEST_F(DesktopFileReaderFixture, testParseLinesWithMultipleSpaces) {
    std::stringstream ss;
    ss << "[Desktop File]" << std::endl
       << "Name= What a great  name    " << std::endl;

    DesktopFileReader reader(ss);

    auto section = reader["Desktop File"];
    EXPECT_FALSE(section.empty());

    EXPECT_EQ(section["Name"].value(), "What a great  name");
}
