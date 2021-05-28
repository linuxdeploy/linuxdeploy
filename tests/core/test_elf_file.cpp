#include "gtest/gtest.h"

#include "linuxdeploy/core/elf_file.h"

using namespace std;
using namespace linuxdeploy::core;
namespace bf = boost::filesystem;

using namespace std;
using namespace linuxdeploy::core::elf_file;

namespace LinuxDeployTest {
    class ElfFileTest : public ::testing::Test {};

    TEST_F(ElfFileTest, checkIsDebugSymbolsFile) {
        ElfFile debugSymbolsFile(SIMPLE_LIBRARY_DEBUG_PATH);
        EXPECT_TRUE(debugSymbolsFile.isDebugSymbolsFile());

        ElfFile regularLibraryFile(SIMPLE_LIBRARY_PATH);
        EXPECT_FALSE(regularLibraryFile.isDebugSymbolsFile());
    }

    TEST_F(ElfFileTest, checkIsDynamicallyLinked) {
        ElfFile regularLibraryFile(SIMPLE_EXECUTABLE_PATH);
        EXPECT_TRUE(regularLibraryFile.isDynamicallyLinked());

        ElfFile staticLibraryFile(SIMPLE_EXECUTABLE_STATIC_PATH);
        EXPECT_FALSE(staticLibraryFile.isDynamicallyLinked());
    }
}
