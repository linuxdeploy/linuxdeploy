// system headers
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <unistd.h>
#include <memory.h>
#include <new>
#include <sys/wait.h>

// local headers
#include "linuxdeploy/subprocess/process.h"
#include "linuxdeploy/subprocess/util.h"
#include "linuxdeploy/util/assert.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

namespace {
    // owners which close or free what the constructor has not handed over yet if it throws

    class fd_owner {
    private:
        int fd_ = -1;

    public:
        fd_owner() = default;
        fd_owner(const fd_owner&) = delete;
        fd_owner& operator=(const fd_owner&) = delete;

        ~fd_owner() {
            if (fd_ != -1) {
                ::close(fd_);
            }
        }

        int get() const {
            return fd_;
        }

        void reset(int fd) {
            if (fd_ != -1) {
                ::close(fd_);
            }
            fd_ = fd;
        }

        int release() {
            const auto fd = fd_;
            fd_ = -1;
            return fd;
        }
    };

    struct pipe_fds {
        fd_owner read_end;
        fd_owner write_end;
    };

    // nullptr-terminated array of strings, as exec* want them
    class exec_vector {
    private:
        std::vector<char*> strings_{nullptr};

    public:
        exec_vector() = default;
        exec_vector(const exec_vector&) = delete;
        exec_vector& operator=(const exec_vector&) = delete;

        ~exec_vector() {
            std::for_each(strings_.begin(), strings_.end(), free);
        }

        void add(const std::string& string) {
            // insert an empty slot first, so that the copy cannot leak if inserting throws
            const auto slot = strings_.insert(strings_.end() - 1, nullptr);
            *slot = strdup(string.c_str());
            if (*slot == nullptr) {
                throw std::bad_alloc();
            }
        }

        char* const* data() const {
            return strings_.data();
        }
    };

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
    pipe_fds stdout_pipe;
    pipe_fds stderr_pipe;

    // FIXME: for debugging of #150
    auto create_pipe = [](pipe_fds& ends) {
        int fds[2];
        const auto rv = pipe(fds);

        if (rv != 0) {
            const auto error = errno;
            throw std::logic_error("failed to create pipe: " + std::string(strerror(error)));
        }

        ends.read_end.reset(fds[READ_END_]);
        ends.write_end.reset(fds[WRITE_END_]);
    };

    // create actual pipes
    create_pipe(stdout_pipe);
    create_pipe(stderr_pipe);

    // prepare arguments for exec* before forking, as the child may not allocate memory
    exec_vector exec_args;
    for (const auto& arg : args) {
        exec_args.add(arg);
    }

    exec_vector exec_env;
    for (const auto& [name, value] : env) {
        exec_env.add(name + '=' + value);
    }

    // create child process
    child_pid_ = fork();

    if (child_pid_ < 0) {
        throw std::runtime_error{"fork() failed"};
    }

    if (child_pid_ == 0) {
        // we're in the child process

        // the parent may be multithreaded, so do not allocate or throw until exec*, and use only async-signal-safe
        // functions (POSIX does not list execvpe, but it does not allocate in glibc, musl or FreeBSD)
        // exit with 127 like shells do if the program cannot be run
        auto fail = [] {
            _exit(127);
        };

        // first step: close the read end of both pipes
        ::close(stdout_pipe.read_end.get());
        ::close(stderr_pipe.read_end.get());

        auto connect_fd = [&fail](int fd, int fileno) {
            for (;;) {
                if (dup2(fd, fileno) == -1) {
                    const auto error = errno;
                    if (error != EINTR) {
                        fail();
                    }
                    continue;
                }

                break;
            }
        };

        connect_fd(stdout_pipe.write_end.get(), STDOUT_FILENO);
        connect_fd(stderr_pipe.write_end.get(), STDERR_FILENO);

        // now, we also have to close the write end of both pipes
        ::close(stdout_pipe.write_end.get());
        ::close(stderr_pipe.write_end.get());

        // call subprocess
        execvpe(args.front().c_str(), exec_args.data(), exec_env.data());

        // only reached if exec* fails
        fail();
    }

    // parent code

    // we do not intend to write to these pipes from this end
    close_pipe_fd(stdout_pipe.write_end.release());
    close_pipe_fd(stderr_pipe.write_end.release());

    // store the file descriptors last, as the destructor only closes them once the constructor has completed
    stdout_fd_ = stdout_pipe.read_end.release();
    stderr_fd_ = stderr_pipe.read_end.release();
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
