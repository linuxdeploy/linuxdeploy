// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "../../src/core/desktopfile/desktopfilewriter.h"
#include "../../src/core/desktopfile/desktopfilereader.h"
#include "linuxdeploy/core/desktopfile/exceptions.h"

using namespace linuxdeploy::core::desktopfile;
namespace bf = boost::filesystem;

class DesktopFileWriterFixture : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DesktopFileWriterFixture, testDefaultConstructor) {
    DesktopFileWriter writer;
}

TEST_F(DesktopFileWriterFixture, testDataConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);
}

TEST_F(DesktopFileWriterFixture, testCopyConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter copy(writer);
    EXPECT_EQ(copy, writer);
}

TEST_F(DesktopFileWriterFixture, testCopyAssignmentConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter;
    otherWriter = writer;
    EXPECT_EQ(writer, otherWriter);
}


TEST_F(DesktopFileWriterFixture, testEqualityAndInequalityOperators) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter(data);

    EXPECT_TRUE(writer == otherWriter);
    EXPECT_FALSE(writer != otherWriter);
}

TEST_F(DesktopFileWriterFixture, testMoveAssignmentConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter;
    otherWriter = std::move(writer);
}

TEST_F(DesktopFileWriterFixture, testSaveToPath) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    writer.save("/dev/null");
}

TEST_F(DesktopFileWriterFixture, testSaveToInvalidPath) {
    DesktopFileWriter writer;
    ASSERT_THROW(writer.save("/a/b/c/d/e/f/g/h/1/2/3/4/5/6/7/8"), IOError);
}

TEST_F(DesktopFileWriterFixture, testSaveToStream) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    std::stringstream ss;
    writer.save(ss);
}

TEST_F(DesktopFileWriterFixture, testDataGetter) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    EXPECT_EQ(data, writer.data());
}

TEST_F(DesktopFileWriterFixture, testSerialization) {
    DesktopFile::section_t section = {
        {"Exec", DesktopFileEntry("Exec", "exec")},
        {"Name", DesktopFileEntry("Name", "name")},
    };

    DesktopFile::sections_t data = {
        {"Desktop File", section},
    };

    DesktopFileWriter writer(data);
    EXPECT_EQ(data, writer.data());

    std::stringstream ss;
    writer.save(ss);

    DesktopFileReader reader(ss);
    EXPECT_EQ(reader["Desktop File"], section);
}
