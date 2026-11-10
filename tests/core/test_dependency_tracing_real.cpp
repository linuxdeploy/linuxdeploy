#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/appdir.h"
#include "linuxdeploy/core/elf_file.h"
#include "linuxdeploy/subprocess/subprocess.h"
#include "linuxdeploy/util/util.h"
#include "test_util.h"
#include "dependency_tracing_recording.h"
#include "dependency_tracing_scenarios.h"

namespace fs = std::filesystem;
using namespace linuxdeploy::core::elf_file;
using namespace dependency_tracing_test;

namespace {
    // scenarios which apply only to some platforms are skipped on the others, so they have no recording there
#ifdef __GLIBC__
    constexpr bool usesGlibc = true;
#else
    constexpr bool usesGlibc = false;
#endif

    // musl defines no macro to detect it, but is the only other C library on Linux the fixtures are built with
#if defined(__linux__) && !defined(__GLIBC__)
    constexpr bool usesMusl = true;
#else
    constexpr bool usesMusl = false;
#endif

    // every ldd call is recorded, and printed if the test fails
    // set LINUXDEPLOY_LDD_RECORD_DIR to save the recordings of all scenarios to this directory
    class DependencyTracingTestReal : public testing::Test, public Tracer {
    protected:
        fs::path recordingDirectory;
        std::optional<std::string> oldLibraryPath;
        std::string oldPath;
        // an unset $PATH is restored as unset, not as empty
        bool pathWasSet = false;
        bool isScenario = false;

        void SetUp() override {
            // saved first, as TearDown() restores it even if SetUp() fails
            pathWasSet = getenv("PATH") != nullptr;
            oldPath = pathWasSet ? getenv("PATH") : "";
            if (const auto* path = getenv("LD_LIBRARY_PATH"))
                oldLibraryPath = path;
            ASSERT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);

            setFixtureDirectory(make_temporary_directory(
                (fs::temp_directory_path() / "linuxdeploy-ldd-tests-real-XXXXXX").string()));
            fs::copy_file(SIMPLE_EXECUTABLE_PATH, executable);
            fs::copy_file(SIMPLE_LIBRARY_PATH, simple);
            fs::copy_file(DEPENDENCY_PARENT_LIBRARY_PATH, parent);
            fs::copy_file(DEPENDENCY_LEAF_LIBRARY_PATH, leaf);

            // remove the build tree's search paths, so deleting a copy really makes it missing
            // every library needs its own RUNPATH, as glibc does not use the executable's for transitive dependencies
            for (const auto& path : {executable, simple, parent, leaf})
                ASSERT_TRUE(ElfFile(path).setRPath("$ORIGIN"));
            copyAsUnloadable(DEPENDENCY_LEAF_LIBRARY_PATH, unloadableLeaf);

            const auto ldd = linuxdeploy::util::which("ldd");
            ASSERT_FALSE(ldd.empty()) << "the real tests need ldd";
            recordingDirectory = make_temporary_directory(
                (fs::temp_directory_path() / "linuxdeploy-ldd-recording-XXXXXX").string());
            writeRecordingLdd(recordingDirectory, ldd.parent_path());
            ASSERT_EQ(setenv("PATH", (recordingDirectory.string() + ":" + oldPath).c_str(), 1), 0);
        }

        std::vector<fs::path> trace(const fs::path& path, const std::vector<std::string>& exclusions = {}) override {
            auto dependencies = ElfFile(path).traceDynamicDependencies(exclusions);
            // unlike in the mock tests, the libraries really exist
            for (const auto& dependency : dependencies)
                EXPECT_TRUE(fs::exists(dependency)) << dependency;
            return dependencies;
        }

        bool deployDependencies(const fs::path& path) override {
            const auto appDirPath = directory / "AppDir";
            fs::create_directories(appDirPath / "usr/lib");
            fs::copy_file(path, appDirPath / "usr/lib" / path.filename());
            return linuxdeploy::core::appdir::AppDir(appDirPath).deployDependenciesForExistingFiles();
        }

        void runScenario(void (*scenario)(Tracer&)) {
            isScenario = true;
            scenario(*this);
        }

        // trace without ldd in $PATH
        std::vector<fs::path> traceWithoutLdd(const fs::path& path) {
            const auto emptyPath = directory / "empty-path";
            fs::create_directories(emptyPath);
            EXPECT_EQ(setenv("PATH", emptyPath.c_str(), 1), 0);
            struct RestorePath {
                const fs::path& recordingDirectory;
                const std::string& value;
                ~RestorePath() { setenv("PATH", (recordingDirectory.string() + ":" + value).c_str(), 1); }
            } restorePath{recordingDirectory, oldPath};
            return ElfFile(path).traceDynamicDependencies();
        }

        void reportRecording() {
            // a scenario without ldd calls still gets an empty recording,
            // so the mock tests check that it makes none, but a skipped scenario has nothing to replay
            const auto calls = readRecordedCalls(recordingDirectory, directory);
            if ((calls.empty() && !isScenario) || IsSkipped())
                return;

            const std::string name = testing::UnitTest::GetInstance()->current_test_info()->name();
            const auto recording = formatRecording(calls);

            if (HasFailure()) {
                std::cout << "\n=== ldd recording";
                if (isScenario)
                    std::cout << ", for the mock tests save as tests/core/ldd_recordings/<platform>/" << name << ".txt";
                std::cout << " ===\n" << recording << "=== end of ldd recording ===\n" << std::flush;
            }

            if (const auto* outputDirectory = getenv("LINUXDEPLOY_LDD_RECORD_DIR"); outputDirectory != nullptr && isScenario) {
                fs::create_directories(outputDirectory);
                std::ofstream file(fs::path(outputDirectory) / (name + ".txt"));
                file << recording;
                EXPECT_TRUE(file.good()) << "could not save the ldd recording";
            }
        }

        void TearDown() override {
            if (!recordingDirectory.empty() && !directory.empty())
                reportRecording();
            if (pathWasSet)
                EXPECT_EQ(setenv("PATH", oldPath.c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("PATH"), 0);
            if (oldLibraryPath)
                EXPECT_EQ(setenv("LD_LIBRARY_PATH", oldLibraryPath->c_str(), 1), 0);
            else
                EXPECT_EQ(unsetenv("LD_LIBRARY_PATH"), 0);
            for (const auto& path : {directory, recordingDirectory}) {
                if (!path.empty())
                    fs::remove_all(path);
            }
        }
    };

    // === Dependency discovery and runtime filtering ===

    TEST_F(DependencyTracingTestReal, LibraryDependencies_FindDirectAndTransitive) {
        const auto result = linuxdeploy::subprocess::subprocess({executable.string()}).run();
        ASSERT_EQ(result.exit_code(), 0) << result.stderr_string();
        runScenario(scenarios::LibraryDependencies_FindDirectAndTransitive);
    }

    TEST_F(DependencyTracingTestReal, LibraryDependencies_DeployNeededByPath) {
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", leaf.filename().string(), fs::canonical(leaf).string(), parent.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
        runScenario(scenarios::LibraryDependencies_DeployNeededByPath);
    }

    TEST_F(DependencyTracingTestReal, RuntimeDependencies_FilterRuntimeObjects) {
        runScenario(scenarios::RuntimeDependencies_FilterRuntimeObjects);
    }

    // add glibc's sonames of libraries which musl provides itself (e.g., libm.so.6)
    void addMuslAliases(const fs::path& path) {
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--add-needed", "libm.so.6",
            "--add-needed", "libpthread.so.0",
            "--add-needed", "libdl.so.2",
            "--add-needed", "librt.so.1",
            "--add-needed", "libutil.so.1",
            path.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
    }

    TEST_F(DependencyTracingTestReal, RuntimeDependencies_FilterMuslAliases) {
#ifndef __linux__
        GTEST_SKIP() << "the aliases are sonames of glibc's libraries";
#else
        ASSERT_NO_FATAL_FAILURE(addMuslAliases(parent));
        runScenario(scenarios::RuntimeDependencies_FilterMuslAliases);
#endif
    }

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveOrdinaryMuslAliases) {
        if (!usesGlibc)
            GTEST_SKIP() << "only glibc provides the aliases as libraries of their own";
        ASSERT_NO_FATAL_FAILURE(addMuslAliases(parent));
        runScenario(scenarios::LibraryNames_PreserveOrdinaryMuslAliases);
    }

    // === Library names ===

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveMuslLikeNames) {
        const auto libraryDir = directory / "ld-musl-testing";
        fs::create_directory(libraryDir);
        const auto helper = directory / "libdependency_parent-musl.so";
        fs::rename(leaf, libraryDir / leaf.filename());
        fs::rename(parent, helper);
        ASSERT_TRUE(ElfFile(helper).setRPath("$ORIGIN/ld-musl-testing"));
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", "libdependency_parent.so", "libdependency_parent-musl.so", executable.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
        runScenario(scenarios::LibraryNames_PreserveMuslLikeNames);
    }

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveGlibcLoaderLikeNames) {
        const auto libraryDir = directory / "build-linux-test";
        fs::create_directory(libraryDir);
        fs::rename(leaf, libraryDir / leaf.filename());
        ASSERT_TRUE(ElfFile(parent).setRPath("$ORIGIN/build-linux-test"));
        runScenario(scenarios::LibraryNames_PreserveGlibcLoaderLikeNames);
    }

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveLoaderLikeFileNames) {
        fs::rename(simple, directory / "ld.so.1");
        const auto patchExecutable = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", simple.filename().string(), "ld.so.1", executable.string()}).run();
        ASSERT_EQ(patchExecutable.exit_code(), 0) << patchExecutable.stderr_string();

        const auto helper = directory / "ld-musl-helper.so";
        fs::rename(leaf, helper);
        const auto patchParent = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", leaf.filename().string(), fs::canonical(helper).string(), parent.string()}).run();
        ASSERT_EQ(patchParent.exit_code(), 0) << patchParent.stderr_string();

        runScenario(scenarios::LibraryNames_PreserveLoaderLikeFileNames);
    }

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveLibraryNamedLdd) {
        fs::rename(simple, directory / "ldd");
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", simple.filename().string(), "ldd", executable.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
        runScenario(scenarios::LibraryNames_PreserveLibraryNamedLdd);
    }

    TEST_F(DependencyTracingTestReal, LibraryNames_PreserveNotFoundInPath) {
        const auto libraryDir = directory / "not found";
        fs::create_directory(libraryDir);
        fs::rename(leaf, libraryDir / leaf.filename());
        ASSERT_TRUE(ElfFile(parent).setRPath("$ORIGIN/not found"));
        runScenario(scenarios::LibraryNames_PreserveNotFoundInPath);
    }

    // === Missing dependencies and exclusions ===

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectDirect) {
        ASSERT_TRUE(fs::remove(parent));
        runScenario(scenarios::MissingDependencies_RejectDirect);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectLibraryNamedLdd) {
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", simple.filename().string(), "ldd", executable.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
        ASSERT_TRUE(fs::remove(simple));
        runScenario(scenarios::MissingDependencies_RejectLibraryNamedLdd);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_AllowExcludedDirect) {
        ASSERT_TRUE(fs::remove(parent));
        runScenario(scenarios::MissingDependencies_AllowExcludedDirect);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectTransitive) {
        ASSERT_TRUE(fs::remove(leaf));
        runScenario(scenarios::MissingDependencies_RejectTransitive);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_AllowExcludedTransitive) {
        ASSERT_TRUE(fs::remove(leaf));
        runScenario(scenarios::MissingDependencies_AllowExcludedTransitive);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_AllowWildcardExclusion) {
        ASSERT_TRUE(fs::remove(leaf));
        runScenario(scenarios::MissingDependencies_AllowWildcardExclusion);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_AllowExcludedNeededByPath) {
        const auto patch = linuxdeploy::subprocess::subprocess({TEST_PATCHELF_PATH,
            "--replace-needed", leaf.filename().string(), fs::canonical(leaf).string(), parent.string()}).run();
        ASSERT_EQ(patch.exit_code(), 0) << patch.stderr_string();
        ASSERT_TRUE(fs::remove(leaf));
        runScenario(scenarios::MissingDependencies_AllowExcludedNeededByPath);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectPartiallyExcluded) {
        ASSERT_TRUE(fs::remove(parent));
        ASSERT_TRUE(fs::remove(simple));
        runScenario(scenarios::MissingDependencies_RejectPartiallyExcluded);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectUnrelatedExclusion) {
        ASSERT_TRUE(fs::remove(leaf));
        runScenario(scenarios::MissingDependencies_RejectUnrelatedExclusion);
    }

    TEST_F(DependencyTracingTestReal, MissingDependencies_RejectExcludedLoadError) {
        if (!usesMusl)
            GTEST_SKIP() << "glibc's and FreeBSD's ldd report libraries they cannot load as missing";
        ASSERT_TRUE(ElfFile(parent).setRPath(unloadableLeaf.parent_path().string()));
        runScenario(scenarios::MissingDependencies_RejectExcludedLoadError);
    }

    // === Relocation errors ===

    TEST_F(DependencyTracingTestReal, RelocationErrors_Allow) {
        fs::copy_file(MISSING_SYMBOL_LIBRARY_PATH, leaf, fs::copy_options::overwrite_existing);
        // running the executable proves the symbol is really missing, on either libc
        const auto run = linuxdeploy::subprocess::subprocess({executable.string()}).run();
        EXPECT_NE(run.exit_code(), 0);
        runScenario(scenarios::RelocationErrors_Allow);

        // unlike glibc's ldd without -r, musl's ldd resolves symbols
        if (usesMusl) {
            const auto calls = readRecordedCalls(recordingDirectory, directory);
            ASSERT_FALSE(calls.empty());
            EXPECT_NE(calls.front().status, 0);
            EXPECT_THAT(calls.front().out, testing::HasSubstr("libdependency_parent.so"));
            EXPECT_THAT(calls.front().err, testing::HasSubstr("_Z21dependency_leaf_valuev: symbol not found"));
            // like a plugin's, the symbols the traced library itself needs are unresolved
            EXPECT_THAT(calls.back().err, testing::HasSubstr(
                "Error relocating /fixture/libdependency_parent.so: _Z21dependency_leaf_valuev: symbol not found"));
        }
    }

    // === Invalid programs and loader diagnostics ===

    TEST_F(DependencyTracingTestReal, InvalidPrograms_AcceptNonDynamicDiagnostic) {
        if (!usesGlibc)
            GTEST_SKIP() << "only glibc's ldd reports files it cannot load as not dynamic";
        runScenario(scenarios::InvalidPrograms_AcceptNonDynamicDiagnostic);
    }

    TEST_F(DependencyTracingTestReal, InvalidPrograms_RejectUnloadable) {
        if (usesGlibc)
            GTEST_SKIP() << "glibc's ldd reports files it cannot load as not dynamic";
        runScenario(scenarios::InvalidPrograms_RejectUnloadable);
    }

    // === Static programs ===

    TEST_F(DependencyTracingTestReal, StaticPrograms_SkipLdd) {
        fs::copy_file(SIMPLE_EXECUTABLE_STATIC_PATH, staticExecutable);
        runScenario(scenarios::StaticPrograms_SkipLdd);
    }

    TEST_F(DependencyTracingTestReal, StaticPrograms_StaticPie) {
#ifdef SIMPLE_EXECUTABLE_STATIC_PIE_PATH
        fs::copy_file(SIMPLE_EXECUTABLE_STATIC_PIE_PATH, staticPie);
#endif
        runScenario(scenarios::StaticPrograms_StaticPie);
    }

    // === Missing ldd ===

    TEST_F(DependencyTracingTestReal, MissingLdd_Reject) {
        EXPECT_THAT([&] { traceWithoutLdd(executable); },
                    testing::ThrowsMessage<DependencyTraceError>(testing::HasSubstr("Failed to find or run ldd")));
    }

    TEST_F(DependencyTracingTestReal, MissingLdd_RejectLddWhoseInterpreterIsMissing) {
        // exec* reports this like a missing ldd
        const auto brokenDirectory = directory / "broken";
        fs::create_directory(brokenDirectory);
        std::ofstream(brokenDirectory / "ldd") << "#!/nonexistent/interpreter\n";
        makeExecutable(brokenDirectory / "ldd");
        ASSERT_EQ(setenv("PATH", brokenDirectory.c_str(), 1), 0);

        EXPECT_THAT([&] { trace(executable); },
                    testing::ThrowsMessage<DependencyTraceError>(testing::HasSubstr("Failed to find or run ldd")));
    }

    TEST_F(DependencyTracingTestReal, MissingLdd_RejectLddWhichCannotBeRun) {
        // exec* skips an ldd which is not executable, and reports EACCES if it finds no other one
        const auto lddDirectory = directory / "not-executable";
        fs::create_directory(lddDirectory);
        std::ofstream(lddDirectory / "ldd") << "#!/bin/sh\n";
        ASSERT_EQ(setenv("PATH", lddDirectory.c_str(), 1), 0);

        EXPECT_THAT([&] { trace(executable); },
                    testing::ThrowsMessage<DependencyTraceError>(testing::AllOf(
                        testing::HasSubstr("Failed to run ldd on"),
                        testing::HasSubstr(std::string(": ") + strerror(EACCES)),
                        testing::Not(testing::HasSubstr("could not be run")))));
    }

    // the mock tests cannot install an ldd in the default search path, so this test has no mock
    // how exec* searches for programs is tested with subprocess, this checks that ldd is not searched for up front
    TEST_F(DependencyTracingTestReal, MissingLdd_UseDefaultPathIfUnset) {
        // like exec*, fall back to the C library's default search path, which contains ldd on all CI platforms
        ASSERT_EQ(unsetenv("PATH"), 0);

        EXPECT_THAT(trace(executable), testing::Contains(leaf));
    }

    // === Deployment errors ===

    TEST_F(DependencyTracingTestReal, DeploymentErrors_ReturnFailure) {
        ASSERT_TRUE(ElfFile(parent).setRPath(unloadableLeaf.parent_path().string()));
        runScenario(scenarios::DeploymentErrors_ReturnFailure);
    }
}
