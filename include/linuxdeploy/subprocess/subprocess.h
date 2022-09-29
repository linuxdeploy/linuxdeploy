#pragma once

// system headers
#include <cstdio>
#include <unordered_map>
#include <string>
#include <utility>
#include <vector>

// local headers
#include "subprocess_result.h"
#include "util.h"

namespace linuxdeploy {
    namespace subprocess {
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

            std::string check_output() const;
        };
    }
}
