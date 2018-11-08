#pragma once

// system headers
#include <memory>
#include <string>

// library headers
#include <boost/lexical_cast.hpp>

class DesktopFileEntry {
private:
    // opaque data class pattern
    class PrivateData;
    std::shared_ptr<PrivateData> d;

public:
    // default constructor
    DesktopFileEntry();

    // construct from key and value
    explicit DesktopFileEntry(std::string key, std::string value);

    // copy constructor
    DesktopFileEntry(const DesktopFileEntry& other);

    // copy assignment constructor
    DesktopFileEntry& operator=(const DesktopFileEntry& other);

    // move assignment operator
    DesktopFileEntry& operator=(DesktopFileEntry&& other) noexcept;

    // equality operator
    bool operator==(const DesktopFileEntry& other) const;

    // inequality operator
    bool operator!=(const DesktopFileEntry& other) const;

public:
    // checks whether a key and value have been set
    bool isEmpty() const;

    // return entry's key
    const std::string& key() const;

    // return entry's value
    const std::string& value() const;

public:
    // allow conversion of entry to string
    // returns value
    explicit operator std::string() const;

    // convert value to integer
    // throws boost::bad_lexical_cast in case of type errors
    int asInt() const;

    // convert value to long
    // throws boost::bad_lexical_cast in case of type errors
    long asLong() const;

    // split CSV list value into vector
    // the separator used to split the string is a semicolon as per desktop file spec
    std::vector<std::string> parseStringList() const;
};
