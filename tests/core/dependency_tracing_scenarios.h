#pragma once

#include <cstddef>
#include <elf.h>
#include <filesystem>
#include <fstream>
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

    namespace scenarios {
        using namespace linuxdeploy::core::elf_file;
        using testing::Contains;

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

        // the real test runs only with glibc, whose ldd reports files it cannot load as not dynamic
        inline void InvalidPrograms_AcceptNonDynamicDiagnostic(Tracer& t) {
            EXPECT_THAT(t.trace(t.unloadableLeaf), testing::IsEmpty());
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
    }
}
