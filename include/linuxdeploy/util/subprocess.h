/**
 * Wrapper for cpp-subprocess. Provides additional convenience functions.
 */

#pragma once

// library includes
#include <subprocess.hpp>

namespace linuxdeploy {
    namespace util {
        namespace subprocess {
            using namespace ::subprocess;

            // Reads output channels of existing Popen object into buffers and returns the contents
            std::pair<std::string, std::string> check_output_error(Popen& proc);
        }
    }
}
