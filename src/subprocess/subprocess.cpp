// system headers
#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <thread>

// local headers
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/pipe_reader.h"
#include "linuxdeploy/util/assert.h"

namespace linuxdeploy {
    namespace subprocess {
        subprocess::subprocess(std::initializer_list<std::string> args, subprocess_env_map_t env)
            : subprocess(std::vector<std::string>(args), std::move(env)) {}

        subprocess::subprocess(std::vector<std::string> args, subprocess_env_map_t env)
            : args_(std::move(args)), env_(std::move(env)) {
            // preconditions
            util::assert::assert_not_empty(args_);
        }

        subprocess_result subprocess::run() const {
            process proc{args_, env_};

            // create pipe readers and empty buffers for both stdout and stderr
            // we manage them in this (admittedly, kind of complex-looking) array so we can later easily perform the
            // operations in a loop
            std::array<std::pair<pipe_reader, subprocess_result_buffer_t>, 2> buffers{
                std::make_pair(pipe_reader(proc.stdout_fd()), subprocess_result_buffer_t{}),
                std::make_pair(pipe_reader(proc.stderr_fd()), subprocess_result_buffer_t{}),
            };

            for (;;) {
                for (auto& pair : buffers) {
                    // make code more readable
                    auto& reader = pair.first;
                    auto& buffer = pair.second;

                    // read some bytes into smaller intermediate buffer to prevent either of the pipes to overflow
                    // the results are immediately appended to the main buffer
                    subprocess_result_buffer_t intermediate_buffer(4096);

                    // (try to) read all available data from pipe
                    for (;;) {
                        const auto bytes_read = reader.read(intermediate_buffer);

                        if (bytes_read == 0) {
                            break;
                        }

                        // append to main buffer
                        buffer.reserve(buffer.size() + bytes_read);
                        std::copy(intermediate_buffer.begin(), (intermediate_buffer.begin() + bytes_read),
                                  std::back_inserter(buffer));
                    }
                }

                // do-while might be a little more elegant, but we can save this one unnecessary sleep, so...
                if (proc.is_running()) {
                    // reduce load on CPU
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } else {
                    break;
                }
            }

            // make sure contents are null-terminated
            buffers[0].second.emplace_back('\0');
            buffers[1].second.emplace_back('\0');

            auto exit_code = proc.close();

            return subprocess_result{exit_code, buffers[0].second, buffers[1].second};
        }

        std::string subprocess::check_output() const {
            const auto result = run();

            if (result.exit_code() != 0) {
                throw std::logic_error{"subprocess failed (exit code " + std::to_string(result.exit_code()) + ")"};
            }

            return result.stdout_string();
        }
    }
}
