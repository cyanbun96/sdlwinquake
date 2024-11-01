SDLWinQuake
==============================

CyanBun96: My fork aims to bring it closer to being "Chocolate Quake", with Linux being the primary targeted OS.

Changes from upstream:
-1080p max resolution

Planned:
-Integer scaling
-Mouse sensitivity for widescreen resolutions

Original readme below.

This is SDLWinQuake, a port of id Software's Quake engine to the Simple Direct-media Layer 1.2.  The Autotools build system has been updated to 2023 standards more or less.  There is also a build.sh file (for macOS) and a Windows VS2022 project file with dependencies included.

Installation
------------

Run `autogen.sh` to generate the `configure` file. Then execute `./configure && make` to build the `sdlwinquake` executable. Run the `sdlwinquake` executable from within a directory containing the original Quake data files.

The Quake data files are expected to be named in lower case! This is true if you installed from Linux Quake installation media, but not necessarily true if you copied the data files over from Windows or performed the installation inside of DOSBox. If `sdlwinquake` fails to start and complains about missing files, be sure the `id1` directory and the `.pak` files within it are renamed to lower case.

See `INSTALL` for detailed build instructions. Support for building on 64-bit multiarch systems has been added and may need explanation.

See `README.SDL` for the original porter's comments.

Notes
-----

This is the only 64bit port of WinQuake on the net (that I have seen).  I intend for this to be a complete replacement of 'WinQuake' but using SDL.  There is an SDL2 port in-progress.

As always, contributors are welcome :)

License
-------

SDLWinQuake is licensed under the GPLv2.  You should have received a copy of the GPLv2 in a file called COPYING in the same directory as this README.  If you did not, contact the distributor from whom you recieved this software for a copy.
