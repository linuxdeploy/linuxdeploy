// library headers
#include <gtest/gtest.h>

// local headers
#include <linuxdeploy/core/log.h>

using namespace linuxdeploy::core::log;

int main(int argc, char** argv) {
    // set loglevel to prevent unnecessary log messages
    ldLog::setVerbosity(LD_ERROR);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
