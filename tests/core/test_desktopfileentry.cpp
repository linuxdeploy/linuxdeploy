// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "../../src/core/desktopfileentry.h"

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
