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
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/util/assert.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

int process::pid() const {
    return child_pid_;
}

int process::stdout_fd() const {
    return stdout_fd_;
}

int process::stderr_fd() const {
    return stderr_fd_;
}

process::process(std::initializer_list<std::string> args, const subprocess_env_map_t& env)
    : process(std::vector<std::string>(args), env) {}

process::process(const std::vector<std::string>& args, const subprocess_env_map_t& env) {
    // preconditions
    util::assert::assert_not_empty(args);

    // pipes for both stdout and stderr
    // the order is, as seen from the child: [read, write]
    int stdout_pipe_fds[2];
    int stderr_pipe_fds[2];

    // create actual pipes
    if (pipe(stdout_pipe_fds) != 0 || pipe(stderr_pipe_fds) != 0) {
        throw std::logic_error{"failed to create pipes"};
    }

    // create child process
    child_pid_ = fork();

    if (child_pid_ < 0) {
        throw std::runtime_error{"fork() failed"};
    }

    if (child_pid_ == 0) {
        // we're in the child process

        // first step: close the read end of both pipes
        ::close(stdout_pipe_fds[READ_END_]);
        ::close(stderr_pipe_fds[READ_END_]);

        auto connect_fd = [](int fds[], int fileno) {
            for (;;) {
                if (dup2(fds[WRITE_END_], fileno) == -1) {
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
        ::close(stdout_pipe_fds[WRITE_END_]);
        ::close(stderr_pipe_fds[WRITE_END_]);

        // prepare arguments for exec*
        auto exec_args = make_args_vector_(args);
        auto exec_env = make_env_vector_(env);

        // call subprocess
        execvpe(args.front().c_str(), exec_args.data(), exec_env.data());

        // only reached if exec* fails

        // clean up memory if exec should ever return
        // prevents memleaks if the exception below would be handled by a caller
        auto deleter = [](char* ptr) {
            free(ptr);
            ptr = nullptr;
        };

        std::for_each(exec_args.begin(), exec_args.end(), deleter);
        std::for_each(exec_env.begin(), exec_env.end(), deleter);

        throw std::runtime_error{"exec() failed: " + std::string(strerror(errno))};
    }

    // parent code

    // we do not intend to write to the processes
    ::close(stdout_pipe_fds[WRITE_END_]);
    ::close(stderr_pipe_fds[WRITE_END_]);

    // store file descriptors
    stdout_fd_ = stdout_pipe_fds[READ_END_];
    stderr_fd_ = stderr_pipe_fds[READ_END_];
}

int process::close() {
    if (!exited_) {
        ::close(stdout_fd_);
        stdout_fd_ = -1;

        ::close(stderr_fd_);
        stderr_fd_ = -1;

        {
            int status;

            if (waitpid(child_pid_, &status, 0) == -1) {
                throw std::logic_error{"waitpid() failed"};
            }

            exited_ = true;
            exit_code_ = check_waitpid_status_(status);
        }
    }

    return exit_code_;
}

process::~process() {
    (void) close();
}

std::vector<char*> process::make_args_vector_(const std::vector<std::string>& args) {
    std::vector<char*> rv{};
    rv.reserve(args.size());

    for (const auto& arg : args) {
        rv.emplace_back(strdup(arg.c_str()));
    }

    // execv* want a nullptr-terminated array
    rv.emplace_back(nullptr);

    return rv;
}

std::vector<char*> process::make_env_vector_(const subprocess_env_map_t& env) {
    std::vector<char*> rv;

    // first, copy existing environment
    // we cannot reserve space in the vector unfortunately, as we don't know the size of environ before the iteration
    if (environ != nullptr) {
        for (auto** current_env_var = environ; *current_env_var != nullptr; ++current_env_var) {
            rv.emplace_back(strdup(*current_env_var));
        }
    }

    // add own environment variables, overwriting existing ones if necessary
    for (const auto& env_var : env) {
        const auto& key = env_var.first;
        const auto& value = env_var.second;

        auto predicate = [&key](char* existing_env_var) {
            char* equal_sign = strstr(existing_env_var, "=");

            if (equal_sign == nullptr) {
                throw std::runtime_error{"no equal sign in environment variable"};
            }

            std::string existing_env_var_name{existing_env_var, 0, static_cast<size_t>(equal_sign - existing_env_var)};

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

void process::kill(int signal) const {
    if (::kill(child_pid_, signal) != 0) {
        throw std::logic_error{"failed to kill child process"};
    }

    if (waitpid(child_pid_, nullptr, 0)) {
        throw std::logic_error{"failed to wait for killed child"};
    }
}

bool process::is_running() {
    if (exited_) {
        return false;
    }

    int status;
    auto result = waitpid(child_pid_, &status, WNOHANG);

    if (result == 0) {
        return true;
    }

    if (result == child_pid_) {
        exited_ = true;

        exit_code_ = check_waitpid_status_(status);

        return false;
    }

    if (result < 0) {
        // TODO: check errno == ECHILD
        throw std::logic_error{"waitpid() failed: " + std::string(strerror(errno))};
    }

    // can only happen if waitpid() returns an unknown process ID
    throw std::logic_error{"unknown error occured"};
}

int process::check_waitpid_status_(int status) {
    if (WIFSIGNALED(status) != 0) {
        // TODO: consider treating child exit caused by signals separately
        return WTERMSIG(status);
    } else if (WIFEXITED(status) != 0) {
        return WEXITSTATUS(status);
    }

    throw std::logic_error{"unknown child process state"};
}
