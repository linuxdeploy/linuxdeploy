#pragma once

// system headers
#include <memory>

// local headers
#include "linuxdeploy/core/appdir.h"

namespace linuxdeploy {
    namespace core {
        /**
         * Wrapper for an AppDir that encapsulates all functionality to set up the AppDir root directory.
         */
        class AppDirRootSetup {
        private:
            // PImpl
            class Private;
            std::shared_ptr<Private> d;

        public:
            explicit AppDirRootSetup(const appdir::AppDir& appdir);

            /**
             * Deploy files to the AppDir root directory using the provided desktop file and the information within it.
             * Optionally, a custom AppRun path can be provided which is deployed instead of following the internal
             * default mechanism, which usually just places a symlink to the main binary as AppRun.
             *
             * @param desktopFile
             * @param customAppRunPath
             * @return
             */
            bool run(const desktopfile::DesktopFile& desktopFile, const boost::filesystem::path& customAppRunPath = "") const;
        };

    }
}
