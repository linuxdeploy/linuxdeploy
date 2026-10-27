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
            int term_signal_;
            subprocess_result_buffer_t stdout_contents_;
            subprocess_result_buffer_t stderr_contents_;

        public:
            subprocess_result(int exit_code, int term_signal,
                              subprocess_result_buffer_t stdout_contents, subprocess_result_buffer_t stderr_contents);

            /**
             * @return exit code, or the signal's number if a signal terminated the process
             */
            int exit_code() const;

            /**
             * @return signal which terminated the process, or 0 if it exited
             */
            int term_signal() const;

            /**
             * @return how the process ended for messages, e.g., "exited with code 1" or "terminated by signal 9"
             */
            std::string exit_description() const;

            const subprocess_result_buffer_t& stdout_contents() const;

            std::string stdout_string() const;

            const subprocess_result_buffer_t& stderr_contents() const;

            std::string stderr_string() const;
        };
    }
}
