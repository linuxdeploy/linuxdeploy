// the strip linuxdeploy runs: $STRIP if set, otherwise the one next to linuxdeploy, or the first in $PATH, see README.md
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/util/util.h"
#include "test_util.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::core::appdir;
using testing::Each;
using testing::HasSubstr;
using testing::IsEmpty;
using testing::Not;

namespace {
    class StripTest : public testing::Test {
    protected:
        fs::path directory;
        fs::path log;
        fs::path bundledStrip;
        fs::path oldWorkingDirectory;
        std::map<std::string, std::optional<std::string>> oldEnvironment;

        void SetUp() override {
            for (const auto* name : {"STRIP", "NO_STRIP", "PATH", "STRIP_TEST_LOG"}) {
                const auto* value = getenv(name);
                oldEnvironment[name] = value != nullptr ? std::optional<std::string>(value) : std::nullopt;
            }
            oldWorkingDirectory = fs::current_path();

            directory = make_temporary_directory();
            log = directory / "strip.log";
            ASSERT_EQ(setenv("STRIP_TEST_LOG", log.c_str(), 1), 0);
            ASSERT_EQ(unsetenv("STRIP"), 0);
            ASSERT_EQ(unsetenv("NO_STRIP"), 0);

            // a strip in the $PATH, which the bundled strip and $STRIP take precedence over
            writeStrip(directory / "bin" / "strip", "path");
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
            if (!bundledStrip.empty())
                fs::remove(bundledStrip);
            fs::remove_all(directory);
        }

        // writes a strip which logs its name, and exits with the given status
        static void writeStrip(const fs::path& path, const std::string& name, int status = 0) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/bin/sh\n"
                                << "echo " << name << " >> \"$STRIP_TEST_LOG\"\n"
                                << "exit " << status << "\n";
            fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
        }

        // places a strip next to the test binary, like the one bundled with linuxdeploy
        // returns false if the binary's path is unknown (it is read from /proc, which FreeBSD does not mount)
        bool writeBundledStrip() {
            const auto ownPath = linuxdeploy::util::getOwnExecutablePath();
            if (ownPath.empty())
                return false;
            bundledStrip = fs::path(ownPath).parent_path() / "strip";
            writeStrip(bundledStrip, "bundled");
            return true;
        }

        bool deploy() {
            AppDir appDir(directory / "AppDir");
            appDir.deployLibrary(SIMPLE_LIBRARY_NO_DEPENDENCIES_PATH);
            return appDir.executeDeferredOperations();
        }

        // runs linuxdeploy like a user would, which reports errors instead of aborting
        linuxdeploy::subprocess::subprocess_result runLinuxdeploy() const {
            return linuxdeploy::subprocess::subprocess({LINUXDEPLOY_PATH, "--appdir", (directory / "AppDir").string(),
                                                        "--executable", SIMPLE_EXECUTABLE_PATH}).run();
        }

        // the names of the strips which were called, once per call
        std::vector<std::string> calls() const {
            std::vector<std::string> names;
            std::ifstream file(log);
            for (std::string line; std::getline(file, line);)
                names.push_back(line);
            return names;
        }
    };

    TEST_F(StripTest, UsePathByDefault) {
        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("path")));
    }

    TEST_F(StripTest, PreferBundledToPath) {
        if (!writeBundledStrip())
            GTEST_SKIP() << "the path of the test binary is unknown";

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("bundled")));
    }

    TEST_F(StripTest, PreferStripToBundled) {
        writeBundledStrip();
        writeStrip(directory / "custom" / "strip", "custom");
        ASSERT_EQ(setenv("STRIP", (directory / "custom" / "strip").c_str(), 1), 0);

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("custom")));
    }

    TEST_F(StripTest, IgnoreEmptyStrip) {
        if (!writeBundledStrip())
            GTEST_SKIP() << "the path of the test binary is unknown";
        ASSERT_EQ(setenv("STRIP", "", 1), 0);

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("bundled")));
    }

    TEST_F(StripTest, SearchStripNameInPath) {
        writeBundledStrip();
        writeStrip(directory / "bin" / "custom-strip", "custom");
        ASSERT_EQ(setenv("STRIP", "custom-strip", 1), 0);

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("custom")));
    }

    TEST_F(StripTest, ResolveRelativeStripPath) {
        writeBundledStrip();
        writeStrip(directory / "tools" / "strip", "relative");
        ASSERT_EQ(setenv("STRIP", "./tools/strip", 1), 0);
        fs::current_path(directory);

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("relative")));
    }

    TEST_F(StripTest, FailWithoutFallbackIfStripIsMissing) {
        writeBundledStrip();
        ASSERT_EQ(setenv("STRIP", (directory / "nonexistent").c_str(), 1), 0);

        EXPECT_FALSE(deploy());
        EXPECT_THAT(calls(), IsEmpty());
    }

    TEST_F(StripTest, FailWithoutFallbackIfStripFails) {
        writeBundledStrip();
        writeStrip(directory / "custom" / "strip", "custom", 1);
        ASSERT_EQ(setenv("STRIP", (directory / "custom" / "strip").c_str(), 1), 0);

        EXPECT_FALSE(deploy());
        EXPECT_THAT(calls(), Not(IsEmpty()));
        EXPECT_THAT(calls(), Each(std::string("custom")));
    }

    TEST_F(StripTest, SkipStripIfDisabled) {
        writeBundledStrip();
        ASSERT_EQ(setenv("NO_STRIP", "1", 1), 0);
        ASSERT_EQ(setenv("STRIP", (directory / "nonexistent").c_str(), 1), 0);

        ASSERT_TRUE(deploy());
        EXPECT_THAT(calls(), IsEmpty());
    }

    // linuxdeploy reports which strip failed and exits with an error
    TEST_F(StripTest, ReportMissingStrip) {
        ASSERT_EQ(setenv("STRIP", (directory / "nonexistent").c_str(), 1), 0);

        const auto result = runLinuxdeploy();
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exit_code(), 1);
        EXPECT_THAT(result.stdout_string(),
                    HasSubstr("Failed to find or run strip \"" + (directory / "nonexistent").string() + "\":"));
    }

    // a strip which runs, but fails, is reported with how it ended
    TEST_F(StripTest, ReportFailingStrip) {
        writeStrip(directory / "custom" / "strip", "custom", 3);
        ASSERT_EQ(setenv("STRIP", (directory / "custom" / "strip").c_str(), 1), 0);

        const auto result = runLinuxdeploy();
        EXPECT_EQ(result.exit_code(), 1);
        EXPECT_THAT(result.stdout_string(), HasSubstr("Strip call failed (exited with code 3):"));
    }
}
