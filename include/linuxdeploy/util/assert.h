#pragma once

// system headers
#include <stdexcept>

namespace linuxdeploy {
    namespace util {
        namespace assert {
            template<typename T>
            void assert_not_empty(T container) {
                if (container.empty()) {
                    throw std::invalid_argument{"container must not be empty"};
                }
            }
        }
    }
}
