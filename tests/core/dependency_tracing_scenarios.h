#pragma once

#include <cstddef>
#include <elf.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "linuxdeploy/core/elf_file.h"

// scenarios which both the real tests and the mock tests run
// the real tests prepare the files (e.g., delete a library) and trace them with the system's ldd, the mock tests replay
// the recorded ldd calls of every platform, so a scenario must make the same calls in the same order every time
namespace dependency_tracing_test {
    namespace fs = std::filesystem;

    // copy an ELF file and change its type to ET_CORE, so that loaders refuse to load it, but ElfFile can still parse it
    // musl does not check the machine type, so a library for another architecture would not do
    inline void copyAsUnloadable(const fs::path& source, const fs::path& destination) {
        fs::create_directories(destination.parent_path());
        fs::copy_file(source, destination, fs::copy_options::overwrite_existing);

        std::fstream file(destination, std::ios::in | std::ios::out | std::ios::binary);
        file.seekg(EI_DATA);
        const auto byteOrder = file.get();
        // e_type has the same offset in both ELF classes, and is stored in the file's byte order
        const char type[2] = {static_cast<char>(byteOrder == ELFDATA2MSB ? 0 : ET_CORE),
                              static_cast<char>(byteOrder == ELFDATA2MSB ? ET_CORE : 0)};
        file.seekp(offsetof(Elf64_Ehdr, e_type));
        file.write(type, sizeof(type));
    }

    // the fixture files: simple_executable -> libsimple_library.so, libdependency_parent.so -> libdependency_leaf.so
    // unloadableLeaf is a copy of libdependency_leaf.so which loaders refuse to load, in a directory of its own
    // staticPie is empty if the toolchain cannot link static-pie executables
    class Tracer {
    public:
        fs::path directory, executable, simple, parent, leaf, unloadableLeaf;
        fs::path staticExecutable, staticPie;

        virtual std::vector<fs::path> trace(const fs::path& path, const std::vector<std::string>& exclusions = {}) = 0;

        // copy the file into an empty AppDir, and deploy its dependencies
        virtual bool deployDependencies(const fs::path& path) = 0;

    protected:
        ~Tracer() = default;

        void setFixtureDirectory(const fs::path& fixtureDirectory) {
            directory = fixtureDirectory;
            executable = directory / "simple_executable";
            simple = directory / fs::path(SIMPLE_LIBRARY_PATH).filename();
            parent = directory / fs::path(DEPENDENCY_PARENT_LIBRARY_PATH).filename();
            leaf = directory / fs::path(DEPENDENCY_LEAF_LIBRARY_PATH).filename();
            unloadableLeaf = directory / "unloadable" / leaf.filename();
            staticExecutable = directory / fs::path(SIMPLE_EXECUTABLE_STATIC_PATH).filename();
#ifdef SIMPLE_EXECUTABLE_STATIC_PIE_PATH
            staticPie = directory / fs::path(SIMPLE_EXECUTABLE_STATIC_PIE_PATH).filename();
#endif
        }
    };

    // returns what linuxdeploy logs to std::cout while function runs
    template<typename Function>
    std::string captureLog(Function&& function) {
        struct RedirectCout {
            std::streambuf* const old;
            explicit RedirectCout(std::ostream& to) : old(std::cout.rdbuf(to.rdbuf())) {}
            ~RedirectCout() { std::cout.rdbuf(old); }
        };

        std::ostringstream log;
        {
            RedirectCout redirect(log);
            function();
        }
        return log.str();
    }

    namespace scenarios {
        using namespace linuxdeploy::core::elf_file;
        using testing::Contains;
        using testing::Not;

        // the real test moves libdependency_leaf.so to ld-musl-testing/ and renames libdependency_parent.so
        inline void LibraryNames_PreserveMuslLikeNames(Tracer& t) {
            const auto dependencies = t.trace(t.executable);
            EXPECT_THAT(dependencies, Contains(t.directory / "libdependency_parent-musl.so"));
            EXPECT_THAT(dependencies, Contains(t.directory / "ld-musl-testing" / t.leaf.filename()));
        }

        // the real test moves libdependency_leaf.so to "not found/"
        // FreeBSD's ldd appends an address to missing libraries, which must not match paths containing "not found"
        inline void LibraryNames_PreserveNotFoundInPath(Tracer& t) {
            EXPECT_THAT(t.trace(t.executable), Contains(t.directory / "not found" / t.leaf.filename()));
        }

        // the real test renames libsimple_library.so to ldd
        inline void LibraryNames_PreserveLibraryNamedLdd(Tracer& t) {
            EXPECT_THAT(t.trace(t.executable), Contains(t.directory / "ldd"));
        }

        // the real test deletes libdependency_parent.so
        inline void MissingDependencies_RejectDirect(Tracer& t) {
            EXPECT_THROW(t.trace(t.executable), DependencyNotFoundError);
        }

        // the real test renames libsimple_library.so to ldd, and deletes it
        // FreeBSD's ldd lists libraries whose name does not start with lib in another format, which must not hide that
        // they are missing
        inline void MissingDependencies_RejectLibraryNamedLdd(Tracer& t) {
            EXPECT_THROW(t.trace(t.executable), DependencyNotFoundError);
        }

        // the real test deletes libdependency_parent.so
        inline void MissingDependencies_AllowExcludedDirect(Tracer& t) {
            const auto dependencies = t.trace(t.executable, {"libdependency_parent.so"});
            EXPECT_THAT(dependencies, Contains(t.simple));
            EXPECT_THAT(dependencies, Not(Contains(t.parent)));
            // libdependency_leaf is reachable only through libdependency_parent
            EXPECT_THAT(dependencies, Not(Contains(t.leaf)));
        }

        // the real test deletes libdependency_leaf.so
        inline void MissingDependencies_RejectTransitive(Tracer& t) {
            EXPECT_THROW(t.trace(t.executable), DependencyNotFoundError);
            EXPECT_THROW(t.trace(t.parent), DependencyNotFoundError);
        }

        // the real test deletes libdependency_leaf.so
        inline void MissingDependencies_AllowExcludedTransitive(Tracer& t) {
            const auto dependencies = t.trace(t.executable, {"libdependency_leaf.so"});
            EXPECT_THAT(dependencies, Contains(t.simple));
            EXPECT_THAT(dependencies, Contains(t.parent));
            EXPECT_THAT(dependencies, Not(Contains(t.leaf)));
        }

        // the real test deletes libdependency_leaf.so
        inline void MissingDependencies_AllowWildcardExclusion(Tracer& t) {
            const auto dependencies = t.trace(t.executable, {"libdependency_leaf.so*"});
            EXPECT_THAT(dependencies, Contains(t.parent));
            EXPECT_THAT(dependencies, Not(Contains(t.leaf)));
        }

        // the real test deletes libdependency_parent.so and libsimple_library.so
        inline void MissingDependencies_RejectPartiallyExcluded(Tracer& t) {
            EXPECT_THROW(t.trace(t.executable, {"libdependency_parent.so"}), DependencyNotFoundError);
        }

        // the real test deletes libdependency_leaf.so
        inline void MissingDependencies_RejectUnrelatedExclusion(Tracer& t) {
            EXPECT_THROW(t.trace(t.executable, {"libdependency_parent.so"}), DependencyNotFoundError);
        }

        // the real test runs only with glibc, whose ldd reports files it cannot load as not dynamic
        inline void InvalidPrograms_AcceptNonDynamicDiagnostic(Tracer& t) {
            EXPECT_THAT(t.trace(t.unloadableLeaf), testing::IsEmpty());
        }

        // the real test does not run with glibc, whose ldd reports files it cannot load as not dynamic
        inline void InvalidPrograms_RejectUnloadable(Tracer& t) {
            EXPECT_THROW(t.trace(t.unloadableLeaf), DependencyTraceError);
        }

        // the real test runs only with musl, and points libdependency_parent.so's RUNPATH to the unloadable leaf
        // musl's ldd fails to load it, rather than reporting it as missing like glibc's and FreeBSD's ldd
        // a library which exists but cannot be loaded is not a missing library, so excluding it must not hide the error
        inline void MissingDependencies_RejectExcludedLoadError(Tracer& t) {
            EXPECT_THAT([&] { t.trace(t.executable, {"libdependency_leaf.so"}); },
                        testing::ThrowsMessage<DependencyTraceError>(testing::AllOf(
                            testing::HasSubstr("Failed to run ldd"), testing::HasSubstr("stdout:"),
                            testing::HasSubstr("stderr:"), testing::HasSubstr("Exec format error"))));
        }

        // the real test replaces libdependency_leaf.so with a library which lacks the symbol libdependency_parent needs
        // every library exists, so the list of dependencies is complete on every platform
        // only musl's ldd resolves symbols, glibc's (without -r) and FreeBSD's do not notice the missing one
        // programs are accepted, too, as excluding a missing library leaves its symbols unresolved (see
        // MissingDependencies_AllowExcludedDirect)
        inline void RelocationErrors_Allow(Tracer& t) {
            for (const auto& exclusions : {std::vector<std::string>{},
                                           std::vector<std::string>{"libdependency_leaf.so"}}) {
                SCOPED_TRACE(exclusions.empty() ? "no exclusion" : "leaf excluded");
                std::vector<fs::path> dependencies;
                const auto log = captureLog([&] { dependencies = t.trace(t.executable, exclusions); });
                // musl reports the symbol libdependency_parent.so needs, the others nothing
                EXPECT_THAT(log, testing::AnyOf(
                    Not(testing::HasSubstr("unresolved")),
                    testing::HasSubstr("ignoring 1 unresolved symbol of it or its dependencies")));
                EXPECT_THAT(dependencies, Contains(t.simple));
                EXPECT_THAT(dependencies, Contains(t.parent));
                EXPECT_THAT(dependencies, Contains(t.leaf));
            }

            // libdependency_parent.so stands for a plugin, whose symbols the program loading it may provide
            std::vector<fs::path> dependencies;
            const auto log = captureLog([&] { dependencies = t.trace(t.parent); });
            EXPECT_THAT(dependencies, Contains(t.leaf));
            // musl's diagnostics are only logged at debug level
            EXPECT_THAT(log, Not(testing::HasSubstr("symbol not found")));
        }

        // static programs must not be traced with ldd, so the recordings are empty
        inline void StaticPrograms_SkipLdd(Tracer& t) {
            EXPECT_THAT(t.trace(t.staticExecutable), testing::IsEmpty());
        }

        // static-pie executables have a PT_DYNAMIC segment, but nothing to trace
        inline void StaticPrograms_StaticPie(Tracer& t) {
            if (t.staticPie.empty())
                GTEST_SKIP() << "toolchain cannot link static-pie executables";
            EXPECT_THAT(t.trace(t.staticPie), testing::IsEmpty());
        }

        // the real test points libdependency_parent.so's RUNPATH to the unloadable leaf, so the deployment must fail
        // the libraries the mock tests trace do not exist, so check that it fails because of the tracing error
        inline void DeploymentErrors_ReturnFailure(Tracer& t) {
            bool deployed;
            const auto log = captureLog([&] { deployed = t.deployDependencies(t.parent); });

            EXPECT_FALSE(deployed) << log;
            EXPECT_THAT(log, testing::AnyOf(testing::HasSubstr("Failed to run ldd"),
                                            testing::HasSubstr("Could not find dependency: libdependency_leaf.so")));
        }
    }
}
