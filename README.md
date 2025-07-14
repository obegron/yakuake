# Yakuake Browser

This is a fork of [Yakuake](https://kde.org/applications/system/org.kde.yakuake) that adds web browser functionality, creating a hybrid drop-down terminal and browser.

## About Yakuake

Yakuake is a drop-down terminal emulator based on KDE Konsole technology. It's a KDE Extragear application released under GPL v2, GPL v3 or any later version accepted by the membership of KDE e.V.

This fork retains all of Yakuake's original features and adds a browser tab.

## Known Issues

*   **Wayland Display Issue:** When using Wayland, the browser window may initially appear in the wrong location.
    *   **Workaround:** Press the `F12` key twice to toggle the window, which should correct its position.

## Project Information

The information below pertains to the original Yakuake project. For issues or contributions related to the browser functionality, please use this repository's issue tracker.

*   **Original Maintainer:** Eike Hein <hein@kde.org>
*   **Yakuake Website:** <https://kde.org/applications/system/org.kde.yakuake>
*   **Yakuake Bug Tracker:** <https://bugs.kde.org/>
*   **Yakuake Source Code:** <https://invent.kde.org/utilities/yakuake>

## Basic build and installation instructions

1.  Download the source code or clone this repository
2.  `cd` to the source code folder
3.  `mkdir build`
4.  `cd build`
5.  `cmake ../` - defaults to `/usr/local` as installation path on UNIX ([docs](https://cmake.org/cmake/help/latest/variable/CMAKE_INSTALL_PREFIX.html)), optionally use `-DCMAKE_INSTALL_PREFIX=<path to install to>`
6.  `make`
7.  `sudo make install`

To remove use `sudo make uninstall`

For more, please see the KDE Techbase wiki (<https://techbase.kde.org/>) and
the CMake documentation (at <https://www.cmake.org/>).
