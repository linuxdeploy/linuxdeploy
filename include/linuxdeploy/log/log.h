// system includes
#include <filesystem>
#include <iostream>

#pragma once

namespace linuxdeploy::log {
    enum LD_LOGLEVEL {
        LD_DEBUG = 0,
        LD_INFO,
        LD_WARNING,
        LD_ERROR
    };

    enum LD_STREAM_CONTROL {
        LD_NOOP = 0,
        LD_NO_SPACE,
    };

    class ldLog {
        private:
            // this is the type of std::cout
            typedef std::basic_ostream<char, std::char_traits<char> > CoutType;

            // this is the function signature of std::endl
            typedef CoutType& (* stdEndlType)(CoutType&);

        private:
            static LD_LOGLEVEL verbosity;

        private:
            bool prependSpace;
            bool logLevelSet;
            CoutType& stream = std::cout;

            LD_LOGLEVEL currentLogLevel;

        private:
            // advanced behavior
            ldLog(bool prependSpace, bool logLevelSet, LD_LOGLEVEL logLevel);

            void checkPrependSpace();

            bool checkVerbosity();

        public:
            static void setVerbosity(LD_LOGLEVEL verbosity);

        public:
            // public constructor
            // does not implement the advanced behavior -- see private constructors for that
            ldLog();

        public:
            ldLog operator<<(const std::string& message);
            ldLog operator<<(const char* message);
            ldLog operator<<(const std::filesystem::path& path);
            ldLog operator<<(const int val);
            ldLog operator<<(const size_t val);
            ldLog operator<<(const double val);
            ldLog operator<<(stdEndlType strm);
            ldLog operator<<(const LD_LOGLEVEL logLevel);
            ldLog operator<<(const LD_STREAM_CONTROL streamControl);

            void write(const char* s, const size_t n);
    };
}
