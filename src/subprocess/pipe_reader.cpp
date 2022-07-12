// system headers
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>
#include <functional>
#include <cstring>
#include <stdexcept>

// local headers
#include "linuxdeploy/subprocess/pipe_reader.h"

pipe_reader::pipe_reader(int pipe_fd) : pipe_fd_(pipe_fd) {
    // add O_NONBLOCK TO fd's flags to be able to read
    auto flags = fcntl(pipe_fd_, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(pipe_fd_, F_SETFL, flags);
}

size_t pipe_reader::read(std::vector<std::string::value_type>& buffer) const {
    for (;;) {
        ssize_t rv = ::read(pipe_fd_, buffer.data(), buffer.size());

        if (rv == -1) {
            switch (errno) {
                // retry in case data is currently not available
                case EINTR:
                case EAGAIN:
                    continue;
                default:
                    // TODO: introduce custom subprocess_error
                    throw std::runtime_error{"unexpected error reading from pipe: " + std::string(strerror(errno))};
            }
        }

        return rv;
    }
}
