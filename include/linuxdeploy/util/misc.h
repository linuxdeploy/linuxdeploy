#pragma once

// system headers
#include <algorithm>
#include <climits>
#include <cstring>
#include <filesystem>
#include <fnmatch.h>
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

            // the tool findTool() selected, and where it came from for messages (e.g., "set in $STRIP")
            struct FoundTool {
                std::string path;
                std::string source;
            };

            // returns the tool to run: envVar if it is not empty (a name or a path), otherwise name next to the
            // linuxdeploy binary, otherwise name, for exec* to search in $PATH
            static FoundTool findTool(const char* envVar, const std::string& name) {
                // allows users to use a custom tool instead of the bundled one
                const auto* envTool = getenv(envVar);
                if (envTool != nullptr && *envTool != '\0')
                    return {envTool, std::string("set in $") + envVar};

                // without the binary's path, the bundled tool would be searched for in the current directory
                const auto ownPath = getOwnExecutablePath();
                if (!ownPath.empty()) {
                    const auto bundledTool = std::filesystem::path(ownPath).parent_path() / name;
                    if (std::filesystem::exists(bundledTool))
                        return {bundledTool.string(), "bundled with linuxdeploy"};
                }

                return {name, "searched for in $PATH"};
            }

            // finds a program like exec* does, but skips empty $PATH entries and finds nothing if $PATH is unset, so
            // run programs by name through subprocess rather than via the returned path
            static std::filesystem::path which(const std::string& name) {
                namespace fs = std::filesystem;

                auto isExecutable = [](const fs::path& binary) {
                    std::error_code ec;
                    return access(binary.c_str(), X_OK) == 0 && fs::is_regular_file(binary, ec);
                };

                // like exec*, check a name which contains a slash directly instead of searching $PATH
                const fs::path name_path(name);
                if (name.find('/') != std::string::npos)
                    return isExecutable(name_path) ? name_path : fs::path{};

                const auto* path = getenv("PATH");

                if (path == nullptr)
                    return "";

                for (const auto& binDir : split(path, ':')) {
                    // empty entries would search the current directory
                    if (binDir.empty())
                        continue;

                    // check the candidate directly, as searchable but unreadable directories cannot be listed
                    const auto binary = fs::path(binDir) / name;

                    if (isExecutable(binary))
                        return binary;
                }

                return {};
            }

            // returns a string vector splitted from envVar
            static std::vector<std::string> splitEnv(const char *envVar, char delimiter) {
                std::vector<std::string> result;
                const auto ret = getenv(envVar);
                if (ret) {
                    result = split(ret, delimiter);
                }
                return result;
            }

            static bool isInExcludelist(const std::filesystem::path& fileName, const std::vector<std::string> &excludeList) {
                for (const auto& excludePattern : excludeList) {
                    // simple string match is faster than using fnmatch
                    if (excludePattern == fileName)
                        return true;

                    auto fnmatchResult = fnmatch(excludePattern.c_str(), fileName.string().c_str(), FNM_PATHNAME);
                    switch (fnmatchResult) {
                        case 0:
                            return true;
                        case FNM_NOMATCH:
                            break;
                        default:
                            return false;
                    }
                }

                return false;
            }
        }
    }
}
