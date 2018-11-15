// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "../../src/core/desktopfile/desktopfilewriter.h"
#include "../../src/core/desktopfile/desktopfilereader.h"
#include "linuxdeploy/core/desktopfile/exceptions.h"

using namespace linuxdeploy::core::desktopfile;
namespace bf = boost::filesystem;

class DesktopFileWriterTest : public ::testing::Test {
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DesktopFileWriterTest, testDefaultConstructor) {
    DesktopFileWriter writer;
}

TEST_F(DesktopFileWriterTest, testDataConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);
}

TEST_F(DesktopFileWriterTest, testCopyConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter copy(writer);
    EXPECT_EQ(copy, writer);
}

TEST_F(DesktopFileWriterTest, testCopyAssignmentConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter;
    otherWriter = writer;
    EXPECT_EQ(writer, otherWriter);
}


TEST_F(DesktopFileWriterTest, testEqualityAndInequalityOperators) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter(data);

    EXPECT_TRUE(writer == otherWriter);
    EXPECT_FALSE(writer != otherWriter);
}

TEST_F(DesktopFileWriterTest, testMoveAssignmentConstructor) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    DesktopFileWriter otherWriter;
    otherWriter = std::move(writer);
}

TEST_F(DesktopFileWriterTest, testSaveToPath) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    writer.save("/dev/null");
}

TEST_F(DesktopFileWriterTest, testSaveToInvalidPath) {
    DesktopFileWriter writer;
    ASSERT_THROW(writer.save("/a/b/c/d/e/f/g/h/1/2/3/4/5/6/7/8"), IOError);
}

TEST_F(DesktopFileWriterTest, testSaveToStream) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    std::stringstream ss;
    writer.save(ss);
}

TEST_F(DesktopFileWriterTest, testDataGetter) {
    DesktopFile::sections_t data;
    DesktopFileWriter writer(data);

    EXPECT_EQ(data, writer.data());
}

TEST_F(DesktopFileWriterTest, testSerialization) {
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
