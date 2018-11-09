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
                    PrivateData* d;

                public:
                    // default constructor
                    DesktopFile();

                    // construct from existing desktop file
                    // file must exist
                    explicit DesktopFile(const boost::filesystem::path& path);

                    // read desktop file
                    // sets path associated with this file
                    bool read(const boost::filesystem::path& path);

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
                    bool save(const boost::filesystem::path& path) const;

                    // check if entry exists in given section and key
                    bool entryExists(const std::string& section, const std::string& key) const;

                    // get key from desktop file
                    // an std::string passed as value parameter will be populated with the contents
                    // returns true (and populates value) if the key exists, false otherwise
                    bool getEntry(const std::string& section, const std::string& key, std::string& value) const;

                    // add key to section in desktop file
                    // the section will be created if it doesn't exist already
                    // returns true if an existing key was overwritten, false otherwise
                    bool setEntry(const std::string& section, const std::string& key, const std::string& value);

                    // create common application entries in desktop file
                    // returns false if one of the keys exists and was left unmodified
                    bool addDefaultKeys(const std::string& executableFileName);

                    // validate desktop file
                    bool validate() const;
            };
        }
    }
}
