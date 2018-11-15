// library headers
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>

// local headers
#include "linuxdeploy/core/desktopfile/desktopfile.h"
#include "linuxdeploy/core/desktopfile/exceptions.h"

using boost::bad_lexical_cast;
using namespace linuxdeploy::core::desktopfile;

namespace bf = boost::filesystem;

class DesktopFileConformanceTest : public ::testing::Test {
private:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(DesktopFileConformanceTest, testBasicFormatInvalidKeyCharacters) {
    // test conformance with a couple of invalid values
    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "no spaces in key=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }

    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "UmlautTestÄöÜ=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }

    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "NoUnderscores_=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }
}

TEST_F(DesktopFileConformanceTest, testBasicFormatValidKeyCharacters) {
    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "TestKey=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }

    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "4242trolol0=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }

    {
        std::stringstream ss;
        ss << "[Desktop Entry]" << std::endl
           << "----=foo" << std::endl;

        ASSERT_THROW(DesktopFile file(ss), ParseError);
    }
}
