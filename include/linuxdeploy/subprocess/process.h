// system headers
#include <unordered_map>
#include <vector>

// local headers
#include "linuxdeploy/subprocess/subprocess.h"

namespace linuxdeploy {
    namespace subprocess {
        class process {
        private:
            // child process ID
            int child_pid_;

            // pipes to child process's stdout/stderr
            int stdout_fd_;
            int stderr_fd_;

            // process exited
            bool exited_ = false;
            // exit code -- will be initialized by close()
            int exit_code_;

            // these constants help make the pipe code more readable
            static constexpr int READ_END_ = 0, WRITE_END_ = 1;

            static std::vector<char*> make_args_vector_(const std::vector<std::string>& args) ;

            static std::vector<char*> make_env_vector_(const subprocess_env_map_t& env) ;

        public:
            /**
             * Create a child process.
             * @param args parameters for process
             * @param env additional environment variables (current environment will be copied)
             */
            process(std::initializer_list<std::string> args, const subprocess_env_map_t& env);

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
             * Close all pipes and wait for process to exit. If process was closed already, just returns exit code.
             * @return child process's exit code
             */
            int close();
        };
    }
}
