#pragma once

// system includes
#include <memory>

// library includes
#include <boost/filesystem.hpp>

class DesktopFileReader {
private:
    // opaque data class pattern
    class PrivateData;
    std::shared_ptr<PrivateData> d;

public:
    // default constructor
    DesktopFileReader();

    // construct from path
    explicit DesktopFileReader(boost::filesystem::path path);

    // copy constructor
    DesktopFileReader(const DesktopFileReader& other);

    // copy assignment constructor
    DesktopFileReader& operator=(const DesktopFileReader& other);

    // move assignment operator
    DesktopFileReader& operator=(DesktopFileReader&& other) noexcept;

    // equality operator
    bool operator==(const DesktopFileReader& other) const;

    // inequality operator
    bool operator!=(const DesktopFileReader& other) const;

public:
    // checks whether a file has been read already
    bool isEmpty();

    // returns desktop file path
    boost::filesystem::path path();
};
