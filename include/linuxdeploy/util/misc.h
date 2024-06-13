#pragma once

// system headers
#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace linuxdeploy {
    namespace util {
        namespace misc {
            static inline bool ltrim(std::string& s, char to_trim = ' ') {
                // TODO: find more efficient way to check whether elements have been removed
                size_t initialLength = s.length();
                s.erase(s.begin(), std::find_if(s.begin(), s.end(), [to_trim](int ch) {
                    return ch != to_trim;
                }));
                return s.length() < initialLength;
            }

            static inline bool rtrim(std::string& s, char to_trim = ' ') {
                // TODO: find more efficient way to check whether elements have been removed
                auto initialLength = s.length();
                s.erase(std::find_if(s.rbegin(), s.rend(), [to_trim](int ch) {
                    return ch != to_trim;
                }).base(), s.end());
                return s.length() < initialLength;
            }

            static inline bool trim(std::string& s, char to_trim = ' ') {
                // returns true if either modifies s
                auto ltrim_result = ltrim(s, to_trim);
                return rtrim(s, to_trim) && ltrim_result;
            }

            static std::vector<std::string> split(const std::string& s, char delim = ' ') {
                std::vector<std::string> result;

                std::stringstream ss(s);
                std::string item;

                while (std::getline(ss, item, delim)) {
                    result.push_back(item);
                }

                return result;
            }

            static std::vector<std::string> splitLines(const std::string& s) {
                return split(s, '\n');
            }

            static std::string join(const std::vector<std::string> &strings, const std::string &delimiter) {
                std::string result;
                for (size_t i = 0; i < strings.size(); i++) {
                    result += strings[i];

                    if (i != strings.size() - 1) {
                        result += delimiter;
                    }
                }
                return result;
            }

            static inline std::string strLower(std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
                return s;
            }

            static bool stringStartsWith(const std::string& string, const std::string& prefix) {
                // sanity check
                if (string.size() < prefix.size())
                    return false;

                return strncmp(string.c_str(), prefix.c_str(), prefix.size()) == 0;
            }

            static bool stringEndsWith(const std::string& string, const std::string& suffix) {
                // sanity check
                if (string.size() < suffix.size())
                    return false;

                return strcmp(string.c_str() + (string.size() - suffix.size()), suffix.c_str()) == 0;
            }

            static bool stringContains(const std::string& string, const std::string& part) {
                return string.find(part) != std::string::npos;
            }

            static std::string getOwnExecutablePath() {
                // FIXME: reading /proc/self/exe line is Linux specific
                std::vector<char> buf(PATH_MAX, '\0');

                if (readlink("/proc/self/exe", buf.data(), buf.size()) < 0) {
                    return "";
                }

                return buf.data();
            }

            // very simple but for our purposes good enough which like algorithm to find binaries in $PATH
            static std::filesystem::path which(const std::string& name) {
                namespace fs = std::filesystem;

                // check if name is an absolute path and a valid binary to run
                const fs::path name_path(name);
                if (name_path.is_absolute()
                    && fs::exists(name_path)
                    && (fs::status(name_path).permissions() & fs::perms::owner_exec) != fs::perms::none) {
                        return name_path;
                }

                const auto* path = getenv("PATH");

                if (path == nullptr)
                    return "";

                for (const auto& binDir : split(path, ':')) {
                    if (!fs::is_directory(binDir)) {
                        continue;
                    }

                    for (fs::directory_iterator it(binDir); it != fs::directory_iterator{}; ++it) {
                        const auto binary = it->path();

                        if (binary.filename() == name) {
                            // TODO: check if file is executable (skip otherwise)
                            return binary;
                        }
                    }
                }

                return {};
            }

            // sets value variable with value from envVar if exists
            static bool charFromEnv(const char *envVar, char &value) {
                const auto ret = getenv(envVar);
                if (ret && strlen(ret) == 1) {
                    value = ret[0];
                    return true;
                }
                return false;
            }

            // sets strings vector with strings from envVar if exists
            static bool stringVectorFromEnv(const char *envVar, char delimiter, std::vector<std::string> &strings) {
                const auto ret = getenv(envVar);
                if (ret) {
                    strings = split(ret, delimiter);
                    return true;
                }
                return false;
            }

            // set envVar with value from strings
            static bool stringVectorToEnv(const char *envVar, char delimiter, const std::vector<std::string> &strings) {
                const auto ret = join(strings, std::string{delimiter});
                return setenv(envVar, ret.c_str(), 1) == 0;
            }
        }
    }
}
