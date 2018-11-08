// local headers
#include "../../src/core/desktopfileentry.h"
#include "desktopfileentry.h"


class DesktopFileEntry::PrivateData {
public:
    std::string key;
    std::string value;

public:
    void copyData(const std::shared_ptr<PrivateData>& other) {
        key = other->key;
        value = other->value;
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
