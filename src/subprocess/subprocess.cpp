// system headers
#include <algorithm>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <memory.h>
#include <wait.h>

// local headers
#include "subprocess.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

subprocess::subprocess(std::initializer_list<std::string> args, subprocess_env_map_t env)
    : args_(args), env_(std::move(env)) {
    assert_args_not_empty_();
}

subprocess::subprocess(std::vector<std::string> args, subprocess_env_map_t env)
    : args_(std::move(args)), env_(std::move(env)) {
    assert_args_not_empty_();
}

subprocess_result subprocess::run() const {
    // these constants help make the code below more readable
    static constexpr int READ_END = 0, WRITE_END = 1;

    // pipes for both stdout and stderr
    // the order is, as seen from the child: [read, write]
    int stdout_pipe_fds[2];
    int stderr_pipe_fds[2];

    // create actual pipes
    if (pipe(stdout_pipe_fds) != 0 || pipe(stderr_pipe_fds) != 0) {
        throw std::logic_error{"failed to create pipes"};
    }

    // create child process
    pid_t child_pid = fork();

    if (child_pid < 0) {
        throw std::runtime_error{"fork() failed"};
    }

    std::cout << "child pid: " << std::to_string(child_pid) << std::endl;

    if (child_pid == 0) {
        // we're in the child process

        // first step: close the read end of both pipes
        close(stdout_pipe_fds[READ_END]);
        close(stderr_pipe_fds[READ_END]);

        auto connect_fd = [](int fds[], int fileno) {
            for (;;) {
                if (dup2(fds[WRITE_END], fileno) == -1) {
                    if (errno != EINTR) {
                        throw std::logic_error{"failed to connect pipes"};
                    }
                    continue;
                }

                break;
            }
        };

        connect_fd(stdout_pipe_fds, STDOUT_FILENO);
        connect_fd(stderr_pipe_fds, STDERR_FILENO);

        // now, we also have to close the write end of both pipes
        close(stdout_pipe_fds[WRITE_END]);
        close(stderr_pipe_fds[WRITE_END]);

        // prepare arguments for exec*
        auto exec_args = make_args_vector_();
        auto exec_env = make_env_vector_();

        // call subprocess
        execvpe(args_.front().c_str(), exec_args.data(), exec_env.data());

        throw std::runtime_error{"exec() failed: " + std::string(strerror(errno))};
    }

    // parent code

    // we do not intend to write to the processes
    close(stdout_pipe_fds[WRITE_END]);
    close(stderr_pipe_fds[WRITE_END]);

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

    read_into_buffer(stdout_pipe_fds[READ_END], stdout_contents);
    read_into_buffer(stderr_pipe_fds[READ_END], stderr_contents);

    // done reading -> close read end
    close(stdout_pipe_fds[READ_END]);
    close(stderr_pipe_fds[READ_END]);

    // make sure contents are null-terminated
    stdout_contents.emplace_back('\0');
    stderr_contents.emplace_back('\0');

    // wait for child to exit and fetch its exit code
    int exit_code;
    {
        int temporary;
        waitpid(child_pid, &temporary, 0);
        exit_code = WEXITSTATUS(temporary);
    }

    return subprocess_result{exit_code, stdout_contents, stderr_contents};
}

void subprocess::assert_args_not_empty_() const {
    if(args_.empty()) {
        throw std::invalid_argument("args may not be empty");
    }
}

std::vector<char*> subprocess::make_args_vector_() const {
    std::vector<char*> rv{};
    rv.reserve(args_.size());

    for (const auto& arg : args_) {
        rv.emplace_back(strdup(arg.c_str()));
    }

    // execv* want a nullptr-terminated array
    rv.emplace_back(nullptr);

    return rv;
}

std::vector<char*> subprocess::make_env_vector_() const {
    std::vector<char*> rv;

    // first, copy existing environment
    // we cannot reserve space in the vector unfortunately, as we don't know the size of environ before the iteration
    if (environ != nullptr) {
        for (auto** current_env_var = environ; *current_env_var != nullptr; ++current_env_var) {
            rv.emplace_back(strdup(*current_env_var));
        }
    }

    // add own environment variables, overwriting existing ones if necessary
    for (const auto& env_var : env_) {
        const auto& key = env_var.first;
        const auto& value = env_var.second;

        auto predicate = [&key](char* existing_env_var) {
            char* equal_sign = strstr(existing_env_var, "=");

            if (equal_sign == nullptr) {
                throw std::runtime_error{"no equal sign in environment variable"};
            }

            std::string existing_env_var_name{existing_env_var_name, 0, static_cast<size_t>(existing_env_var - equal_sign)};

            return existing_env_var_name == key;
        };

        // delete existing env var, if any
        rv.erase(std::remove_if(rv.begin(), rv.end(), predicate), rv.end());

        // insert new value
        std::ostringstream oss;
        oss << key;
        oss << "=";
        oss << value;

        rv.emplace_back(strdup(oss.str().c_str()));
    }

    // exec*e want a nullptr-terminated array
    rv.emplace_back(nullptr);

    return rv;
}

std::string subprocess::check_output() const {
    const auto result = run();

    if (result.exit_code() != 0) {
        throw std::logic_error{"subprocess failed (exit code " + std::to_string(result.exit_code()) + ")"};
    }

    return result.stdout_string();
}
