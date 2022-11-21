#include "test_util.h"

#include <string>
#include <string.h>
#include <vector>

std::filesystem::path make_temporary_directory(std::optional<std::string> pattern) {
    // note: previously, the (insecure) boost::filesystem::unique_path() was used to generate a temporary directory
    // this function was never integrated into std::filesystem, so we use the good ol' POSIX function mkdtemp
    static const std::string defaultPattern = "/tmp/linuxdeploy-tests-XXXXXX";

    const std::string tmpl = [pattern]() {
        if (pattern.has_value()) {
            return pattern.value();
        }
        return defaultPattern;
    }();

    std::vector<char> tmplC(tmpl.size() + 1);
    strcpy(tmplC.data(), tmpl.c_str());

    auto tmpDir = mkdtemp(tmplC.data());

    if (tmpDir == nullptr) {
        throw std::runtime_error("could not create temporary directory");
    }

    return tmpDir;
}
