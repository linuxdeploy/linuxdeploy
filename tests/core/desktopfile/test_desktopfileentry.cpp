// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

// local headers
#include "linuxdeploy/core/desktopfile/desktopfileentry.h"

using boost::bad_lexical_cast;
using namespace linuxdeploy::core::desktopfile;

namespace bf = boost::filesystem;

class DesktopFileEntryTest : public ::testing::Test {
public:
    const std::string key;
    const std::string value;

protected:
    DesktopFileEntryTest() : key("testKey"), value("testValue") {}
    
private:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(DesktopFileEntryTest, testDefaultConstructor) {
    DesktopFileEntry entry;
    EXPECT_TRUE(entry.isEmpty());
}

TEST_F(DesktopFileEntryTest, testKeyValueConstructor) {
    DesktopFileEntry entry(key, value);
    EXPECT_FALSE(entry.isEmpty());
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);
}

TEST_F(DesktopFileEntryTest, testGetters) {
    DesktopFileEntry entry(key, value);
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);
}

TEST_F(DesktopFileEntryTest, testEqualityAndInequalityOperators) {
    DesktopFileEntry emptyEntry;
    EXPECT_TRUE(emptyEntry == emptyEntry);
    EXPECT_FALSE(emptyEntry != emptyEntry);

    DesktopFileEntry nonEmptyEntry(key, value);
    EXPECT_NE(emptyEntry, nonEmptyEntry);

    DesktopFileEntry nonEmptyEntryWithDifferentValue(key, value + "abc");
    EXPECT_NE(nonEmptyEntry, nonEmptyEntryWithDifferentValue);
}

TEST_F(DesktopFileEntryTest, testCopyConstructor) {
    DesktopFileEntry entry(key, value);
    EXPECT_FALSE(entry.isEmpty());

    DesktopFileEntry copy = entry;
    EXPECT_FALSE(copy.isEmpty());

    EXPECT_EQ(entry, copy);
}

TEST_F(DesktopFileEntryTest, testCopyAssignmentConstructor) {
    DesktopFileEntry entry;
    EXPECT_TRUE(entry.isEmpty());

    DesktopFileEntry otherEntry(key, value);
    EXPECT_FALSE(otherEntry.isEmpty());

    entry = otherEntry;
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);

    // test self-assignment
    entry = entry;
}

TEST_F(DesktopFileEntryTest, testMoveAssignmentConstructor) {
    DesktopFileEntry entry;
    EXPECT_TRUE(entry.isEmpty());

    DesktopFileEntry otherEntry(key, value);
    EXPECT_FALSE(otherEntry.isEmpty());

    entry = std::move(otherEntry);
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);

    // test self-assignment
    entry = std::move(entry);
}

TEST_F(DesktopFileEntryTest, testConversionToInt) {
    DesktopFileEntry intEntry(key, "1234");
    EXPECT_EQ(intEntry.asInt(), 1234);

    DesktopFileEntry brokenValueEntry(key, "abcd");
    ASSERT_THROW(brokenValueEntry.asInt(), bad_lexical_cast);

    DesktopFileEntry emptyEntry(key, "");
    ASSERT_THROW(emptyEntry.asInt(), std::invalid_argument);
}

TEST_F(DesktopFileEntryTest, testConversionToLong) {
    DesktopFileEntry intEntry(key, "123456789123456789");
    EXPECT_EQ(intEntry.asLong(), 123456789123456789L);

    DesktopFileEntry brokenValueEntry(key, "abcd");
    ASSERT_THROW(brokenValueEntry.asLong(), bad_lexical_cast);

    DesktopFileEntry emptyEntry(key, "");
    ASSERT_THROW(emptyEntry.asLong(), std::invalid_argument);
}

TEST_F(DesktopFileEntryTest, testConversionToDouble) {
    DesktopFileEntry doubleEntry(key, "1.234567");
    EXPECT_NEAR(doubleEntry.asDouble(), 1.234567, 0.00000001);

    DesktopFileEntry brokenValueEntry(key, "abcd");
    ASSERT_THROW(brokenValueEntry.asDouble(), bad_lexical_cast);

    DesktopFileEntry emptyEntry(key, "");
    ASSERT_THROW(emptyEntry.asDouble(), std::invalid_argument);
}

TEST_F(DesktopFileEntryTest, testParsingStringList) {
    DesktopFileEntry emptyEntry(key, "");
    EXPECT_EQ(emptyEntry.parseStringList(), std::vector<std::string>({}));

    DesktopFileEntry nonListEntry(key, value);
    EXPECT_EQ(nonListEntry.parseStringList(), std::vector<std::string>({value}));

    DesktopFileEntry listEntry(key, "val1;val2;");
    EXPECT_EQ(listEntry.parseStringList(), std::vector<std::string>({"val1", "val2"}));

    DesktopFileEntry listEntryWithoutTrailingSemicolon(key, "val1;val2");
    EXPECT_EQ(listEntry.parseStringList(), std::vector<std::string>({"val1", "val2"}));

    DesktopFileEntry listEntryWithEmptyItems(key, "val1;;;val2;;");
    EXPECT_EQ(listEntry.parseStringList(), std::vector<std::string>({"val1", "val2"}));
}
