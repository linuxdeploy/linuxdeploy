// local includes
#include "linuxdeploy/core/log.h"

namespace linuxdeploy {
    namespace core {
        namespace log {
            LD_LOGLEVEL ldLog::verbosity = LD_INFO;

            void ldLog::setVerbosity(LD_LOGLEVEL verbosity) {
                ldLog::verbosity = verbosity;
            }

            ldLog::ldLog() {
                prependSpace = false;
                currentLogLevel = LD_INFO;
                logLevelSet = false;
            };

            ldLog::ldLog(bool prependSpace, bool logLevelSet, LD_LOGLEVEL logLevel) {
                this->prependSpace = prependSpace;
                this->currentLogLevel = logLevel;
                this->logLevelSet = logLevelSet;
            }

            void ldLog::checkPrependSpace() {
                if (prependSpace) {
                    stream << " ";
                    prependSpace = false;
                }
            }

            bool ldLog::checkVerbosity() {
//                std::cerr << "current: " << currentLogLevel << " verbosity: " << verbosity << std::endl;
                return (currentLogLevel >= verbosity);
            }

            ldLog ldLog::operator<<(const std::string& message) {
                if (checkVerbosity()) {
                    checkPrependSpace();
                    stream << message;
                }

                return ldLog(true, logLevelSet, currentLogLevel);
            }
            ldLog ldLog::operator<<(const char* message) {
                if (checkVerbosity()) {
                    checkPrependSpace();
                    stream << message;
                }

                return ldLog(true, logLevelSet, currentLogLevel);
            }

            ldLog ldLog::operator<<(const boost::filesystem::path& path) {
                if (checkVerbosity()) {
                    checkPrependSpace();
                    stream << path.string();
                }

                return ldLog(true, logLevelSet, currentLogLevel);
            }

            ldLog ldLog::operator<<(const double val) {
                return ldLog::operator<<(std::to_string(val));
            }

            ldLog ldLog::operator<<(stdEndlType strm) {
                if (checkVerbosity()) {
                    checkPrependSpace();
                    stream << strm;
                }

                return ldLog(false, logLevelSet, currentLogLevel);
            }

            ldLog ldLog::operator<<(const LD_LOGLEVEL logLevel) {
                if (logLevelSet) {
                    throw std::runtime_error(
                        "log level must be first element passed via the stream insertion operator");
                }

                logLevelSet = true;
                currentLogLevel = logLevel;

                if (checkVerbosity()) {
                    switch (logLevel) {
                        case LD_DEBUG:
                            stream << "DEBUG: ";
                            break;
                        case LD_WARNING:
                            stream << "WARNING: ";
                            break;
                        case LD_ERROR:
                            stream << "ERROR: ";
                            break;
                        default:
                            break;
                    }
                }

                return ldLog(false, logLevelSet, currentLogLevel);
            }

            ldLog ldLog::operator<<(const LD_STREAM_CONTROL streamControl) {
                bool prependSpace = true;

                switch (streamControl) {
                    case LD_NO_SPACE:
                        prependSpace = false;
                        break;
                    default:
                        break;
                }

                return ldLog(prependSpace, logLevelSet, currentLogLevel);
            }
        }
    }
}
