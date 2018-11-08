// system includes
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <utility>

// local headers
#include "linuxdeploy/util/util.h"
#include "desktopfileentry.h"
#include "desktopfilereader.h"

namespace bf = boost::filesystem;

class DesktopFileReader::PrivateData {
public:
    bf::path path;

public:
    bool isEmpty()  {
        return path.empty();
    }

    void assertPathIsNotEmptyAndFileExists() {
        if (path.empty())
            throw std::invalid_argument("empty path is not permitted");
    }

    void copyData(const std::shared_ptr<PrivateData>& other) {
        path = other->path;
    }

    void parse(std::istream& file) {}
};

DesktopFileReader::DesktopFileReader() : d(new PrivateData) {}

DesktopFileReader::DesktopFileReader(boost::filesystem::path path) : DesktopFileReader() {
    d->path = std::move(path);
    d->assertPathIsNotEmptyAndFileExists();

    std::ifstream ifs(d->path.string());
    if (!ifs)
        throw std::invalid_argument("could not open file: " + d->path.string());

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
