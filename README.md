# loudgain
This repo is a fork of the loudgain command line utility, originally by [ghedo](https://github.com/ghedo/loudgain) and continued by [Moonbase59](https://github.com/Moonbase59/loudgain). The primary purpose of this fork was to port the program to run natively in Windows (Moonbase59's version could only run under WSL). However, since the previous repo seems to have been abandoned, I am hoping that loudgain users will submit PRs for bugfixes and new features on this repo going forward.

The following changes have been made from Moonbase59's latest version, 0.6.8:
- The build system was completely rewritten to support Windows. For Linux, the build script now supports building .deb packages directly. See [Building](#building) for more info.
- The I/O code was significantly refactored to be cross-platform, with function definitions that are abstracted, and separate Unix/Windows code underneath
- The progress bar was refactored to be more performant

The underlying program logic was mostly untouched, other than a few minor tweaks I made to get it to compile in MSVC, which doesn't have full C99 support.

Due to the previous maintainer's decision to commit static binary files, the repo ballooned to neary 1 GB in size. Consequently, I decided to delete the git history and start over from new, and rebaseline the versioning at 1.0. For this repo, binary packages for Windows 64 bit and Debian/Ubuntu amd64 will be provided on the [release page](https://github.com/complexlogic/loudgain/releases). 

For the best results, loudgain on Windows should be run on the latest version of Windows 10, or Windows 11. The Linux .deb package is compatible with Debian Bullseye and later, Ubuntu 20.04 and later.

I have also included an original python script I wrote called ```scan.py``` that I use to scan my music library. It is a much simpler alternative to the more complex ```rgbpm``` and ```rgbpm2``` scripts that were written by Moonbase59, and is cross-platform Unix/Windows.

## Usage
Usage is exactly the same as previous versions. No features have been added or removed at this point in time. See the [previous repo](https://github.com/Moonbase59/loudgain#getting-started) for usage instructions.

## Scanning Your Music Library With scan.py
The repo contains a simple, cross-platform python script ```scan.py```, which will recursively scan your entire music library using the recommended settings for each file type. To use the script, the following requirements must be met:
- The loudgain executable is in your PATH
- Your library is organized with each album in its own folder
- In each album folder, all audio files are of the same type. It is acceptable to have non-audio files in an album folder e.g. log files or cover art, but if multiple audio file types are detected, the folder will not be scanned.

To use the script, run it in the python interpreter and pass the root of the directory you want to scan as the first argument, e.g.:

```
python scan.py /path/to/music/library
```
```
python scan.py "C:\Music\ripped CDs"
```

The previous scripts ```rgbpm``` and ```rgbpm2``` that were written by Moonbase59 are also included in the scripts directory.

## Building
The build system was rewritten to support both Unix and Windows.

### Unix
The basic build instructions are the same as for the [previous repo](https://github.com/Moonbase59/loudgain#building). 

For Linux, the build system now directly supports building .deb packages via CPack. During the cmake generation step, pass ```-DPACKAGE=DEB``` and ```-DCMAKE_INSTALL_PREFIX=/usr``` to cmake. Then, build the package with:
```
make package
```
By default, this will build a package for the amd64 architecture. To explicitly specify an architecture, pass ```-DCPACK_DEBIAN_PACKAGE_ARCHITECTURE```.

### Windows 
The Windows toolchain consists of Visual Studio and vcpkg in addition to Git  and CMake. Before starting, make sure that Visual Studio is installed with C++ core desktop features and C++ CMake tools.

Clone the master repo and create a build directory:
```
git clone https://github.com/complexlogic/loudgain.git
cd loudgain
mkdir build && cd build
```
Build the dependencies with vcpkg:
```
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
.\vcpkg\vcpkg install taglib libebur128 getopt ffmpeg[avcodec,opus,avformat,swresample] --triplet=x64-windows
```

Generate the Visual Studio project files:
```
cmake .. -DCMAKE_TOOLCHAIN_FILE=".\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET="x64-windows"
```
Build and test the program:
```
cmake --build .
.\Debug\loudgain.exe
```
Optionally, generate a zipped install package:
```
cmake --build . --config Release --target package
```
