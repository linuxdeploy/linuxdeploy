#include <string>
#include <unordered_map>
#include <vector>

namespace linuxdeploy::subprocess {
    using subprocess_env_map_t = std::unordered_map<std::string, std::string>;

    /**
     * Parse current process's environment and store the variables in a map.
     * @return map of environment variables
     */
    subprocess_env_map_t get_environment();
}
