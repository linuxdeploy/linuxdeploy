// global headers
#include <string>
#include <vector>
#include <unistd.h>

// local headers
#include "linuxdeploy/util/subprocess.h"

namespace linuxdeploy {
    namespace util {
        namespace subprocess {
            std::pair<std::string, std::string> check_output_error(Popen& proc) {
                std::vector<char> procStdout, procStderr;

                // make file descriptors non-blocking
                for (const auto& fd : {proc.output(), proc.error()}) {
                    auto flags = fcntl(fileno(fd), F_GETFL, 0);
                    flags |= O_NONBLOCK;
                    fcntl(fileno(fd), F_SETFL, flags);
                }

                while (true) {
                    constexpr auto bufSize = 1;
//                    constexpr auto bufSize = 512*1024;
                    std::vector<char> buf(bufSize, '\0');

                    for (auto& fd : {proc.output(), proc.error()}) {
                        auto& outBuf = fd == proc.output() ? procStdout : procStderr;

                        auto size = fread(buf.data(), sizeof(char), buf.size(), fd);

                        if (size < 0)
                            throw std::runtime_error("Error reading fd");

                        if (size == 0)
                            continue;

                        if (size > buf.size())
                            throw std::runtime_error("Read more bytes than buffer size");

                        auto outBufSize = outBuf.size();
                        outBuf.reserve(outBufSize + size + 1);
                        std::copy(buf.begin(), buf.begin() + size, std::back_inserter(outBuf));
                    }

                    if (proc.poll() >= 0 && feof(proc.output()) != 0 && feof(proc.error()) != 0)
                        break;
                }

                std::string stdoutContents(procStdout.begin(), procStdout.end());
                std::string stderrContents(procStderr.begin(), procStderr.end());

                return std::make_pair(stdoutContents, stderrContents);
            }
        }
    }
}
