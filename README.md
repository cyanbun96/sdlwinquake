SDLWinQuake
==============================

CyanBun96: My fork aims to bring it closer to being "Chocolate Quake", with Linux being the primary targeted OS.

Requires SDL2.
Build on Linux with ./autogen.sh && ./configure && make
Other OSs not tested yet.


Changes from upstream:


-WIP SDL2 port

---No SDL1.2 includes

---Integer scaling

---Borderless window with -borderless parameter

---Auto-resolution fullscreen with -fullscreen_desktop

---Higher resolutions (max is 1920x1080 ATM)

---Hardware-accelerated frame > screen rendering

------Boosts performance massively on systems with GPUs

------Tanks performance on machines without GPUs

------Use the new -forceoldrender flag to disable


-High resolution support

---Maximum tested is 16K, 2000 times bigger that 320x200

---Defined by MAXHEIGHT and MAXWIDTH in r_shared.h and d_ifacea.h

---Can probably be set higher for billboard gaming


-Non-square pixels for 320x200 and 640x400 modes

---Can be forced on other modes with -stretchpixels


-General feature parity with the original WinQuake

---"Use Mouse" option in windowed mode (also _windowed_mouse cvar)

---Video configuration menu (mostly for show, use -width and -heigth)


-Proper UI scaling


-Advanced audio configuration

---The default audio rate is 11025 for more muffled WinQuake sound

---New flags -sndsamples and -sndpitch (try -sndpitch 5 or 15)


-vim-like keybinds that work in menus, enable with -vimmode flag


-Mouse sensitivity Y-axis scaling with sensitivityyscale cvar


Planned:

-Fixing whatever was broken by adding the above


Maybe:

-A hidden/optional "Advanced options" menu for all the new stuff

-Brown menu background filter, DOS Quake-style

-Modern mod support

Original readme below.
------------

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
