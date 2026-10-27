// system headers
#include <string>

// local headers
#include "linuxdeploy/subprocess/subprocess_result.h"

// shorter than using namespace ...
using namespace linuxdeploy::subprocess;

subprocess_result::subprocess_result(int exit_code, int term_signal,
                                     subprocess_result_buffer_t stdout_contents,
                                     subprocess_result_buffer_t stderr_contents)
    : exit_code_(exit_code), term_signal_(term_signal), stdout_contents_(std::move(stdout_contents)), stderr_contents_(std::move(stderr_contents)) {}


int subprocess_result::exit_code() const {
    return exit_code_;
}

int subprocess_result::term_signal() const {
    return term_signal_;
}

std::string subprocess_result::exit_description() const {
    if (term_signal_ != 0) {
        return "terminated by signal " + std::to_string(term_signal_);
    }

    return "exited with code " + std::to_string(exit_code_);
}

const subprocess_result_buffer_t& subprocess_result::stdout_contents() const {
    return stdout_contents_;
}

const subprocess_result_buffer_t& subprocess_result::stderr_contents() const {
    return stderr_contents_;
}

std::string subprocess_result::stdout_string() const {
    return stdout_contents().data();
}

std::string subprocess_result::stderr_string() const {
    return stderr_contents().data();
}
