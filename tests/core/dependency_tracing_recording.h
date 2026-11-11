#pragma once

#include <cctype>
#include <filesystem>
#include <fstream>
#include <istream>
#include <iterator>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

// the real tests record the ldd calls they make, and the mock tests replay these recordings
// recordings are plain text, so the recording a failing real test prints can be saved as is
namespace dependency_tracing_test {
    namespace fs = std::filesystem;

    // recordings refer to the test's files by this directory
    inline const fs::path fixtureDirectory = "/fixture";

    struct LddCall {
        std::string target;
        int status = 0;
        std::string out;
        std::string err;
    };

    // replace the test's directory and the addresses, which differ between runs
    inline std::string normalize(std::string value, const fs::path& directory) {
        for (const auto& prefix : {fs::canonical(directory).string(), directory.string()}) {
            for (std::size_t pos = 0; (pos = value.find(prefix, pos)) != std::string::npos;) {
                value.replace(pos, prefix.size(), fixtureDirectory.string());
                pos += fixtureDirectory.string().size();
            }
        }
        return std::regex_replace(value, std::regex("0x[0-9a-fA-F]+"), "0x1000");
    }

    inline std::string formatRecording(const std::vector<LddCall>& calls) {
        auto withNewline = [](const std::string& value) {
            return value.empty() || value.back() == '\n' ? value : value + '\n';
        };

        std::ostringstream recording;
        for (const auto& call : calls) {
            recording << "# ldd " << call.target << "\n"
                      << "# exit status " << call.status << "\n"
                      << "# stdout\n" << withNewline(call.out)
                      << "# stderr\n" << withNewline(call.err);
        }
        return recording.str();
    }

    inline std::vector<LddCall> parseRecording(std::istream& recording) {
        std::vector<LddCall> calls;
        std::string* section = nullptr;

        for (std::string line; std::getline(recording, line);) {
            if (line.rfind("# ldd ", 0) == 0) {
                calls.emplace_back();
                calls.back().target = line.substr(6);
                section = nullptr;
            } else if (!calls.empty() && line.rfind("# exit status ", 0) == 0) {
                calls.back().status = std::stoi(line.substr(14));
            } else if (!calls.empty() && line == "# stdout") {
                section = &calls.back().out;
            } else if (!calls.empty() && line == "# stderr") {
                section = &calls.back().err;
            } else if (section != nullptr) {
                *section += line + '\n';
            } else {
                throw std::runtime_error("Invalid line in ldd recording: " + line);
            }
        }

        return calls;
    }

    inline void makeExecutable(const fs::path& path) {
        fs::permissions(path, fs::perms::owner_exec, fs::perm_options::add);
    }

    // replaces fixtureDirectory with directory where it starts a path in value (e.g., in ldd's output or $LD_PRELOAD)
    inline std::string replaceFixtureDirectory(std::string value, const std::string& directory) {
        const auto& fixture = fixtureDirectory.string();
        auto isNameCharacter = [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '/' || c == '_' || c == '-' || c == '.';
        };
        for (std::size_t pos = 0; (pos = value.find(fixture, pos)) != std::string::npos;) {
            const auto end = pos + fixture.size();
            if ((pos > 0 && isNameCharacter(value[pos - 1])) ||
                (end < value.size() && value[end] != '/' && isNameCharacter(value[end]))) {
                pos = end;
                continue;
            }
            value.replace(pos, fixture.size(), directory);
            pos += directory.size();
        }
        return value;
    }

    // install mock_ldd.sh as the ldd in directory, which replays the calls in order, checks each call's target unless
    // it is empty, and fails if it is called more often; paths in fixtureDirectory are mapped to fixtureCopies, in
    // the targets and the output, so that the output names existing files, like the real ldd's
    inline void writeReplayingLdd(const fs::path& directory, const std::vector<LddCall>& calls,
                                  const fs::path& fixtureCopies = {}) {
        const auto replayDirectory = directory / "replay";
        fs::remove_all(replayDirectory);
        fs::create_directory(replayDirectory);
        // ldd is called with canonical paths
        const auto copies = fixtureCopies.empty() ? std::string{} : fs::canonical(fixtureCopies).string();
        auto toCopies = [&](const std::string& value) {
            return copies.empty() ? value : replaceFixtureDirectory(value, copies);
        };
        for (std::size_t n = 0; n < calls.size(); ++n) {
            const auto prefix = (replayDirectory / std::to_string(n)).string();
            std::ofstream(prefix + ".target") << toCopies(calls[n].target) << '\n';
            std::ofstream(prefix + ".status") << calls[n].status << '\n';
            std::ofstream(prefix + ".stdout") << toCopies(calls[n].out);
            std::ofstream(prefix + ".stderr") << toCopies(calls[n].err);
        }

        fs::copy_file(MOCK_LDD_PATH, directory / "ldd", fs::copy_options::overwrite_existing);
        makeExecutable(directory / "ldd");
    }

    // the number of calls the ldd installed by writeReplayingLdd() has replayed
    inline std::size_t replayedCalls(const fs::path& directory) {
        std::size_t n = 0;
        while (fs::exists(directory / "replay" / (std::to_string(n) + ".replayed")))
            ++n;
        return n;
    }

    // install an ldd in directory which records every call to the ldd in lddDirectory
    // ldd is called by its name, as musl's ldd prints the name it is called by
    inline void writeRecordingLdd(const fs::path& directory, const fs::path& lddDirectory) {
        std::ofstream script(directory / "ldd");
        script << "#!/bin/sh\n"
               << "recording_dir=${0%/*}\n"
               << "n=0\n"
               << "while [ -e \"$recording_dir/$n.status\" ]; do n=$((n + 1)); done\n"
               << "printf '%s\\n' \"$*\" > \"$recording_dir/$n.target\"\n"
               << "PATH='" << lddDirectory.string() << "':$PATH ldd \"$@\" "
               << "> \"$recording_dir/$n.stdout\" 2> \"$recording_dir/$n.stderr\"\n"
               << "status=$?\n"
               << "printf '%s\\n' \"$status\" > \"$recording_dir/$n.status\"\n"
               << "cat \"$recording_dir/$n.stdout\"\n"
               << "cat \"$recording_dir/$n.stderr\" >&2\n"
               << "exit \"$status\"\n";
        script.close();
        makeExecutable(directory / "ldd");
    }

    // read the calls recorded by writeRecordingLdd(), and normalize the paths in directory
    inline std::vector<LddCall> readRecordedCalls(const fs::path& recordingDirectory, const fs::path& directory) {
        auto read = [](const fs::path& path) {
            std::ifstream file(path);
            return std::string{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        };

        std::vector<LddCall> calls;
        for (int n = 0; fs::exists(recordingDirectory / (std::to_string(n) + ".status")); ++n) {
            const auto prefix = recordingDirectory / std::to_string(n);
            LddCall call;
            call.target = normalize(read(prefix.string() + ".target"), directory);
            call.target.erase(call.target.find_last_not_of('\n') + 1);
            call.status = std::stoi(read(prefix.string() + ".status"));
            call.out = normalize(read(prefix.string() + ".stdout"), directory);
            call.err = normalize(read(prefix.string() + ".stderr"), directory);
            calls.push_back(call);
        }
        return calls;
    }
}
