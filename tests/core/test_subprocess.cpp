#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/resource.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "test_util.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::subprocess;
using testing::HasSubstr;

namespace {
    TEST(SubprocessTest, ReportExitCode) {
        const auto result = subprocess({"sh", "-c", "echo output; exit 9"}).run();
        EXPECT_EQ(result.exit_code(), 9);
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.exec_error(), 0);
        EXPECT_EQ(result.stdout_string(), "output\n");
    }

    TEST(SubprocessTest, ReportTerminationBySignal) {
        // a signal must not look like an exit, even if the process printed something before
        const auto result = subprocess({"sh", "-c", "echo partial; kill -KILL $$"}).run();
        EXPECT_EQ(result.term_signal(), SIGKILL);
        EXPECT_NE(result.exit_code(), 0);
        EXPECT_EQ(result.stdout_string(), "partial\n");
    }

    TEST(SubprocessTest, ExecFailureExitsChild) {
        // the forked child must exit instead of throwing, otherwise it would continue to run the caller's code
        // it reports why only through exec_error()
        const auto result = subprocess({"/nonexistent/program"}).run();
        EXPECT_EQ(result.exec_error(), ENOENT);
        EXPECT_EQ(result.exit_code(), 127);
        EXPECT_EQ(result.term_signal(), 0);
        EXPECT_EQ(result.stderr_string(), "");
    }

    TEST(SubprocessTest, ReportNoExecErrorForProgramExitingLikeMissingCommand) {
        // a program may exit with the code a failed exec* leads to, but it was run
        const auto result = subprocess({"sh", "-c", "exit 127"}).run();
        EXPECT_EQ(result.exit_code(), 127);
        EXPECT_EQ(result.exec_error(), 0);
        EXPECT_EQ(result.term_signal(), 0);
    }

    TEST(SubprocessTest, ReportExecErrorOfProgramWhichCannotBeRun) {
        // the directory exists, but cannot be run
        const auto result = subprocess({"/"}).run();
        EXPECT_EQ(result.exec_error(), EACCES);
    }

    TEST(SubprocessTest, CheckOutputRejectsExecError) {
        EXPECT_THAT([] { subprocess({"/nonexistent/program"}).check_output(); },
                    testing::ThrowsMessage<called_process_error>(HasSubstr("could not be run")));
    }

    TEST(SubprocessTest, CloseDescriptorsIfPipesCannotBeCreated) {
        rlimit oldLimit{};
        ASSERT_EQ(getrlimit(RLIMIT_NOFILE, &oldLimit), 0);

        // only descriptors below the limit can be opened
        const auto searchLimit = std::min<rlim_t>(oldLimit.rlim_cur, 1024);
        auto isOpen = [](int fd) { return fcntl(fd, F_GETFD) != -1; };
        auto openDescriptors = [&]() {
            int count = 0;
            for (int fd = 0; fd < static_cast<int>(searchLimit); ++fd)
                count += isOpen(fd);
            return count;
        };
        const auto openBefore = openDescriptors();

        // the process needs three pipes, so with up to five free descriptors, each of them fails in turn
        for (int freeDescriptors = 0; freeDescriptors < 6; ++freeDescriptors) {
            SCOPED_TRACE(freeDescriptors);

            // the lowest limit below which exactly that many descriptors are free
            rlim_t limit = 0;
            for (int found = 0; found < freeDescriptors; ++limit)
                found += !isOpen(static_cast<int>(limit));
            ASSERT_LT(limit, searchLimit);

            rlimit lowLimit = oldLimit;
            lowLimit.rlim_cur = limit;
            ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &lowLimit), 0);
            bool threw = false;
            try {
                process proc({"true"});
            } catch (const std::logic_error&) {
                threw = true;
            }
            ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &oldLimit), 0);

            EXPECT_TRUE(threw);
            EXPECT_EQ(openDescriptors(), openBefore);
        }
    }

    // runs a program while the standard streams of the test are closed, so the pipes may get their numbers
    std::optional<subprocess_result> runWithoutStandardStreams(const std::vector<std::string>& args) {
        int saved[3];
        for (int fd = 0; fd < 3; ++fd) {
            saved[fd] = fcntl(fd, F_DUPFD_CLOEXEC, 3);
            if (saved[fd] == -1)
                return std::nullopt;
        }

        std::optional<subprocess_result> result;
        std::exception_ptr error;
        for (int fd = 0; fd < 3; ++fd)
            ::close(fd);
        try {
            result = subprocess(args).run();
        } catch (...) {
            error = std::current_exception();
        }
        for (int fd = 0; fd < 3; ++fd) {
            dup2(saved[fd], fd);
            ::close(saved[fd]);
        }

        if (error)
            std::rethrow_exception(error);
        return result;
    }

    TEST(SubprocessTest, ConnectStreamsIfStandardStreamsAreClosed) {
        // connecting one pipe must not replace another
        const auto result = runWithoutStandardStreams({"sh", "-c", "echo output; echo error >&2; exit 3"});
        ASSERT_TRUE(result);
        EXPECT_EQ(result->exit_code(), 3);
        EXPECT_EQ(result->exec_error(), 0);
        EXPECT_EQ(result->stdout_string(), "output\n");
        EXPECT_EQ(result->stderr_string(), "error\n");
    }

    TEST(SubprocessTest, ReportExecErrorIfStandardStreamsAreClosed) {
        const auto result = runWithoutStandardStreams({"/nonexistent/program"});
        ASSERT_TRUE(result);
        EXPECT_EQ(result->exec_error(), ENOENT);
        EXPECT_EQ(result->stderr_string(), "");
    }

    TEST(SubprocessTest, ClosePipesOnExec) {
        // programs started by other threads must not keep the pipes open
        process proc({"true"});
        EXPECT_NE(fcntl(proc.stdout_fd(), F_GETFD) & FD_CLOEXEC, 0);
        EXPECT_NE(fcntl(proc.stderr_fd(), F_GETFD) & FD_CLOEXEC, 0);
    }

    TEST(SubprocessTest, DescribeExit) {
        EXPECT_EQ(subprocess({"sh", "-c", "exit 3"}).run().exit_description(), "exited with code 3");
        EXPECT_EQ(subprocess({"sh", "-c", "kill -KILL $$"}).run().exit_description(),
                  "terminated by signal " + std::to_string(SIGKILL));
        EXPECT_EQ(subprocess({"/"}).run().exit_description(), "could not be run: " + std::string(strerror(EACCES)));
    }

    TEST(SubprocessTest, CheckOutputRejectsTerminationBySignal) {
        EXPECT_THAT([] { subprocess({"sh", "-c", "kill -KILL $$"}).check_output(); },
                    testing::ThrowsMessage<called_process_error>(HasSubstr("failed (terminated by signal")));
    }

    TEST(SubprocessTest, CheckOutputRejectsNonZeroExit) {
        EXPECT_THAT([] { subprocess({"sh", "-c", "exit 3"}).check_output(); },
                    testing::ThrowsMessage<called_process_error>(HasSubstr("failed (exited with code 3)")));
    }

    // handlers written for the std::logic_error which check_output() threw before still catch it
    TEST(SubprocessTest, CheckOutputFailureIsLogicError) {
        EXPECT_THROW(subprocess({"sh", "-c", "exit 3"}).check_output(), std::logic_error);
    }

    // errors of starting any program (e.g., too many open files) are not failures of the program
    TEST(SubprocessTest, CheckOutputDoesNotReportStartErrorsAsProgramFailures) {
        rlimit oldLimit{};
        ASSERT_EQ(getrlimit(RLIMIT_NOFILE, &oldLimit), 0);

        // no descriptor is free below the lowest one which is not open, so no pipe can be created
        int lowestFree = 0;
        while (fcntl(lowestFree, F_GETFD) != -1)
            ++lowestFree;
        rlimit lowLimit = oldLimit;
        lowLimit.rlim_cur = lowestFree;
        ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &lowLimit), 0);

        bool reportedAsFailure = false;
        bool threw = false;
        try {
            subprocess({"true"}).check_output();
        } catch (const called_process_error&) {
            reportedAsFailure = true;
        } catch (const std::logic_error&) {
            threw = true;
        }
        ASSERT_EQ(setrlimit(RLIMIT_NOFILE, &oldLimit), 0);

        EXPECT_TRUE(threw);
        EXPECT_FALSE(reportedAsFailure);
    }

    // programs are searched for like exec* does, which differs from a lookup in $PATH
    class SubprocessSearchTest : public testing::Test {
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
            fs::remove_all(directory);
        }

        static void setPath(const std::string& path) {
            ASSERT_EQ(setenv("PATH", path.c_str(), 1), 0);
        }

        // writes a program which prints its name
        static void writeProgram(const fs::path& path, const std::string& name) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/bin/sh\necho " << name << "\n";
            fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
        }

        // writes a script whose interpreter is missing
        static void writeBrokenProgram(const fs::path& path) {
            fs::create_directories(path.parent_path());
            std::ofstream(path) << "#!/nonexistent/interpreter\n";
            fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
        }
    };

    TEST_F(SubprocessSearchTest, SkipProgramWhichCannotBeRun) {
        // like execvpe(), continue searching the $PATH if a program cannot be run
        writeBrokenProgram(directory / "broken" / "program");
        writeProgram(directory / "working" / "program", "working");
        setPath((directory / "broken").string() + ":" + (directory / "working").string());

        const auto result = subprocess({"program"}).run();
        EXPECT_EQ(result.exec_error(), 0);
        EXPECT_EQ(result.stdout_string(), "working\n");
    }

    TEST_F(SubprocessSearchTest, ReportMissingInterpreterLikeMissingProgram) {
        // exec* reports ENOENT if the program was found, but its interpreter was not
        writeBrokenProgram(directory / "broken" / "program");
        setPath((directory / "broken").string());

        EXPECT_EQ(subprocess({"program"}).run().exec_error(), ENOENT);
    }

    TEST_F(SubprocessSearchTest, SearchEmptyPathEntry) {
        // an empty entry stands for the current directory
        writeProgram(directory / "program", "current");
        setPath((directory / "nonexistent").string() + ":");
        fs::current_path(directory);

        const auto result = subprocess({"program"}).run();
        EXPECT_EQ(result.exec_error(), 0);
        EXPECT_EQ(result.stdout_string(), "current\n");
    }

    TEST_F(SubprocessSearchTest, UseDefaultPathIfUnset) {
        // without a $PATH, exec* falls back to the C library's default search path, which contains sh
        ASSERT_EQ(unsetenv("PATH"), 0);

        const auto result = subprocess({"sh", "-c", "echo found"}).run();
        EXPECT_EQ(result.exec_error(), 0);
        EXPECT_EQ(result.stdout_string(), "found\n");
    }
}
