#pragma once

// system headers
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace linuxdeploy {
    namespace subprocess {
        typedef std::vector<std::string::value_type> subprocess_result_buffer_t;

        /**
         * Result of subprocess execution. Follows Value Object design pattern.
         */
        class subprocess_result {
        private:
            int exit_code_;
            subprocess_result_buffer_t stdout_contents_;
            subprocess_result_buffer_t stderr_contents_;

        public:
            subprocess_result(int exit_code, subprocess_result_buffer_t stdout_contents,
                              subprocess_result_buffer_t stderr_contents);

            int exit_code() const;

            const subprocess_result_buffer_t& stdout_contents() const;

            std::string stdout_string() const;

            const subprocess_result_buffer_t& stderr_contents() const;

            std::string stderr_string() const;
        };
    }
}
