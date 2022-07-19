// global headers
#include <unistd.h>

// local headers
#include "linuxdeploy/subprocess/util.h"
#include "linuxdeploy/util/misc.h"

namespace linuxdeploy::subprocess {
    subprocess_env_map_t get_environment() {
        subprocess_env_map_t result;

        // first, copy existing environment
        // we cannot reserve space in the vector unfortunately, as we don't know the size of environ before the iteration
        if (environ != nullptr) {
            for (auto** current_env_var = environ; *current_env_var != nullptr; ++current_env_var) {
                std::string current_env_var_str(*current_env_var);
                const auto first_eq = current_env_var_str.find_first_of('=');
                const auto env_var_name = current_env_var_str.substr(0, first_eq);
                const auto env_var_value = current_env_var_str.substr(first_eq);
                result[env_var_name] = env_var_value;
            }
        }

        return result;
    }
}
