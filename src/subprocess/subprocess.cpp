// system headers
#include <algorithm>
#include <array>
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

        subprocess::subprocess(std::initializer_list<std::string> args)
            : subprocess(std::vector<std::string>(args), get_environment()) {}

        subprocess::subprocess(std::initializer_list<std::string> args, subprocess_env_map_t env)
            : subprocess(std::vector<std::string>(args), std::move(env)) {}

        subprocess::subprocess(std::vector<std::string> args)
            : args_(std::move(args)), env_(get_environment()) {}

        subprocess::subprocess(std::vector<std::string> args, subprocess_env_map_t env)
            : args_(std::move(args)), env_(std::move(env)) {
            // preconditions
            util::assert::assert_not_empty(args_);
        }

        subprocess_result subprocess::run() const {
            process proc{args_, env_};

            class PipeState {
            public:
                pipe_reader reader;
                subprocess_result_buffer_t  buffer;
                bool eof = false;

                explicit PipeState(int fd) : reader(fd) {}
            };

            // create pipe readers and empty buffers for both stdout and stderr
            // we manage them in this (admittedly, kind of complex-looking) array so we can later easily perform the
            // operations in a loop
            std::array<PipeState, 2> buffers = {
                PipeState(proc.stdout_fd()),
                PipeState(proc.stderr_fd())
            };

            for (;;) {
                for (auto& pipe_state : buffers) {
                    // read some bytes into smaller intermediate buffer to prevent either of the pipes to overflow
                    // the results are immediately appended to the main buffer
                    subprocess_result_buffer_t intermediate_buffer(4096);

                    // (try to) read all available data from pipe
                    for (; !pipe_state.eof; ) {
                        switch (pipe_state.reader.read(intermediate_buffer)) {
                            case pipe_reader::result::SUCCESS: {
                                // append to main buffer
                                pipe_state.buffer.reserve(pipe_state.buffer.size() + intermediate_buffer.size());
                                std::copy(intermediate_buffer.begin(), intermediate_buffer.end(), std::back_inserter(pipe_state.buffer));
                                break;
                            }
                            case pipe_reader::result::END_OF_FILE: {
                                pipe_state.eof = true;
                                break;
                            }
                            default:
                                break;
                        }
                    }
                }

                if (proc.is_running()) {
                    // reduce load on CPU until EOF
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }

                // once all buffers are EOF, we can stop reading
                if (std::all_of(buffers.begin(), buffers.end(), [](const PipeState& pipe_state) {
                    return pipe_state.eof;
                })) {
                    break;
                }
            }

            // make sure contents are null-terminated
            buffers[0].buffer.emplace_back('\0');
            buffers[1].buffer.emplace_back('\0');

            auto exit_code = proc.close();

            return subprocess_result{exit_code, buffers[0].buffer, buffers[1].buffer};
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
