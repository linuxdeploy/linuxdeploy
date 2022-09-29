// system headers
#include <algorithm>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <memory.h>
#include <wait.h>
#include <sstream>

// local headers
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/util.h"
#include "linuxdeploy/util/assert.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

namespace {
    std::vector<char*> make_args_vector(const std::vector<std::string>& args) {
        std::vector<char*> rv{};
        rv.reserve(args.size());

        for (const auto& arg : args) {
            rv.emplace_back(strdup(arg.c_str()));
        }

        // execv* want a nullptr-terminated array
        rv.emplace_back(nullptr);

        return rv;
    }

    int check_waitpid_status(int status) {
        if (WIFSIGNALED(status) != 0) {
            // TODO: consider treating child exit caused by signals separately
            return WTERMSIG(status);
        } else if (WIFEXITED(status) != 0) {
            return WEXITSTATUS(status);
        }

        throw std::logic_error{"unknown child process state"};
    }

    void close_pipe_fd(int fd, bool ignore_ebadf = false) {
        const auto rv = ::close(fd);

        if (rv != 0) {
            const auto error = errno;

            // might be closed already by the child process, in this case we can safely ignore the error
            if (error != EBADF) {
                throw std::logic_error("failed to close pipe fd: " + std::string(strerror(error)));
            };
        }
    }

    using env_vector_t = std::vector<char*>;

    /**
     * Convert a map of environment variables to a vector usable within
     * @param map
     * @return
     */
    env_vector_t env_map_to_vector(const subprocess_env_map_t& map) {
        env_vector_t out;

        for (const auto& [name, value] : map) {
            std::ostringstream oss;
            oss << name << '=' << value;
            out.emplace_back(strdup(oss.str().c_str()));
        }

        // must be null terminated, of course
        out.emplace_back(nullptr);

        return out;
    }
}

int process::pid() const {
    return child_pid_;
}

int process::stdout_fd() const {
    return stdout_fd_;
}

int process::stderr_fd() const {
    return stderr_fd_;
}

process::process(std::initializer_list<std::string> args)
    : process(std::vector<std::string>(args), get_environment()) {}

process::process(std::initializer_list<std::string> args, const subprocess_env_map_t& env)
    : process(std::vector<std::string>(args), env) {}

process::process(const std::vector<std::string>& args)
    : process(args, get_environment()) {}

process::process(const std::vector<std::string>& args, const subprocess_env_map_t& env) {
    // preconditions
    util::assert::assert_not_empty(args);

    // pipes for both stdout and stderr
    // the order is, as seen from the child: [read, write]
    int stdout_pipe_fds[2];
    int stderr_pipe_fds[2];

    // FIXME: for debugging of #150
    auto create_pipe = [](int fds[]) {
        const auto rv = pipe(fds);

        if (rv != 0) {
            const auto error = errno;
            throw std::logic_error("failed to create pipe: " + std::string(strerror(error)));
        }
    };

    // create actual pipes
    create_pipe(stdout_pipe_fds);
    create_pipe(stderr_pipe_fds);

    // create child process
    child_pid_ = fork();

    if (child_pid_ < 0) {
        throw std::runtime_error{"fork() failed"};
    }

    if (child_pid_ == 0) {
        // we're in the child process

        // first step: close the read end of both pipes
        close_pipe_fd(stdout_pipe_fds[READ_END_]);
        close_pipe_fd(stderr_pipe_fds[READ_END_]);

        auto connect_fd = [](int fd, int fileno) {
            for (;;) {
                if (dup2(fd, fileno) == -1) {
                    const auto error = errno;
                    if (error != EINTR) {
                        throw std::logic_error{"failed to connect pipes: " + std::string(strerror(error))};
                    }
                    continue;
                }

                break;
            }
        };

        connect_fd(stdout_pipe_fds[WRITE_END_], STDOUT_FILENO);
        connect_fd(stderr_pipe_fds[WRITE_END_], STDERR_FILENO);

        // now, we also have to close the write end of both pipes
        close_pipe_fd(stdout_pipe_fds[WRITE_END_]);
        close_pipe_fd(stderr_pipe_fds[WRITE_END_]);

        // prepare arguments for exec*
        auto exec_args = make_args_vector(args);
        auto exec_env = env_map_to_vector(env);

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

    // we do not intend to write to these pipes from this end
    close_pipe_fd(stdout_pipe_fds[WRITE_END_]);
    close_pipe_fd(stderr_pipe_fds[WRITE_END_]);

    // store file descriptors
    stdout_fd_ = stdout_pipe_fds[READ_END_];
    stderr_fd_ = stderr_pipe_fds[READ_END_];
}

int process::close() {
    if (!exited_) {
        int status;

        if (waitpid(child_pid_, &status, 0) == -1) {
            throw std::logic_error{"waitpid() failed"};
        }

        exited_ = true;
        exit_code_ = check_waitpid_status(status);
    }

    close_pipe_fd(stdout_fd_, true);
    stdout_fd_ = -1;

    close_pipe_fd(stderr_fd_, true);
    stderr_fd_ = -1;

    return exit_code_;
}

process::~process() {
    (void) close();
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
        exit_code_ = check_waitpid_status(status);

        return false;
    }

    if (result < 0) {
        // TODO: check errno == ECHILD
        throw std::logic_error{"waitpid() failed: " + std::string(strerror(errno))};
    }

    // can only happen if waitpid() returns an unknown process ID
    throw std::logic_error{"unknown error occured"};
}
