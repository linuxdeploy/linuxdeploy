// library headers
#include <boost/lexical_cast.hpp>

// local headers
#include "linuxdeploy/core/log.h"
#include "linuxdeploy/core/desktopfile/desktopfileentry.h"

using boost::lexical_cast;

namespace linuxdeploy {
    namespace core {
        using namespace log;

        namespace desktopfile {
            class DesktopFileEntry::PrivateData {
            public:
                std::string key;
                std::string value;

            public:
                void copyData(const std::shared_ptr<PrivateData>& other) {
                    key = other->key;
                    value = other->value;
                }

                void assertValueNotEmpty() {
                    if (value.empty())
                        throw std::invalid_argument("value is empty");
                }
            };

            DesktopFileEntry::DesktopFileEntry() : d(new PrivateData) {}

            DesktopFileEntry::DesktopFileEntry(std::string key, std::string value) : DesktopFileEntry() {
                d->key = std::move(key);
                d->value = std::move(value);
            }

            DesktopFileEntry::DesktopFileEntry(const DesktopFileEntry& other) : DesktopFileEntry() {
                d->copyData(other.d);
            }

            DesktopFileEntry& DesktopFileEntry::operator=(const DesktopFileEntry& other) {
                if (this != &other) {
                    d.reset(new PrivateData);
                    d->copyData(other.d);
                }

                return *this;
            }

            DesktopFileEntry& DesktopFileEntry::operator=(DesktopFileEntry&& other) noexcept {
                if (this != &other) {
                    d = other.d;
                    other.d = nullptr;
                }

                return *this;
            }

            bool DesktopFileEntry::operator==(const DesktopFileEntry& other) const {
                return d->key == other.d->key && d->value == other.d->value;
            }

            bool DesktopFileEntry::operator!=(const DesktopFileEntry& other) const {
                return !operator==(other);
            }

            bool DesktopFileEntry::isEmpty() const {
                return d->key.empty();
            }

            const std::string& DesktopFileEntry::key() const {
                return d->key;
            }

            const std::string& DesktopFileEntry::value() const {
                return d->value;
            }

            int32_t DesktopFileEntry::asInt() const {
                d->assertValueNotEmpty();

                return lexical_cast<int32_t>(value());
            }

            int64_t DesktopFileEntry::asLong() const {
                d->assertValueNotEmpty();

                return lexical_cast<int64_t>(value());
            }

            double DesktopFileEntry::asDouble() const {
                d->assertValueNotEmpty();

                return lexical_cast<double>(value());
            }

            std::vector<std::string> DesktopFileEntry::parseStringList() const {
                const auto& value = this->value();

                if (value.empty())
                    return {};

                if (value.back() != ';')
                    ldLog() << LD_DEBUG << "desktop file string list does not end with semicolon:" << value
                            << std::endl;

                std::vector<std::string> list;

                std::stringstream ss(value);

                std::string currentVal;
                while (std::getline(ss, currentVal, ';')) {
                    // the last value will be empty, as in desktop files, lists shall end with a semicolon
                    // therefore we skip all empty values (assuming that empty values in lists in desktop files don't make sense anyway)
                    if (!currentVal.empty())
                        list.emplace_back(currentVal);
                }

                return list;
            }
        }
    }
};
