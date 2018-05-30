// library headers
#include <INI.h>

// local headers
#include "linuxdeploy/core/desktopfile.h"
#include "linuxdeploy/core/log.h"

using namespace linuxdeploy::core;
using namespace linuxdeploy::core::log;

namespace bf = boost::filesystem;

namespace linuxdeploy {
    namespace core {
        namespace desktopfile {
            class DesktopFile::PrivateData {
                public:
                    bf::path path;
                    INI<> ini;

                public:
                    PrivateData() : path(), ini("", false) {};
            };

            DesktopFile::DesktopFile() {
                d = new PrivateData();
            }

            DesktopFile::DesktopFile(const bf::path& path) : DesktopFile() {
                if (!read(path))
                    throw std::runtime_error("Failed to read desktop file");
            };

            bool DesktopFile::read(const boost::filesystem::path& path) {
                setPath(path);

                clear();

                // nothing to do
                if (!bf::exists(path))
                    return true;

                std::ifstream ifs(path.string());
                if (!ifs)
                    return false;

                d->ini.parse(ifs);
                return true;
            }

            boost::filesystem::path DesktopFile::path() const {
                return d->path;
            }

            void DesktopFile::setPath(const boost::filesystem::path& path) {
                d->path = path;
            }

            void DesktopFile::clear() {
                d->ini.clear();
            }

            bool DesktopFile::save() const {
                return save(d->path);
            }

            bool DesktopFile::save(const boost::filesystem::path& path) const {
                return d->ini.save(path.string());
            }

            bool DesktopFile::entryExists(const std::string& section, const std::string& key) const {
                if (!d->ini.select(section))
                    return false;

                std::string absolutelyUnlikeValue = "<>!§$%&/()=?+'#-.,_:;'*¹²³½¬{[]}^°|";

                auto value =  d->ini.get<std::string, std::string, std::string>(section, key, absolutelyUnlikeValue);

                return value != absolutelyUnlikeValue;
            }

            bool DesktopFile::setEntry(const std::string& section, const std::string& key, const std::string& value) {
                // check if value exists -- used for return value
                auto rv = entryExists(section, key);

                // set key
                d->ini[section][key] = value;

                return rv;
            }

            bool DesktopFile::getEntry(const std::string& section, const std::string& key, std::string& value) const {
                if (!entryExists(section, key))
                    return false;

                if (!d->ini.select(section))
                    return false;

                value = d->ini.get(key);
                return true;
            }

            bool DesktopFile::addDefaultKeys(const std::string& executableFileName) {
                auto rv = true;

                auto setDefault = [&rv, this](const std::string& section, const std::string& key, const std::string& value) {
                    if (setEntry(section, key, value)) {
                        rv = false;
                    }
                };

                setDefault("Desktop Entry", "Name", executableFileName);
                setDefault("Desktop Entry", "Exec", executableFileName);
                setDefault("Desktop Entry", "Icon", executableFileName);
                setDefault("Desktop Entry", "Type", "Application");
                setDefault("Desktop Entry", "Categories", "Utility;");

                return rv;
            }

            bool DesktopFile::validate() const {
                // FIXME: call desktop-file-validate
                return true;
            }
        }
    }
}



