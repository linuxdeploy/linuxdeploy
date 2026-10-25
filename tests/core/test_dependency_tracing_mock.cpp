#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/elf_file.h"
#include "test_util.h"
#include "dependency_tracing_recording.h"
#include "dependency_tracing_scenarios.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::core::elf_file;

namespace {
    // === Recorded scenarios ===

    // replays the ldd calls from ldd_recordings/<platform>/<scenario>.txt
    class DependencyTracingTestRecorded : public testing::TestWithParam<std::string>,
                                          public dependency_tracing_test::Tracer {
    protected:
        fs::path fakeLddDirectory;
        fs::path copiesDirectory;
        std::optional<std::string> oldPath;
        std::vector<dependency_tracing_test::LddCall> calls;

        void SetUp() override {
            setFixtureDirectory(dependency_tracing_test::fixtureDirectory);
            std::string scenario = testing::UnitTest::GetInstance()->current_test_info()->name();
            scenario.erase(scenario.find('/'));
            const auto recording = fs::path(LDD_RECORDINGS_DIR) / GetParam() / (scenario + ".txt");

            // the real tests skip scenarios which do not apply to their platform, so there is no recording
            if (!fs::exists(recording))
                GTEST_SKIP() << "no recording for this platform: " << recording;

            std::ifstream file(recording);
            calls = dependency_tracing_test::parseRecording(file);
            if (const auto* path = getenv("PATH"))
                oldPath = path;
            fakeLddDirectory = make_temporary_directory(
                (fs::temp_directory_path() / "linuxdeploy-ldd-tests-recorded-XXXXXX").string());
            copiesDirectory = fakeLddDirectory / "fixture";

            fs::create_directory(copiesDirectory);
            fs::copy_file(SIMPLE_EXECUTABLE_PATH, copy(executable));
            fs::copy_file(SIMPLE_LIBRARY_PATH, copy(simple));
            fs::copy_file(DEPENDENCY_PARENT_LIBRARY_PATH, copy(parent));
            fs::copy_file(DEPENDENCY_LEAF_LIBRARY_PATH, copy(leaf));

            dependency_tracing_test::copyAsUnloadable(DEPENDENCY_LEAF_LIBRARY_PATH, copy(unloadableLeaf));
            fs::copy_file(SIMPLE_EXECUTABLE_STATIC_PATH, copy(staticExecutable));
#ifdef SIMPLE_EXECUTABLE_STATIC_PIE_PATH
            fs::copy_file(SIMPLE_EXECUTABLE_STATIC_PIE_PATH, copy(staticPie));
#endif
            dependency_tracing_test::writeReplayingLdd(fakeLddDirectory, calls, copiesDirectory);

            ASSERT_EQ(setenv("PATH", fakeLddDirectory.c_str(), 1), 0);
        }

        // the copy of a fixture file
        fs::path copy(const fs::path& path) const {
            return copiesDirectory / path.lexically_relative(directory);
        }

        // the mock ldd checks that the copy is the file the recording expects
        // its output names the copies, so the dependencies are mapped back to fixtureDirectory
        std::vector<fs::path> trace(const fs::path& path, const std::vector<std::string>& exclusions = {}) override {
            const auto copies = fs::canonical(copiesDirectory).string();

            auto dependencies = ElfFile(copy(path)).traceDynamicDependencies(exclusions);
            for (auto& dependency : dependencies) {
                const auto relative = dependency.lexically_relative(copies);
                if (!relative.empty() && *relative.begin() != "..")
                    dependency = directory / relative;
            }
            return dependencies;
        }

        void TearDown() override {
            if (fakeLddDirectory.empty())
                return;
            EXPECT_EQ(dependency_tracing_test::replayedCalls(fakeLddDirectory), calls.size())
                << "the scenario does not make the recorded ldd calls";
            if (oldPath)
                EXPECT_EQ(setenv("PATH", oldPath->c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("PATH"), 0);
            fs::remove_all(fakeLddDirectory);
        }
    };

    // there are no recordings until the ldd calls of the real tests have been recorded
    std::vector<std::string> recordedPlatforms() {
        std::vector<std::string> platforms;
        if (!fs::is_directory(LDD_RECORDINGS_DIR))
            return platforms;
        for (const auto& entry : fs::directory_iterator(LDD_RECORDINGS_DIR)) {
            if (entry.is_directory())
                platforms.emplace_back(entry.path().filename().string());
        }
        std::sort(platforms.begin(), platforms.end());
        return platforms;
    }

    INSTANTIATE_TEST_SUITE_P(Recordings, DependencyTracingTestRecorded, testing::ValuesIn(recordedPlatforms()),
                             [](const testing::TestParamInfo<std::string>& info) {
                                 auto name = info.param;
                                 std::replace_if(name.begin(), name.end(), [](char c) { return !std::isalnum(c); }, '_');
                                 return name;
                             });
    GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(DependencyTracingTestRecorded);

    using namespace dependency_tracing_test::scenarios;

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveMuslLikeNames) {
        LibraryNames_PreserveMuslLikeNames(*this);
    }

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveNotFoundInPath) {
        LibraryNames_PreserveNotFoundInPath(*this);
    }

    TEST_P(DependencyTracingTestRecorded, InvalidPrograms_AcceptNonDynamicDiagnostic) {
        InvalidPrograms_AcceptNonDynamicDiagnostic(*this);
    }

    TEST_P(DependencyTracingTestRecorded, StaticPrograms_SkipLdd) {
        StaticPrograms_SkipLdd(*this);
    }

    TEST_P(DependencyTracingTestRecorded, StaticPrograms_StaticPie) {
        StaticPrograms_StaticPie(*this);
    }
}
