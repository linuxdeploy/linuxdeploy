#include <algorithm>
#include <fcntl.h>
#include <sys/resource.h>

#include "gtest/gtest.h"

#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/subprocess.h"

using namespace linuxdeploy::subprocess;

namespace {
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
}
