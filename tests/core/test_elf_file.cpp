#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/elf_file.h"

using namespace std;
using namespace linuxdeploy::core;

namespace fs = std::filesystem;

using namespace std;
using namespace linuxdeploy::core::elf_file;

namespace {
    typedef std::function<void()> callback_t;

    template<typename T>
    void expectThrowMessageHasSubstr(const callback_t& callback, const char* message) {
        EXPECT_THAT(callback, ::testing::ThrowsMessage<ElfFileParseError>(::testing::HasSubstr(message)));
    }

    void expectElfFileConstructorThrowMessage(const char* path, const char* message) {
        return expectThrowMessageHasSubstr<ElfFileParseError>([=]() { ElfFile{path}; }, message);
    }

    void expectThrowsElfFileErrorInvalidElfHeader(const char* path) {
        expectElfFileConstructorThrowMessage(path, "Invalid magic bytes in file header");
    }

    void expectThrowsElfFileErrorFileNotFound(const char* path) {
        expectElfFileConstructorThrowMessage(path, "No such file or directory: ");
    }
}

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

        // shared libraries have no program interpreter,
        // but they need other libraries
        ElfFile sharedLibraryFile(SIMPLE_LIBRARY_PATH);
        EXPECT_TRUE(sharedLibraryFile.isDynamicallyLinked());

        // like static-pie executables, this library has neither a program
        // interpreter nor needs other libraries, but it is not a PIE
        ElfFile noDependenciesLibraryFile(SIMPLE_LIBRARY_NO_DEPENDENCIES_PATH);
        EXPECT_TRUE(noDependenciesLibraryFile.isDynamicallyLinked());
    }

    TEST_F(ElfFileTest, checkIsDynamicallyLinkedStaticPie) {
#ifdef SIMPLE_EXECUTABLE_STATIC_PIE_PATH
        ElfFile staticPieFile(SIMPLE_EXECUTABLE_STATIC_PIE_PATH);
        EXPECT_FALSE(staticPieFile.isDynamicallyLinked());
#else
        GTEST_SKIP() << "toolchain cannot link static-pie executables";
#endif
    }

    TEST_F(ElfFileTest, checkIsDynamicallyLinkedNoInterpreterPie) {
#ifdef SIMPLE_EXECUTABLE_NO_INTERPRETER_PIE_PATH
        // like static-pie executables, this PIE has no program interpreter,
        // but it needs other libraries
        ElfFile noInterpreterPieFile(SIMPLE_EXECUTABLE_NO_INTERPRETER_PIE_PATH);
        EXPECT_TRUE(noInterpreterPieFile.isDynamicallyLinked());
#else
        GTEST_SKIP() << "toolchain cannot link executables without a dynamic linker";
#endif
    }

    TEST_F(ElfFileTest, checkInvalidElfHeaderOnEmptyFile) {
        expectThrowsElfFileErrorInvalidElfHeader("/dev/null");
    }

    TEST_F(ElfFileTest, checkInvalidElfHeaderOnRandomFiles) {
        expectThrowsElfFileErrorInvalidElfHeader(SIMPLE_DESKTOP_ENTRY_PATH);
        expectThrowsElfFileErrorInvalidElfHeader(SIMPLE_ICON_PATH);
        expectThrowsElfFileErrorInvalidElfHeader(SIMPLE_FILE_PATH);
    }

    TEST_F(ElfFileTest, checkFileNotFound) {
        expectThrowsElfFileErrorFileNotFound("/abc/def/ghi/jkl/mno/pqr/stu/vwx/yz");
    }
}
