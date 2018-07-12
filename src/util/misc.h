#pragma once

#include <algorithm>
#include <climits>
#include <cstring>
#include <sstream>
#include <string>
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
        }
    }
}
