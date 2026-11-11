#pragma once

// system headers
#include <cstdio>
#include <stdexcept>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

// local headers
#include "subprocess_result.h"
#include "util.h"

namespace linuxdeploy {
    namespace subprocess {
        // thrown by check_output() if the program fails or cannot be run, not if starting any program fails (e.g.,
        // too many open files); a std::logic_error, like check_output() threw before, so existing handlers still
        // catch it
        class called_process_error : public std::logic_error {
        public:
            explicit called_process_error(const std::string& msg) : std::logic_error(msg) {}
        };

        class subprocess {
        private:
            std::vector<std::string> args_{};
            std::unordered_map<std::string, std::string> env_{};

        public:
            explicit subprocess(std::initializer_list<std::string> args);

            explicit subprocess(std::initializer_list<std::string> args, subprocess_env_map_t env);

            explicit subprocess(std::vector<std::string> args);

            explicit subprocess(std::vector<std::string> args, subprocess_env_map_t env);

            subprocess_result run() const;

            // throws called_process_error if the program exits with a non-zero code, is terminated, or cannot be run
            std::string check_output() const;
        };
    }
}
