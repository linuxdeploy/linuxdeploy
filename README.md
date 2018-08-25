# linuxdeploy

AppDir creation and maintenance tool.


## About

AppImages are a well known and quite popular format for distributing applications from developers to end users.

[appimagetool](https://github.com/AppImage/AppImageKit), the tool creating AppImages, expects directories in a specific format that will then be converted into the final AppImage. This format is called [AppDir](https://github.com/TheAssassin/linuxdeploy/wiki/AppDir-specification). It is not very difficult to understand, but creating AppDirs for arbitrary applications tends to be a very repetitive task. Also, bundling all the dependencies properly can be a quite difficult task. It seems like there is a need for tools which simplify these tasks.

linuxdeploy is designed to be an AppDir maintenance tool. It provides extensive functionalities to create and bundle AppDirs for applications. It features a plugin system that allows for easy bundling of frameworks and creating output bundles such as AppImages with little effort.

linuxdeploy was greatly influenced by [linuxdeployqt](https://github.com/probonopd/linuxdeployqt), and while it employs stricter rules on AppDirs, it's more flexible in use. If you use linuxdeployqt at the moment, consider switching to linuxdeploy today!


## Usage

linuxdeploy, being a tool for creating AppDirs and eventually AppImages, is released as an AppImage itself. Please download and use the AppImage for normal use. If you encounter errors, e.g., when building the tool from source, please try with the AppImage first before [creating an issue](https://github.com/TheAssassin/linuxdeploy/issues/new).

There are two main ways to use linuxdeploy: Provide the paths to all files you want to bundle manually, or create an FHS-like directory and run linuxdeploy on it.

Most build systems provide some kind of `make install` functionality that is capable of producing FHS-like directory trees that can be turned into AppDirs:

```sh
# automake
./configure --prefix=/usr
make install DESTDIR=AppDir

# cmake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
make install DESTDIR=AppDir

# qmake
qmake CONFIG+=release PREFIX=/usr .
make install INSTALL_ROOT=AppDir
```

Now, simply run linuxdeploy like `./linuxdeploy*.AppImage --appdir AppDir`.

Please use `./linuxdeploy*.AppImage --help` to get a list of options supported by linuxdeploy.

If your build system cannot produce such install trees or you prefer to bundle everything manually, you can also specify the paths to the resources yourself. linuxdeploy provides the following options for this use case:

```
-l[library...],
--lib=[library...],
--library=[library...]            Shared library to deploy
-e[executable...],
--executable=[executable...]      Executable to deploy
-d[desktop file...],
--desktop-file=[desktop file...]  Desktop file to deploy
                                  enough for some tests
-i[icon file...],
--icon-file=[icon file...]        Icon to deploy
```

An example run could look like this:

```bash
./linuxdeploy*.AppImage --appdir AppDir --init-appdir -e myapp -d myapp.desktop -i myapp_64x64.png
```

Of course both approaches can be combined, e.g., you can bundle additional executables with your main app.

linuxdeploy doesn't mind being run on an AppDir more than once, as it recognize previous runs, and should not break files within the AppDir. This is called an "iterative workflow". In case of errors, you can simply fix them, and re-run linuxdeploy afterwards.
If you ever encounter issues when running linuxdeploy on an existing AppDir, please let us know by [creating an issue](https://github.com/TheAssassin/linuxdeploy/issues/new).


## Plugins

linuxdeploy features a plugin system. Plugins are separate executables which implement a CLI-based plugin interface ([specification](https://github.com/TheAssassin/linuxdeploy/wiki/Plugin-system)).

There are two types of plugins: bundling and output plugins. Bundling plugins can be used to add resources to the AppDir. Output plugins turn the AppDir in actual bundles, e.g., AppImages.

linuxdeploy looks for plugins in the following places:

  - the directory containing the linuxdeploy binary
  - when using the AppImage: the directory containing the AppImage
  - the directories in the user's `$PATH`

You can use `./linuxdeploy*.AppImage --list-plugins` to get a list of all the plugins linuxdeploy has detected on your system.

linuxdeploy currently ships with some plugins. These are likely out of date. In case of issues, please download the latest version, which will take precendence over the bundled plugin.

If you want to use a plugin to bundle additional resources, please add `./linuxdeploy*.AppImage --plugin <name>` to your linuxdeploy command. Output plugins can be activated using `./linuxdeploy*.AppImage --output <name>`.


## User guides and examples

Please see the [linuxdeploy user guide](https://docs.appimage.org/packaging-guide/linuxdeploy-user-guide.html) and the [native binaries packaging guide](https://docs.appimage.org/packaging-guide/native-binaries.html) in the [AppImage documentation](https://docs.appimage.org). There's also an [examples section](https://docs.appimage.org/packaging-guide/native-binaries.html#Examples).


## Troubleshooting

> I bundled additional resources, but when I try to run them, either the system binary is called or the file is not found.

linuxdeploy does not change any environment variables such as `$PATH`. Your application **must** search for additional resources such as icon files or executables relative to the main binary.


## Contact

The easiest way to get in touch with the developers is to join the IRC chatroom [#AppImage](https://webchat.freenode.net/?channels=appimage) on FreeNode. This is the preferred way for general feedback or questions how to use this application.

To report problems, please [create an issue](https://github.com/TheAssassin/linuxdeploy/issues/new) on GitHub.

Contributions welcome! Please feel free to fork this repository and send us a pull request. Even small changes, e.g., in this README, are highly appreciated!
