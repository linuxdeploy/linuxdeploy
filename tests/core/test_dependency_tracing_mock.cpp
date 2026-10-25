#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/core/elf_file.h"
#include "test_util.h"
#include "dependency_tracing_recording.h"
#include "dependency_tracing_scenarios.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::core::elf_file;
using testing::ElementsAre;
using testing::HasSubstr;

namespace {
    class DependencyTracingTestMock : public testing::Test {
    protected:
        fs::path directory;
        std::optional<std::string> oldPath;

        void SetUp() override {
            if (const auto* path = getenv("PATH"))
                oldPath = path;
            directory = make_temporary_directory(
                (fs::temp_directory_path() / "linuxdeploy-ldd-tests-mock-XXXXXX").string());
            ASSERT_EQ(setenv("PATH", directory.c_str(), 1), 0);
        }

        std::vector<fs::path> trace(const fs::path& path,
                                    const std::vector<std::string>& exclusions = {}) {
            return ElfFile(path).traceDynamicDependencies(exclusions);
        }

        void TearDown() override {
            if (oldPath)
                EXPECT_EQ(setenv("PATH", oldPath->c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("PATH"), 0);
            fs::remove_all(directory);
        }

        void setLddOutput(const std::string& output, const std::string& error = "", int status = 0) {
            dependency_tracing_test::writeReplayingLdd(directory, {{"", status, output, error}});
        }
    };

    // === Dependency discovery and runtime filtering ===

    // musl lists the libraries it provides itself under the name it was run as, which may differ from the CI platforms'
    // (e.g., Android's musl is named libc_musl.so, or a wrapper may run it by another name)
    TEST_F(DependencyTracingTestMock, RuntimeDependencies_FilterRuntimeObjects_MuslLoaderNames) {
        for (const std::string loader : {"/lib/libc_musl.so", "/lib/libc.so", "/opt/musl/bin/musl-ldd"}) {
            SCOPED_TRACE(loader);
            setLddOutput("\t" + loader + " (0x1000)\n"
                         "\tlibc.so => " + loader + " (0x1000)\n"
                         "\tlibpthread.so.0 => " + loader + " (0x1000)\n"
                         "\tlibdependency_leaf.so => /fixture/libdependency_leaf.so (0x2000)\n");
            EXPECT_THAT(trace(DEPENDENCY_PARENT_LIBRARY_PATH),
                        ElementsAre(fs::path("/fixture/libdependency_leaf.so")));
        }
    }

    // === Library names ===

    // ldd lists a library by the name it was opened by, so no platform lists it under another library's file name
    TEST_F(DependencyTracingTestMock, LibraryNames_PreserveLibcFileNameOfOtherLibraries) {
        setLddOutput("\t/lib/ld-musl-x86_64.so.1 (0x1000)\n"
                     "\tlibdependency_leaf.so => /opt/libc.so (0x1000)\n");
        EXPECT_THAT(trace(DEPENDENCY_PARENT_LIBRARY_PATH), ElementsAre(fs::path("/opt/libc.so")));
    }

    // === Invalid programs and loader diagnostics ===

    // no platform's ldd fails without a diagnostic, but a non-zero exit status must not be ignored
    TEST_F(DependencyTracingTestMock, InvalidPrograms_RejectNonzeroExitWithoutStderr) {
        for (const std::string output : {"", "libdependency_leaf.so => /fixture/libdependency_leaf.so (0x1000)\n"}) {
            SCOPED_TRACE(output);
            setLddOutput(output, "", 127);
            EXPECT_THROW(trace(DEPENDENCY_PARENT_LIBRARY_PATH), DependencyTraceError);
        }
    }

    // hand-written, as no fixture needs a symbol through several relocations, which musl reports once each
    TEST_F(DependencyTracingTestMock, RelocationErrors_CountEachSymbolOnce) {
        setLddOutput("\tlibdependency_leaf.so => /fixture/libdependency_leaf.so (0x1000)\n",
                     "Error relocating /fixture/libdependency_leaf.so: first: symbol not found\n"
                     "Error relocating /fixture/libdependency_leaf.so: first: symbol not found\n"
                     "Error relocating /fixture/libdependency_leaf.so: second: symbol not found\n"
                     "Error relocating /fixture/libdependency_parent.so: first: symbol not found\n", 127);
        const auto log = dependency_tracing_test::captureLog([&] { trace(DEPENDENCY_PARENT_LIBRARY_PATH); });
        EXPECT_THAT(log, HasSubstr("ignoring 3 unresolved symbols"));
    }

    // no platform's ldd is terminated by a signal by itself, but its output may then be incomplete, even if it
    // contains only diagnostics which are accepted otherwise, like musl's unresolved symbols
    TEST_F(DependencyTracingTestMock, InvalidPrograms_RejectTerminationBySignal) {
        std::ofstream(directory / "ldd")
            << "#!/bin/sh\n"
            << "printf '\\tlibdependency_leaf.so => /fixture/libdependency_leaf.so (0x1000)\\n'\n"
            << "printf 'Error relocating %s: _Z21dependency_leaf_valuev: symbol not found\\n' \"$1\" >&2\n"
            << "kill -KILL $$\n";
        dependency_tracing_test::makeExecutable(directory / "ldd");

        EXPECT_THAT([&] { trace(DEPENDENCY_PARENT_LIBRARY_PATH); },
                    testing::ThrowsMessage<DependencyTraceError>(HasSubstr("terminated by signal")));
    }

    // === Missing ldd ===

    TEST_F(DependencyTracingTestMock, MissingLdd_Reject) {
        // the PATH contains only an empty fixture directory
        EXPECT_THAT([&] { trace(DEPENDENCY_PARENT_LIBRARY_PATH); },
                    testing::ThrowsMessage<DependencyTraceError>(HasSubstr("Failed to find or run ldd")));
    }

    TEST_F(DependencyTracingTestMock, MissingLdd_RejectLddWhoseInterpreterIsMissing) {
        // exec* reports this like a missing ldd
        const auto brokenDirectory = directory / "broken";
        fs::create_directory(brokenDirectory);
        std::ofstream(brokenDirectory / "ldd") << "#!/nonexistent/interpreter\n";
        dependency_tracing_test::makeExecutable(brokenDirectory / "ldd");
        ASSERT_EQ(setenv("PATH", brokenDirectory.c_str(), 1), 0);

        EXPECT_THAT([&] { trace(DEPENDENCY_PARENT_LIBRARY_PATH); },
                    testing::ThrowsMessage<DependencyTraceError>(HasSubstr("Failed to find or run ldd")));
    }

    TEST_F(DependencyTracingTestMock, MissingLdd_RejectLddWhichCannotBeRun) {
        // exec* skips an ldd which is not executable, and reports EACCES if it finds no other one
        const auto lddDirectory = directory / "not-executable";
        fs::create_directory(lddDirectory);
        std::ofstream(lddDirectory / "ldd") << "#!/bin/sh\n";
        ASSERT_EQ(setenv("PATH", lddDirectory.c_str(), 1), 0);

        EXPECT_THAT([&] { trace(DEPENDENCY_PARENT_LIBRARY_PATH); },
                    testing::ThrowsMessage<DependencyTraceError>(testing::AllOf(
                        HasSubstr("Failed to run ldd on"), HasSubstr(std::string(": ") + strerror(EACCES)),
                        testing::Not(HasSubstr("could not be run")))));
    }

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

        bool deployDependencies(const fs::path& path) override {
            const auto appDirPath = copy(directory / "AppDir");
            fs::create_directories(appDirPath / "usr/lib");
            fs::copy_file(copy(path), appDirPath / "usr/lib" / path.filename());
            return linuxdeploy::core::appdir::AppDir(appDirPath).deployDependenciesForExistingFiles();
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

    TEST_P(DependencyTracingTestRecorded, LibraryDependencies_FindDirectAndTransitive) {
        LibraryDependencies_FindDirectAndTransitive(*this);
    }

    TEST_P(DependencyTracingTestRecorded, RuntimeDependencies_FilterRuntimeObjects) {
        RuntimeDependencies_FilterRuntimeObjects(*this);
    }

    TEST_P(DependencyTracingTestRecorded, RuntimeDependencies_FilterMuslAliases) {
        RuntimeDependencies_FilterMuslAliases(*this);
    }

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveOrdinaryMuslAliases) {
        LibraryNames_PreserveOrdinaryMuslAliases(*this);
    }

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveMuslLikeNames) {
        LibraryNames_PreserveMuslLikeNames(*this);
    }

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveNotFoundInPath) {
        LibraryNames_PreserveNotFoundInPath(*this);
    }

    TEST_P(DependencyTracingTestRecorded, LibraryNames_PreserveLibraryNamedLdd) {
        LibraryNames_PreserveLibraryNamedLdd(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectDirect) {
        MissingDependencies_RejectDirect(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectLibraryNamedLdd) {
        MissingDependencies_RejectLibraryNamedLdd(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_AllowExcludedDirect) {
        MissingDependencies_AllowExcludedDirect(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectTransitive) {
        MissingDependencies_RejectTransitive(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_AllowExcludedTransitive) {
        MissingDependencies_AllowExcludedTransitive(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_AllowWildcardExclusion) {
        MissingDependencies_AllowWildcardExclusion(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectPartiallyExcluded) {
        MissingDependencies_RejectPartiallyExcluded(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectUnrelatedExclusion) {
        MissingDependencies_RejectUnrelatedExclusion(*this);
    }

    TEST_P(DependencyTracingTestRecorded, MissingDependencies_RejectExcludedLoadError) {
        MissingDependencies_RejectExcludedLoadError(*this);
    }

    TEST_P(DependencyTracingTestRecorded, RelocationErrors_Allow) {
        RelocationErrors_Allow(*this);
    }

    TEST_P(DependencyTracingTestRecorded, InvalidPrograms_AcceptNonDynamicDiagnostic) {
        InvalidPrograms_AcceptNonDynamicDiagnostic(*this);
    }

    TEST_P(DependencyTracingTestRecorded, InvalidPrograms_RejectUnloadable) {
        InvalidPrograms_RejectUnloadable(*this);
    }

    TEST_P(DependencyTracingTestRecorded, StaticPrograms_SkipLdd) {
        StaticPrograms_SkipLdd(*this);
    }

    TEST_P(DependencyTracingTestRecorded, StaticPrograms_StaticPie) {
        StaticPrograms_StaticPie(*this);
    }

    TEST_P(DependencyTracingTestRecorded, DeploymentErrors_ReturnFailure) {
        DeploymentErrors_ReturnFailure(*this);
    }
}
