// plugin discovery, which linuxdeploy runs on every start
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <unistd.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/subprocess/subprocess.h"
#include "test_util.h"

namespace fs = std::filesystem;
using testing::HasSubstr;

namespace {
    class PluginTest : public testing::Test {
    protected:
        fs::path directory;
        std::optional<std::string> oldPath;

        void SetUp() override {
            if (const auto* path = getenv("PATH"))
                oldPath = path;
            directory = make_temporary_directory();
        }

        void TearDown() override {
            if (oldPath)
                EXPECT_EQ(setenv("PATH", oldPath->c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("PATH"), 0);
            // a directory which is not readable cannot be removed
            // a test's symlink may point to itself, which only the overload with an error code tolerates
            std::error_code ec;
            for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied))
                if (entry.is_directory(ec))
                    fs::permissions(entry.path(), fs::perms::owner_all, fs::perm_options::add);
            fs::permissions(directory, fs::perms::owner_all, fs::perm_options::add);
            fs::remove_all(directory);
        }

        // only the test's directory, so that plugins installed on the machine are not queried
        // the test plugin's interpreter is an absolute path, so it needs nothing from $PATH
        static void setPath(const fs::path& path) {
            ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
        }

        static linuxdeploy::subprocess::subprocess_result listPlugins() {
            return linuxdeploy::subprocess::subprocess({LINUXDEPLOY_PATH, "--list-plugins"}).run();
        }
    };

    TEST_F(PluginTest, SkipDirectoryWhichCannotBeListed) {
        // programs can be run from a directory which can be searched, but not listed
        // root can list any directory, so this only checks the case for other users
        if (geteuid() == 0)
            GTEST_SKIP() << "root can list any directory";
        const auto unreadable = directory / "unreadable";
        fs::create_directory(unreadable);
        fs::permissions(unreadable, fs::perms::owner_exec);
        setPath(unreadable);

        const auto result = listPlugins();
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exit_code(), 0) << result.stdout_string() << result.stderr_string();
    }

    TEST_F(PluginTest, SkipPluginWhoseQueryFails) {
        const auto plugin = directory / "bin" / "linuxdeploy-plugin-broken";
        fs::create_directories(plugin.parent_path());
        std::ofstream(plugin) << "#!/bin/sh\nexit 1\n";
        fs::permissions(plugin, fs::perms::owner_exec, fs::perm_options::add);
        setPath(plugin.parent_path());

        const auto result = listPlugins();
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exit_code(), 0) << result.stdout_string() << result.stderr_string();
        EXPECT_THAT(result.stdout_string(), HasSubstr("Could not load plugin"));
        EXPECT_THAT(result.stdout_string(), HasSubstr(plugin.string()));
    }

    TEST_F(PluginTest, StopOnErrorsWhichAffectAnyProgram) {
        // too few file descriptors for the pipes to query the plugin affect any program
        const auto plugin = directory / "bin" / "linuxdeploy-plugin-queried";
        fs::create_directories(plugin.parent_path());
        std::ofstream(plugin) << "#!/bin/sh\necho 0\n";
        fs::permissions(plugin, fs::perms::owner_exec, fs::perm_options::add);
        setPath(plugin.parent_path());

        // how many descriptors linuxdeploy uses before the query is not known, so try a range of limits, of which
        // the lowest are too low to search for plugins, and the highest high enough for the query
        // with some limits, the plugin itself runs out of descriptors, and is skipped like any failing plugin
        bool sawPipeError = false;
        for (int limit = 3; limit <= 64; ++limit) {
            SCOPED_TRACE(limit);
            // close the inherited descriptors which shells can close portably, as they would use up the limit
            // $PATH contains only the plugin, so use only builtins
            const auto result = linuxdeploy::subprocess::subprocess({"/bin/sh", "-c",
                "for fd in 3 4 5 6 7 8 9; do eval \"exec $fd>&-\"; done; "
                "ulimit -n \"$1\" && exec \"$0\" --list-plugins",
                LINUXDEPLOY_PATH, std::to_string(limit)}).run();
            EXPECT_EQ(result.term_signal(), 0);
            if (result.stdout_string().find(std::string("failed to create pipe: ") + strerror(EMFILE)) !=
                std::string::npos) {
                sawPipeError = true;
                EXPECT_NE(result.exit_code(), 0) << result.stdout_string() << result.stderr_string();
                EXPECT_THAT(result.stdout_string(), testing::Not(HasSubstr("Could not load plugin")));
            }
        }
        EXPECT_TRUE(sawPipeError);
    }

    TEST_F(PluginTest, SkipPluginWhichCannotBeAccessed) {
        // a symlink loop makes the file system checks of the plugin fail
        const auto plugin = directory / "bin" / "linuxdeploy-plugin-loop";
        fs::create_directories(plugin.parent_path());
        fs::create_symlink(plugin.filename(), plugin);
        setPath(plugin.parent_path());

        const auto result = listPlugins();
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exit_code(), 0) << result.stdout_string() << result.stderr_string();
        EXPECT_THAT(result.stdout_string(), HasSubstr("Could not load plugin"));
        EXPECT_THAT(result.stdout_string(), HasSubstr(plugin.string()));
    }
}
