// system headers
#include <unordered_map>
#include <vector>
#include <signal.h>

// local headers
#include "linuxdeploy/subprocess/subprocess.h"

namespace linuxdeploy {
    namespace subprocess {
        class process {
        private:
            // child process ID
            int child_pid_ = -1;

            // pipes to child process's stdout/stderr
            int stdout_fd_ = -1;
            int stderr_fd_ = -1;

            // process exited
            bool exited_ = false;
            // exit code -- will be initialized by close()
            int exit_code_ = -1;
            // signal which terminated the process, 0 if it exited -- will be initialized by close()
            int term_signal_ = 0;
            // errno of the failed attempt to run the program, 0 if it could be run
            int exec_error_ = 0;

            // these constants help make the pipe code more readable
            static constexpr int READ_END_ = 0, WRITE_END_ = 1;

        public:
            /**
             * Create a child process.
             * This constructor passes the system environment to the child process.
             * @param args parameters for process
             */
            process(std::initializer_list<std::string> args);

            /**
             * Create a child process.
             * @param args parameters for process
             * @param env map of all environment variables (caller must include the existing vars if they want to)
             */
            process(std::initializer_list<std::string> args, const subprocess_env_map_t& env);

            /**
             * Create a child process.
             * This constructor passes the system environment to the child process.
             * @param args parameters for process
             */
            process(const std::vector<std::string>& args);

            /**
             * Create a child process.
             * @param args parameters for process
             * @param env additional environment variables (current environment will be copied)
             */
            process(const std::vector<std::string>& args, const subprocess_env_map_t& env);

            ~process();

            /**
             * @return child process's ID
             */
            int pid() const;

            /**
             * @return child process's stdout file descriptor ID
             */
            int stdout_fd() const;


            /**
             * @return child process's stderr file descriptor ID
             */
            int stderr_fd() const;

            /**
             * Close all pipes and wait for process to exit.
             * If process is not running any more, just returns exit code.
             * @return child process's exit code
             */
            int close();

            /**
             * Use close() or is_running() to wait for the process first.
             * @return signal which terminated the child process, or 0 if it exited
             */
            int term_signal() const;

            /**
             * If the program could not be run (e.g., it was not found in $PATH), the child process exits with code 127.
             * @return errno of the failed attempt to run the program (e.g., ENOENT), or 0 if it could be run
             */
            int exec_error() const;

            /**
             * Kill underlying process with given signal. By default, SIGTERM is used to end the process.
             */
            void kill(int signal = SIGTERM) const;

            /**
             * Check whether process is still alive. Use close() to fetch exit code.
             * @return true while process is alive, false otherwise
             */
            bool is_running();
        };
    }
}
