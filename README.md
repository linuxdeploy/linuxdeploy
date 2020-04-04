# linuxdeploy

AppDir creation and maintenance tool.


## About

AppImages are a well known and quite popular format for distributing applications from developers to end users.

[appimagetool](https://github.com/AppImage/AppImageKit), the tool creating AppImages, expects directories in a specific format that will then be converted into the final AppImage. This format is called [AppDir](https://github.com/TheAssassin/linuxdeploy/wiki/AppDir-specification). It is not very difficult to understand, but creating AppDirs for arbitrary applications tends to be a very repetitive task. Also, bundling all the dependencies properly can be a quite difficult task. It seems like there is a need for tools which simplify these tasks.

linuxdeploy is designed to be an AppDir maintenance tool. It provides extensive functionalities to create and bundle AppDirs for applications. It features a plugin system that allows for easy bundling of frameworks and creating output bundles such as AppImages with little effort.

linuxdeploy was greatly influenced by [linuxdeployqt](https://github.com/probonopd/linuxdeployqt), and while it employs stricter rules on AppDirs, it's more flexible in use. If you use linuxdeployqt at the moment, consider switching to linuxdeploy today!


## User guides and examples

Please see the [linuxdeploy user guide](https://docs.appimage.org/packaging-guide/from-source/linuxdeploy-user-guide.html) and the [native binaries packaging guide](https://docs.appimage.org/packaging-guide/from-source/native-binaries.html) in the [AppImage documentation](https://docs.appimage.org). There's also an [examples section](https://docs.appimage.org/packaging-guide/from-source/native-binaries.html#examples).


## Projects using linuxdeploy

This is an incomplete list of projects using linuxdeploy. You might want to read their build scripts to see how they use linuxdeploy.

- [Pext](https://github.com/Pext/Pext)
- [AppImageLauncher](https://github.com/TheAssassin/AppImageLauncher)
- [OpenRCT2](https://github.com/OpenRCT2/OpenRCT2)
- [AppImageUpdate](https://github.com/AppImage/AppImageUpdate)
- [appimaged](https://github.com/AppImage/appimaged)
- [MediaElch](https://github.com/Komet/MediaElch/)


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

**A list of official and community plugins can be found in the [awesome-linuxdeploy](http://github.com/linuxdeploy/awesome-linuxdeploy) project.**

**Note:** If you want to suggest a plugin for a specific framework, language etc., please feel free to [create a new issue](https://github.com/linuxdeploy/linuxdeploy/issues/new). Current plugin requests can be found [here](https://github.com/linuxdeploy/linuxdeploy/issues?utf8=%E2%9C%93&q=label%3A%22plugin+request%22).


## Troubleshooting

> I bundled additional resources, but when I try to run them, either the system binary is called or the file is not found.

linuxdeploy does not change any environment variables such as `$PATH`. Your application **must** search for additional resources such as icon files or executables relative to the main binary.


## Contact

The easiest way to get in touch with the developers is to join the IRC chatroom [#AppImage](https://webchat.freenode.net/?channels=appimage) on FreeNode. This is the preferred way for general feedback or questions how to use this application.

To report problems, please [create an issue](https://github.com/TheAssassin/linuxdeploy/issues/new) on GitHub.

Contributions welcome! Please feel free to fork this repository and send us a pull request. Even small changes, e.g., in this README, are highly appreciated!
