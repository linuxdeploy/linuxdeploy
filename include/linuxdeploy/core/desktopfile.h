// system includes
#include <unordered_map>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "desktopfileentry.h"

#pragma once

namespace linuxdeploy {
    namespace core {
        namespace desktopfile {
            /*
             * Parse and read desktop files.
             */
            class DesktopFile {
            public:
                // describes a single section
                typedef std::unordered_map<std::string, DesktopFileEntry> section_t;

                // describes all sections in the desktop file
                typedef std::unordered_map<std::string, section_t> sections_t;

            private:
                    // private data class pattern
                    class PrivateData;
                    std::shared_ptr<PrivateData> d;

                public:
                    // default constructor
                    DesktopFile();

                    // construct from existing desktop file
                    // file must exist, otherwise std::runtime_error is thrown
                    explicit DesktopFile(const boost::filesystem::path& path);

                    // construct by reading an existing stream
                    // file must exist, otherwise std::runtime_error is thrown
                    explicit DesktopFile(std::istream& is);

                    // copy constructor
                    DesktopFile(const DesktopFile& other);

                    // copy assignment constructor
                    DesktopFile& operator=(const DesktopFile& other);

                    // move assignment operator
                    DesktopFile& operator=(DesktopFile&& other) noexcept;

                    // equality operator
                    bool operator==(const DesktopFile& other);

                    // inequality operator
                    bool operator!=(const DesktopFile& other);

            public:
                    // returns true if a file has been loaded, false otherwise
                    bool isEmpty() const;

                    // read desktop file
                    // sets path associated with this file
                    // throws exceptions in case of issues, see DesktopFileReader for more information
                    void read(const boost::filesystem::path& path);

                    // read desktop file from existing stream
                    // throws exceptions in case of issues, see DesktopFileReader for more information
                    void read(std::istream& is);

                    // get path associated with this file
                    boost::filesystem::path path() const;

                    // sets the path associated with this desktop file
                    // used to e.g., save the desktop file
                    void setPath(const boost::filesystem::path& path);

                    // clear contents of desktop file
                    void clear();

                    // save desktop file
                    bool save() const;

                    // save desktop file to path
                    // does not change path associated with desktop file
                    // throws exceptions in case of errors, see DesktopFileWriter::save(...) for more information
                    bool save(const boost::filesystem::path& path) const;

                    // save desktop file to ostream
                    // does not change path associated with desktop file
                    // throws exceptions in case of errors, see DesktopFileWriter::save(...) for more information
                    bool save(std::ostream& os) const;

                    // check if entry exists in given section and key
                    bool entryExists(const std::string& section, const std::string& key) const;

                    // get key from desktop file
                    // an std::string passed as value parameter will be populated with the contents
                    // returns true (and populates value) if the key exists, false otherwise
                    bool getEntry(const std::string& section, const std::string& key, std::string& value) const;

                    // get key from desktop file
                    // an std::string passed as value parameter will be populated with the contents
                    // returns true (and populates value) if the key exists, false otherwise
                    bool getEntry(const std::string& section, const std::string& key, DesktopFileEntry& value) const;

                    // add key to section in desktop file
                    // the section will be created if it doesn't exist already
                    // returns true if an existing key was overwritten, false otherwise
                    bool setEntry(const std::string& section, const std::string& key, const std::string& value);

                    // add key to section in desktop file
                    // the section will be created if it doesn't exist already
                    // returns true if an existing key was overwritten, false otherwise
                    bool setEntry(const std::string& section, const DesktopFileEntry& entry);

                    // add key to section in desktop file
                    // the section will be created if it doesn't exist already
                    // returns true if an existing key was overwritten, false otherwise
                    bool setEntry(const std::string& section, const DesktopFileEntry&& entry);

                    // create common application entries in desktop file
                    // returns false if one of the keys exists and was left unmodified
                    bool addDefaultKeys(const std::string& executableFileName);

                    // validate desktop file
                    bool validate() const;
            };
        }
    }
}
