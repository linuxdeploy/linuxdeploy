// system headers
#include <tuple>
#include <thread>

// local headers
#include <linuxdeploy/plugin/plugin_process_handler.h>
#include <linuxdeploy/subprocess/process.h>
#include <linuxdeploy/util/util.h>
#include <linuxdeploy/core/log.h>
#include <linuxdeploy/subprocess/pipe_reader.h>

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace plugin {
        using namespace core::log;

        plugin_process_handler::plugin_process_handler(std::string name, bf::path path) : name_(std::move(name)), path_(std::move(path)) {}

        int plugin_process_handler::run(const bf::path& appDir) const {
            // prepare arguments and environment variables
            const std::initializer_list<std::string> args = {path_.string(), "--appdir", appDir.string()};

            subprocess::subprocess_env_map_t environmentVariables{};

            // add $LINUXDEPLOY, which points to the current binary
            // we do not need to pass $APPIMAGE or alike, since while linuxdeploy is running, the path in the
            // temporary mountpoint of its AppImage will be valid anyway
            environmentVariables["LINUXDEPLOY"] = linuxdeploy::util::getOwnExecutablePath();

            linuxdeploy::subprocess::process proc{args, environmentVariables};

            // we want to insert a custom log prefix whenever a CR or LF is written into either buffer
            // like in subprocess's check_output, we use pipe readers to read from the subprocess's stdout/stderr pipes
            // however, we just dump everything we receive directly in the log, using our ĺogging framework
            // we store an ldLog instance per stream so we can just send all data into those, which allows us to get away
            // with relatively small buffers (we don't have to cache complete lines or alike)
            // parameter order: pipe reader, log type (used in prefix), ldLog instance, first message
            std::array<std::tuple<pipe_reader, std::string, ldLog, bool>, 2> pipes_to_be_logged{
                std::make_tuple(pipe_reader(proc.stdout_fd()), "stdout", ldLog{}, true),
                std::make_tuple(pipe_reader(proc.stderr_fd()), "stderr", ldLog{}, true),
            };

            for (;;) {
                for (auto& tuple : pipes_to_be_logged) {
                    // make code in this loop more readable
                    auto& reader = std::get<0>(tuple);
                    const auto& stream_name = std::get<1>(tuple);
                    auto& log = std::get<2>(tuple);
                    auto& is_first_message = std::get<3>(tuple);

                    const auto log_prefix = "[" + name_ + "/" + stream_name + "] ";

                    // since we have our own ldLog instance for every pipe, we can get away with this rather small read buffer
                    subprocess::subprocess_result_buffer_t intermediate_buffer(4096);

                    // (try to) read from pipe
                    const auto bytes_read = reader.read(intermediate_buffer);

                    // no action required in case we have not read anything from the pipe
                    if (bytes_read > 0) {
                        // special handling for the first message
                        if (is_first_message) {
                            log << log_prefix;
                            is_first_message = false;
                        }

                        // we just trim the buffer to the bytes we read (makes the code below easier)
                        intermediate_buffer.resize(bytes_read);

                        // all we have to do now is to look for CR or LF, send everything up to that location into the ldLog instance,
                        // write our prefix and then repeat
                        for (auto it = intermediate_buffer.begin(); it != intermediate_buffer.end(); ++it) {
                            const auto next_lf = std::find(it, intermediate_buffer.end(), '\n');
                            const auto next_cr = std::find(it, intermediate_buffer.end(), '\r');

                            // we don't care which one goes first -- we pick the closest one, write everything up to it into our ldLog,
                            // then print our prefix and repeat that until there's nothing left in our buffer
                            auto next_control_char = std::min({next_lf, next_cr});

                            if (next_control_char == intermediate_buffer.end()) {
                                break;
                            }

                            // need to make sure we include the control char in the write
                            log.write(
                                intermediate_buffer.data() + std::distance(intermediate_buffer.begin(), it),
                                std::distance(it, next_control_char) + 1
                            );

                            log << log_prefix;

                            it = next_control_char;
                        }
                    }
                }

                // do-while might be a little more elegant, but we can save this one unnecessary sleep, so...
                if (proc.is_running()) {
                    // reduce load on CPU
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                } else {
                    break;
                }
            }

            return proc.close();
        }

    }
}
