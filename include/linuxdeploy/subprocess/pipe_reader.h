#pragma once

#include <string>
#include <set>
#include <vector>
#include <poll.h>

/**
 * Reads from a pipe when data is available, and hands data to registered callbacks.
 */
class pipe_reader {
private:
    const int pipe_fd_;

public:
    /**
     * Construct new instance from pipe file descriptor.
     * @param pipe_fd file descriptor for pipe we will read from (e.g., a subprocess's stdout, stderr pipes)
     */
    explicit pipe_reader(int pipe_fd);

    /**
     * Read from pipe file descriptor and store data in buffer.
     * This method is non-blocking, i.e., if there is no data to read, it returns immediately.
     * Otherwise, it will read until either of the following conditions is met:
     *
     *     - no more data left in the pipe to be read
     *     - buffer is completely filled
     *
     * On errors, a subprocess_error is thrown.
     *
     * @param buffer buffer to store read data into
     * @returns amount of characters read from the pipe
     */
    size_t read(std::vector<std::string::value_type>& buffer) const;
};
