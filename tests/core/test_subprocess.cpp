#include <algorithm>
#include <csignal>
#include <exception>
#include <optional>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/resource.h>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/subprocess.h"

using namespace linuxdeploy::subprocess;
using testing::HasSubstr;

namespace {
    TEST(SubprocessTest, ReportExitCode) {
        const auto result = subprocess({"sh", "-c", "echo output; exit 9"}).run();
        EXPECT_EQ(result.exit_code(), 9);
        EXPECT_EQ(result.term_signal(), 0);
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
        const auto result = subprocess({"/nonexistent/program"}).run();
        EXPECT_EQ(result.exit_code(), 127);
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

        // the process needs two pipes, so with up to three free descriptors, each of them fails in turn
        for (int freeDescriptors = 0; freeDescriptors < 4; ++freeDescriptors) {
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
        EXPECT_EQ(result->stdout_string(), "output\n");
        EXPECT_EQ(result->stderr_string(), "error\n");
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
    }

    TEST(SubprocessTest, CheckOutputRejectsTerminationBySignal) {
        EXPECT_THAT([] { subprocess({"sh", "-c", "kill -KILL $$"}).check_output(); },
                    testing::ThrowsMessage<std::logic_error>(HasSubstr("failed (terminated by signal")));
    }

    TEST(SubprocessTest, CheckOutputRejectsNonZeroExit) {
        EXPECT_THAT([] { subprocess({"sh", "-c", "exit 3"}).check_output(); },
                    testing::ThrowsMessage<std::logic_error>(HasSubstr("failed (exited with code 3)")));
    }
}
