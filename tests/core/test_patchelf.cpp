// the patchelf linuxdeploy runs: $PATCHELF if set, otherwise the one next to linuxdeploy, or the first in $PATH, see
// README.md
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <fcntl.h>
#include <sys/resource.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/elf_file.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/util/util.h"
#include "test_util.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::core::elf_file;
using testing::HasSubstr;

namespace {
    class PatchelfTest : public testing::Test {
    protected:
        fs::path directory;
        fs::path bundledPatchelf;
        fs::path oldWorkingDirectory;
        std::map<std::string, std::optional<std::string>> oldEnvironment;

        void SetUp() override {
            for (const auto* name : {"PATCHELF", "PATH"}) {
                const auto* value = getenv(name);
                oldEnvironment[name] = value != nullptr ? std::optional<std::string>(value) : std::nullopt;
            }
            oldWorkingDirectory = fs::current_path();

            directory = make_temporary_directory();
            ASSERT_EQ(unsetenv("PATCHELF"), 0);

            // a patchelf in the $PATH, which the bundled patchelf and $PATCHELF take precedence over
            writePatchelf(directory / "bin" / "patchelf", "path");
            const auto path = (directory / "bin").string() + ":" + oldEnvironment["PATH"].value_or("");
            ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
        }

        void TearDown() override {
            fs::current_path(oldWorkingDirectory);
            for (const auto& [name, value] : oldEnvironment) {
                if (value)
                    EXPECT_EQ(setenv(name.c_str(), value->c_str(), 1), 0);
                else
                    EXPECT_EQ(unsetenv(name.c_str()), 0);
            }
            if (!bundledPatchelf.empty())
                fs::remove(bundledPatchelf);
            fs::remove_all(directory);
        }

        // writes a patchelf which prints its name as the rpath
        static void writePatchelf(const fs::path& path, const std::string& name) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/bin/sh\n"
                                << "echo " << name << "\n";
            fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
        }

        // runs linuxdeploy like a user would, which reports errors instead of aborting
        linuxdeploy::subprocess::subprocess_result runLinuxdeploy() const {
            return linuxdeploy::subprocess::subprocess({LINUXDEPLOY_PATH, "--appdir", (directory / "AppDir").string(),
                                                        "--executable", SIMPLE_EXECUTABLE_PATH}).run();
        }

        // places a patchelf next to the test binary, like the one bundled with linuxdeploy
        // returns false if the binary's path is unknown (it is read from /proc, which FreeBSD does not mount)
        bool writeBundledPatchelf() {
            const auto ownPath = linuxdeploy::util::getOwnExecutablePath();
            if (ownPath.empty())
                return false;
            bundledPatchelf = fs::path(ownPath).parent_path() / "patchelf";
            writePatchelf(bundledPatchelf, "bundled");
            return true;
        }

        // writes a patchelf which fails
        static void writeFailingPatchelf(const fs::path& path) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/bin/sh\nexit 3\n";
            fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
        }

        // the name of the patchelf which was run
        static std::string calledPatchelf() {
            return ElfFile(SIMPLE_LIBRARY_PATH).getRPath();
        }
    };

    TEST_F(PatchelfTest, UsePathByDefault) {
        EXPECT_EQ(calledPatchelf(), "path");
    }

    TEST_F(PatchelfTest, PreferBundledToPath) {
        if (!writeBundledPatchelf())
            GTEST_SKIP() << "the path of the test binary is unknown";

        EXPECT_EQ(calledPatchelf(), "bundled");
    }

    TEST_F(PatchelfTest, PreferPatchelfToBundled) {
        writeBundledPatchelf();
        writePatchelf(directory / "custom" / "patchelf", "custom");
        ASSERT_EQ(setenv("PATCHELF", (directory / "custom" / "patchelf").c_str(), 1), 0);

        EXPECT_EQ(calledPatchelf(), "custom");
    }

    TEST_F(PatchelfTest, IgnoreEmptyPatchelf) {
        if (!writeBundledPatchelf())
            GTEST_SKIP() << "the path of the test binary is unknown";
        ASSERT_EQ(setenv("PATCHELF", "", 1), 0);

        EXPECT_EQ(calledPatchelf(), "bundled");
    }

    TEST_F(PatchelfTest, SearchPatchelfNameInPath) {
        writeBundledPatchelf();
        writePatchelf(directory / "bin" / "custom-patchelf", "custom");
        ASSERT_EQ(setenv("PATCHELF", "custom-patchelf", 1), 0);

        EXPECT_EQ(calledPatchelf(), "custom");
    }

    TEST_F(PatchelfTest, ResolveRelativePatchelfPath) {
        writeBundledPatchelf();
        writePatchelf(directory / "tools" / "patchelf", "relative");
        ASSERT_EQ(setenv("PATCHELF", "./tools/patchelf", 1), 0);
        fs::current_path(directory);

        EXPECT_EQ(calledPatchelf(), "relative");
    }

    TEST_F(PatchelfTest, StopWithoutFallbackIfPatchelfIsMissing) {
        writeBundledPatchelf();
        ASSERT_EQ(setenv("PATCHELF", (directory / "nonexistent").c_str(), 1), 0);

        EXPECT_THAT([] { calledPatchelf(); },
                    testing::ThrowsMessage<std::runtime_error>(HasSubstr("Failed to find or run patchelf")));
        EXPECT_THAT([] { ElfFile(SIMPLE_LIBRARY_PATH).setRPath("$ORIGIN"); },
                    testing::ThrowsMessage<std::runtime_error>(HasSubstr("Failed to find or run patchelf")));
    }

    TEST_F(PatchelfTest, StopWithoutFallbackIfPatchelfCannotBeRun) {
        writeBundledPatchelf();
        writePatchelf(directory / "custom" / "patchelf", "custom");
        fs::permissions(directory / "custom" / "patchelf", fs::perms::owner_exec, fs::perm_options::remove);
        ASSERT_EQ(setenv("PATCHELF", (directory / "custom" / "patchelf").c_str(), 1), 0);

        EXPECT_THAT([] { calledPatchelf(); },
                    testing::ThrowsMessage<std::runtime_error>(HasSubstr("Failed to find or run patchelf")));
    }

    TEST_F(PatchelfTest, StopIfPatchelfFailsToReadRpath) {
        // an empty rpath would replace the existing one, so patchelf errors must throw
        writeFailingPatchelf(directory / "custom" / "patchelf");
        ASSERT_EQ(setenv("PATCHELF", (directory / "custom" / "patchelf").c_str(), 1), 0);

        EXPECT_THAT([] { calledPatchelf(); },
                    testing::ThrowsMessage<std::runtime_error>(HasSubstr("Call to patchelf failed (exited with code 3)")));
    }

    TEST_F(PatchelfTest, ReadNoRpathFromFilesWithoutDynamicSection) {
        // patchelf fails on them, so it is not run
        writeFailingPatchelf(directory / "custom" / "patchelf");
        ASSERT_EQ(setenv("PATCHELF", (directory / "custom" / "patchelf").c_str(), 1), 0);

        EXPECT_EQ(ElfFile(SIMPLE_EXECUTABLE_STATIC_PATH).getRPath(), "");
        EXPECT_EQ(ElfFile(SIMPLE_LIBRARY_DEBUG_PATH).getRPath(), "");
    }

    TEST_F(PatchelfTest, StopIfPatchelfCannotBeStarted) {
        // e.g., without free file descriptors for its pipes, the process cannot be started
        ElfFile file(SIMPLE_LIBRARY_PATH);

        rlimit oldLimit{};
        ASSERT_EQ(getrlimit(RLIMIT_NOFILE, &oldLimit), 0);

        // the lowest limit below which no descriptor is free
        rlim_t limit = 0;
        while (fcntl(static_cast<int>(limit), F_GETFD) != -1)
            ++limit;
        ASSERT_LT(limit, oldLimit.rlim_cur);

        rlimit lowLimit = oldLimit;
        lowLimit.rlim_cur = limit;
        ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &lowLimit), 0);
        bool threw = false;
        try {
            file.getRPath();
        } catch (const std::exception&) {
            threw = true;
        }
        ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &oldLimit), 0);

        EXPECT_TRUE(threw);
    }

    TEST_F(PatchelfTest, SearchDefaultPathIfPathIsUnset) {
        // like exec*, fall back to the C library's default search path, which may not contain patchelf
        ASSERT_EQ(unsetenv("PATH"), 0);

        try {
            calledPatchelf();
        } catch (const std::runtime_error& error) {
            EXPECT_THAT(error.what(), HasSubstr("Failed to find or run patchelf"));
        }
    }

    // linuxdeploy reports which patchelf failed and exits with an error instead of aborting
    TEST_F(PatchelfTest, ReportMissingPatchelf) {
        ASSERT_EQ(setenv("PATCHELF", (directory / "nonexistent").c_str(), 1), 0);

        const auto result = runLinuxdeploy();
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exit_code(), 1);
        EXPECT_THAT(result.stdout_string(),
                    HasSubstr("Failed to find or run patchelf \"" + (directory / "nonexistent").string() + "\":"));
    }

    // a patchelf which runs, but fails, is reported with how it ended
    TEST_F(PatchelfTest, ReportFailingPatchelf) {
        writeFailingPatchelf(directory / "custom" / "patchelf");
        ASSERT_EQ(setenv("PATCHELF", (directory / "custom" / "patchelf").c_str(), 1), 0);

        const auto result = runLinuxdeploy();
        EXPECT_EQ(result.exit_code(), 1);
        EXPECT_THAT(result.stdout_string(), HasSubstr("Call to patchelf failed (exited with code 3):"));
    }
}
