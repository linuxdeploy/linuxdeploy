// system includes
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

// local headers
#include "linuxdeploy/util/util.h"
#include "linuxdeploy/core/desktopfile/desktopfileentry.h"
#include "linuxdeploy/core/desktopfile/exceptions.h"
#include "desktopfilereader.h"

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace desktopfile {
            class DesktopFileReader::PrivateData {
            public:
                bf::path path;
                DesktopFile::sections_t sections;

            public:
                bool isEmpty() {
                    return sections.empty();
                }

                void assertPathIsNotEmptyAndFileExists() {
                    if (path.empty())
                        throw IOError("empty path is not permitted");
                }

                void copyData(const std::shared_ptr<PrivateData>& other) {
                    path = other->path;
                }

                void parse(std::istream& file) {
                    std::string line;
                    bool first = true;

                    std::string currentSectionName;

                    while (std::getline(file, line)) {
                        if (first) {
                            first = false;
                            // said to allow handling of UTF-16/32 documents, not entirely sure why
                            if (line[0] == static_cast<std::string::value_type>(0xEF)) {
                                line.erase(0, 3);
                                return;
                            }
                        }

                        if (!line.empty()) {
                            auto len = line.length();
                            if (len > 0 &&
                                !((len >= 2 && (line[0] == '/' && line[1] == '/')) || (len >= 1 && line[0] == '#'))) {
                                if (line[0] == '[') {
                                    // this line apparently introduces a new section
                                    auto closingBracketPos = line.find_last_of(']');

                                    if (closingBracketPos == std::string::npos)
                                        throw ParseError("No closing ] bracket in section header");

                                    size_t length = len - 2;
                                    auto title = line.substr(1, closingBracketPos - 1);

                                    // set up the new section
                                    sections.insert(std::make_pair(title, DesktopFile::section_t()));
                                    currentSectionName = std::move(title);
                                } else {
                                    // we require at least one section to be present in the desktop file
                                    if (currentSectionName.empty())
                                        throw ParseError("No section in desktop file");

                                    // this line should be a normal key-value pair
                                    std::string key = line.substr(0, line.find('='));
                                    std::string value = line.substr(line.find('=') + 1, line.size());

                                    // we can strip away any sort of leading or trailing whitespace safely
                                    linuxdeploy::util::trim(key);
                                    linuxdeploy::util::trim(value);

                                    // empty keys are not allowed for obvious reasons
                                    if (key.empty())
                                        throw ParseError("Empty keys are not allowed");

                                    // who are we to judge whether empty values are an issue
                                    // that'd require checking the key names and implementing checks per key according to the
                                    // freedesktop.org spec
                                    sections[currentSectionName][key] = DesktopFileEntry(key, value);
                                }
                            }
                        }
                    }
                }
            };

            DesktopFileReader::DesktopFileReader() : d(new PrivateData) {}

            DesktopFileReader::DesktopFileReader(boost::filesystem::path path) : DesktopFileReader() {
                d->path = std::move(path);
                d->assertPathIsNotEmptyAndFileExists();

                std::ifstream ifs(d->path.string());
                if (!ifs)
                    throw IOError("could not open file: " + d->path.string());

                d->parse(ifs);
            }

            DesktopFileReader::DesktopFileReader(std::istream& is) : DesktopFileReader() {
                d->parse(is);
            }

            DesktopFileReader::DesktopFileReader(const DesktopFileReader& other) : DesktopFileReader() {
                d->copyData(other.d);
            }

            DesktopFileReader& DesktopFileReader::operator=(const DesktopFileReader& other) {
                if (this != &other) {
                    // set up a new instance of PrivateData, and copy data over from other object
                    d.reset(new PrivateData);
                    d->copyData(other.d);
                }

                return *this;
            }

            DesktopFileReader& DesktopFileReader::operator=(DesktopFileReader&& other) noexcept {
                if (this != &other) {
                    // move other object's data into this one, and remove reference there
                    d = other.d;
                    other.d = nullptr;
                }

                return *this;
            }

            bool DesktopFileReader::isEmpty() const {
                return d->isEmpty();
            }

            bool DesktopFileReader::operator==(const DesktopFileReader& other) const {
                return d->path == other.d->path;
            }

            bool DesktopFileReader::operator!=(const DesktopFileReader& other) const {
                return !operator==(other);
            }

            boost::filesystem::path DesktopFileReader::path() const {
                return d->path;
            }

            DesktopFile::sections_t DesktopFileReader::data() const {
                return d->sections;
            }

            DesktopFile::section_t DesktopFileReader::operator[](const std::string& name) const {
                auto it = d->sections.find(name);

                // the map would lazy-initialize a new entry in case the section doesn't exist
                // therefore explicitly checking whether the section exists, throwing an exception in case it does not
                if (it == d->sections.end())
                    throw UnknownSectionError(name);

                return it->second;
            }
        }
    }
}
