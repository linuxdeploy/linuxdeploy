// system headers
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <unistd.h>

// local headers
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/util/assert.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

subprocess::subprocess(std::initializer_list<std::string> args, subprocess_env_map_t env)
    : subprocess(std::vector<std::string>(args), std::move(env)) {}

subprocess::subprocess(std::vector<std::string> args, subprocess_env_map_t env)
    : args_(std::move(args)), env_(std::move(env)) {
    // preconditions
    util::assert::assert_not_empty(args_);
}

subprocess_result subprocess::run() const {
    process proc{args_, env_};

    subprocess_result_buffer_t stdout_contents{};
    subprocess_result_buffer_t stderr_contents{};

    auto read_into_buffer = [](int fd, subprocess_result_buffer_t& out_buffer) {
        // intermediate buffer
        subprocess_result_buffer_t buffer(4096);

        for (;;) {
            const auto bytes_read = read(fd, buffer.data(), buffer.size());

            if (bytes_read == -1) {
                // handle interrupts properly
                if (errno == EINTR) {
                    continue;
                }

                throw std::logic_error{"failed to read from fd"};
            }

            if (bytes_read == 0) {
                break;
            }

            std::copy(buffer.begin(), buffer.begin() + bytes_read, std::back_inserter(out_buffer));
        }
    };

    // TODO: make sure neither stderr nor stdout overflow
    read_into_buffer(proc.stdout_fd(), stdout_contents);
    read_into_buffer(proc.stderr_fd(), stderr_contents);

    // make sure contents are null-terminated
    stdout_contents.emplace_back('\0');
    stderr_contents.emplace_back('\0');

    auto exit_code = proc.close();

    return subprocess_result{exit_code, stdout_contents, stderr_contents};
}

std::string subprocess::check_output() const {
    const auto result = run();

    if (result.exit_code() != 0) {
        throw std::logic_error{"subprocess failed (exit code " + std::to_string(result.exit_code()) + ")"};
    }

    return result.stdout_string();
}
