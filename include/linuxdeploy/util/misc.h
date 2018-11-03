#pragma once

#include <algorithm>
#include <climits>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>
#include <boost/filesystem.hpp>

namespace linuxdeploy {
    namespace util {
        namespace misc {
            static bool path_contains_file(boost::filesystem::path dir, boost::filesystem::path file) {
                // If dir ends with "/" and isn't the root directory, then the final
                // component returned by iterators will include "." and will interfere
                // with the std::equal check below, so we strip it before proceeding.
                if (dir.filename() == ".")
                    dir.remove_filename();
                // We're also not interested in the file's name.
                assert(file.has_filename());
                file.remove_filename();

                // If dir has more components than file, then file can't possibly
                // reside in dir.
                auto dir_len = std::distance(dir.begin(), dir.end());
                auto file_len = std::distance(file.begin(), file.end());
                if (dir_len > file_len)
                    return false;

                // This stops checking when it reaches dir.end(), so it's OK if file
                // has more directory components afterward. They won't be checked.
                return std::equal(dir.begin(), dir.end(), file.begin());
            };

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
