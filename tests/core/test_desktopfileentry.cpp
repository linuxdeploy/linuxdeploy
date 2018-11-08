// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>

// local headers
#include "../../src/core/desktopfileentry.h"

using boost::bad_lexical_cast;

namespace bf = boost::filesystem;

class DesktopFileEntryFixture : public ::testing::Test {
public:
    const std::string key;
    const std::string value;

protected:
    DesktopFileEntryFixture() : key("testKey"), value("testValue") {}
    
private:
    void SetUp() override {}

    void TearDown() override {}
};

TEST_F(DesktopFileEntryFixture, testDefaultConstructor) {
    DesktopFileEntry entry;
    EXPECT_TRUE(entry.isEmpty());
}

TEST_F(DesktopFileEntryFixture, testKeyValueConstructor) {
    DesktopFileEntry entry(key, value);
    EXPECT_FALSE(entry.isEmpty());
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);
}

TEST_F(DesktopFileEntryFixture, testGetters) {
    DesktopFileEntry entry(key, value);
    EXPECT_EQ(entry.key(), key);
    EXPECT_EQ(entry.value(), value);
}

TEST_F(DesktopFileEntryFixture, testEqualityAndInequalityOperators) {
    DesktopFileEntry emptyEntry;
    EXPECT_TRUE(emptyEntry == emptyEntry);
    EXPECT_FALSE(emptyEntry != emptyEntry);

    DesktopFileEntry nonEmptyEntry(key, value);
    EXPECT_NE(emptyEntry, nonEmptyEntry);

    DesktopFileEntry nonEmptyEntryWithDifferentValue(key, value + "abc");
    EXPECT_NE(nonEmptyEntry, nonEmptyEntryWithDifferentValue);
}

TEST_F(DesktopFileEntryFixture, testCopyConstructor) {
    DesktopFileEntry entry(key, value);
    EXPECT_FALSE(entry.isEmpty());

    DesktopFileEntry copy = entry;
    EXPECT_FALSE(copy.isEmpty());

    EXPECT_EQ(entry, copy);
}

TEST_F(DesktopFileEntryFixture, testCopyAssignmentConstructor) {
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

TEST_F(DesktopFileEntryFixture, testMoveAssignmentConstructor) {
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

TEST_F(DesktopFileEntryFixture, testConversionToInt) {
    DesktopFileEntry intEntry(key, "1234");
    EXPECT_EQ(intEntry.asInt(), 1234);

    DesktopFileEntry brokenValueEntry(key, "abcd");
    ASSERT_THROW(brokenValueEntry.asInt(), bad_lexical_cast);

    DesktopFileEntry emptyEntry(key, "");
    ASSERT_THROW(emptyEntry.asInt(), std::invalid_argument);
}

TEST_F(DesktopFileEntryFixture, testConversionToLong) {
    DesktopFileEntry intEntry(key, "123456789123456789");
    EXPECT_EQ(intEntry.asLong(), 123456789123456789L);

    DesktopFileEntry brokenValueEntry(key, "abcd");
    ASSERT_THROW(brokenValueEntry.asLong(), bad_lexical_cast);

    DesktopFileEntry emptyEntry(key, "");
    ASSERT_THROW(emptyEntry.asLong(), std::invalid_argument);
}

TEST_F(DesktopFileEntryFixture, testConversionToString) {
    DesktopFileEntry entry(key, value);
    auto result = static_cast<std::string>(entry);
    EXPECT_EQ(value, result);
}

TEST_F(DesktopFileEntryFixture, testParsingStringList) {
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
