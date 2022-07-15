// system headers
#include <algorithm>
#include <cstring>
#include <poll.h>
#include <unistd.h>
#include <stdexcept>

// local headers
#include "linuxdeploy/subprocess/pipe_reader.h"

pipe_reader::pipe_reader(int pipe_fd) : pollfd_(pollfd{pipe_fd, POLLIN | POLLHUP}) {}

pipe_reader::result pipe_reader::read(std::vector<std::string::value_type>& buffer, std::chrono::milliseconds read_timeout) {
    const auto timeout_msec = std::chrono::duration_cast<std::chrono::milliseconds>(read_timeout).count();

    // we could (and probably should) be using poll on multiple fds at once
    // however given the low bandwidth of data to handle, this should be fine, given we use a small-enough timeout
    // also the read buffer sizes could be further increased to improve the overall performance
    switch (poll(&pollfd_, 1, static_cast<int>(timeout_msec))) {
        case -1:
            // TODO: introduce custom subprocess_error
            throw std::runtime_error{"unexpected error reading from pipe: " + std::string(strerror(errno))};
        case 0:
            return result::TIMEOUT;
        case 1: {
            if ((pollfd_.revents & POLLIN) != 0) {
                ssize_t rv = ::read(pollfd_.fd, buffer.data(), buffer.size());

                switch (rv) {
                    case -1: {
                        throw std::runtime_error{"unexpected error reading from pipe: " + std::string(strerror(errno))};
                    }
                    case 0: {
                        return result::END_OF_FILE;
                    }
                    default: {
                        // set the size correctly so the caller can just query the vector's size if the number of read chars is needed
                        buffer.resize(rv);
                        return result::SUCCESS;
                    }
                }
            }

            if ((pollfd_.revents & POLLHUP) != 0) {
                // appears like this can be considered eof
                return result::END_OF_FILE;
            }

            if ((pollfd_.revents & POLLERR) != 0 || (pollfd_.revents & POLLNVAL) != 0) {
                throw std::runtime_error{"poll() failed unexpectedly"};
            }

            break;
        }
        default:
            // this is a should-never-ever-happen case, a return value not handled by the lines above is actually not possible
            throw std::runtime_error{"unexpected return value from pollfd"};
    }
}
