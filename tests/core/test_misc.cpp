#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "gtest/gtest.h"

#include "linuxdeploy/util/util.h"
#include "test_util.h"

namespace fs = std::filesystem;
using linuxdeploy::util::which;

namespace {
    class WhichTest : public testing::Test {
    protected:
        fs::path directory;
        fs::path oldWorkingDirectory;
        std::optional<std::string> oldPath;

        void SetUp() override {
            if (const auto* path = getenv("PATH"))
                oldPath = path;
            oldWorkingDirectory = fs::current_path();
            directory = make_temporary_directory();
        }

        void TearDown() override {
            fs::current_path(oldWorkingDirectory);
            if (oldPath)
                EXPECT_EQ(setenv("PATH", oldPath->c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("PATH"), 0);
            // a directory which is not readable cannot be removed
            for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied))
                if (entry.is_directory())
                    fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add);
            fs::permissions(directory, fs::perms::owner_all, fs::perm_options::add);
            fs::remove_all(directory);
        }

        void setPath(const std::string& path) {
            ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
        }

        // writes a program, which may be executable or not
        static fs::path writeProgram(const fs::path& path, bool executable = true) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/bin/sh\n";
            if (executable)
                fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
            return path;
        }
    };

    TEST_F(WhichTest, FindProgramInPath) {
        writeProgram(directory / "first" / "other");
        const auto program = writeProgram(directory / "second" / "program");
        setPath((directory / "first").string() + ":" + (directory / "second").string());

        EXPECT_EQ(which("program"), program);
    }

    TEST_F(WhichTest, FindProgramInSearchableButUnreadableDirectory) {
        // programs can be run from a directory which can be searched, but not listed
        // root can list any directory, so this only checks the case for other users
        const auto program = writeProgram(directory / "bin" / "program");
        fs::permissions(directory / "bin", fs::perms::owner_exec);
        setPath((directory / "bin").string());

        EXPECT_EQ(which("program"), program);
    }

    TEST_F(WhichTest, SkipProgramWhichIsNotExecutable) {
        writeProgram(directory / "first" / "program", false);
        const auto program = writeProgram(directory / "second" / "program");
        setPath((directory / "first").string() + ":" + (directory / "second").string());

        EXPECT_EQ(which("program"), program);
    }

    TEST_F(WhichTest, SkipDirectoryNamedLikeProgram) {
        fs::create_directories(directory / "first" / "program");
        const auto program = writeProgram(directory / "second" / "program");
        setPath((directory / "first").string() + ":" + (directory / "second").string());

        EXPECT_EQ(which("program"), program);
    }

    TEST_F(WhichTest, SkipEmptyEntries) {
        // an empty entry stands for the current directory, which which() does not search
        writeProgram(directory / "program");
        fs::current_path(directory);
        setPath(":" + (directory / "nonexistent").string() + ":");

        EXPECT_TRUE(which("program").empty());
    }

    TEST_F(WhichTest, FindNothingIfPathIsUnset) {
        ASSERT_EQ(unsetenv("PATH"), 0);

        EXPECT_TRUE(which("sh").empty());
    }

    TEST_F(WhichTest, AcceptExecutableAbsolutePath) {
        const auto program = writeProgram(directory / "program");
        setPath((directory / "nonexistent").string());

        EXPECT_EQ(which(program.string()), program);
        EXPECT_TRUE(which(writeProgram(directory / "not-executable", false).string()).empty());
    }

    TEST_F(WhichTest, CheckRelativePathInsteadOfSearchingPath) {
        // like exec*, a name which contains a slash is relative to the current directory, even if the PATH contains it
        writeProgram(directory / "bin" / "tools" / "program");
        const auto program = writeProgram(directory / "tools" / "program");
        setPath((directory / "bin").string());
        fs::current_path(directory);

        EXPECT_EQ(which("tools/program"), "tools/program");
        EXPECT_EQ(which("./tools/program"), "./tools/program");
        ASSERT_TRUE(fs::remove(program));
        EXPECT_TRUE(which("tools/program").empty());
    }
}

namespace {
    using linuxdeploy::util::readPreloadEntries;
    using linuxdeploy::util::splitPreloadList;

    TEST(SplitPreloadListTest, SplitAtWhitespaceAndColons) {
        EXPECT_EQ(splitPreloadList(" libfakeroot.so:/opt/lib/libfoo.so\t/usr/$LIB/libbar.so::"),
                  (std::vector<std::string>{"libfakeroot.so", "/opt/lib/libfoo.so", "/usr/$LIB/libbar.so"}));
        EXPECT_TRUE(splitPreloadList("").empty());
    }

    TEST(ReadPreloadEntriesTest, ReadEntriesAsWritten) {
        const auto directory = fs::path(make_temporary_directory());
        const auto file = directory / "ld.so.preload";
        std::ofstream(file) << "# preloaded by the system\n"
                               "/usr/lib/arm-linux-gnueabihf/libarmmem-${PLATFORM}.so\n"
                               "/opt/lib/libfoo.so:/usr/$LIB/libbar.so\tlibname.so # by name\n";

        const auto entries = readPreloadEntries(file);
        fs::remove_all(directory);

        // glibc's ldd lists them as written, e.g., with their tokens
        EXPECT_EQ(entries, (std::vector<std::string>{
            "/usr/lib/arm-linux-gnueabihf/libarmmem-${PLATFORM}.so",
            "/opt/lib/libfoo.so",
            "/usr/$LIB/libbar.so",
            "libname.so",
        }));
    }

    TEST(ReadPreloadEntriesTest, AcceptMissingFile) {
        EXPECT_TRUE(readPreloadEntries("/nonexistent/ld.so.preload").empty());
    }
}
