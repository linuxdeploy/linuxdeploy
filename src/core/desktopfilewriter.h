#pragma once

// system includes
#include <memory>
#include <ostream>

// library includes
#include <boost/filesystem.hpp>

// local includes
#include "linuxdeploy/core/desktopfile.h"
#include "linuxdeploy/core/desktopfileentry.h"

namespace linuxdeploy {
    namespace core {
        namespace desktopfile {
            class DesktopFileWriter {
            private:
                // opaque data class pattern
                class PrivateData;
                std::shared_ptr<PrivateData> d;

            public:
                // default constructor
                DesktopFileWriter();

                // construct from data
                explicit DesktopFileWriter(DesktopFile::sections_t data);

                // copy constructor
                DesktopFileWriter(const DesktopFileWriter& other);

                // copy assignment constructor
                DesktopFileWriter& operator=(const DesktopFileWriter& other);

                // move assignment operator
                DesktopFileWriter& operator=(DesktopFileWriter&& other) noexcept;

                // equality operator
                bool operator==(const DesktopFileWriter& other) const;

                // inequality operator
                bool operator!=(const DesktopFileWriter& other) const;

            public:
                // returns desktop file path
                DesktopFile::sections_t data() const;

            public:
                // save to given path
                void save(const boost::filesystem::path& path);

                // save to given ostream
                void save(std::ostream& os);
            };
        }
    }
}
