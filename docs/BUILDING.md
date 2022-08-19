# BUILDING
rsgain builds natively on Unix and Windows, and features a cross-platform CMake build system. The following external dependencies are required:
- libebur128
- taglib
- FFmpeg, specifically the libraries:
	+ libavformat
	+ libavcodec
	+ libswresample
	+ libavutil
- fmt
- inih

rsgain uses the C++20 standard, and as such requires a relatively modern compiler to build:
- On Windows, use Visual Studio 2022
- On Linux, use GCC 11 or later if possible. rsgain will also build with GCC 10, but the time elapsed statistical output will be disabled (no effect on core functionality). GCC versions 9 and earlier are not supported.
- On macOS, the latest available Xcode for your machine should work.

## Unix
Before starting, make sure you have the devlopment tools Git, CMake and pkg-config installed.

Install the required dependencies:

### APT-based Linux (Debian, Ubuntu, Mint etc.)
```
sudo apt install libebur128-dev libtag1-dev libavformat-dev libavcodec-dev libswresample-dev libavutil-dev libinih-dev
```
### Pacman-based Linux (Arch, Manjaro, etc.)
```
sudo pacman -S libebur128 taglib ffmpeg libinih
```
### DNF-based Linux (Fedora)
FFmpeg is in the official repos in Fedora 36 and later only. If you're on an earlier version you will need to find an alternative source.
```
sudo dnf install libebur128-devel taglib-devel libavformat-free-devel libavcodec-free-devel libswresample-free-devel libavutil-free-devel inih-devel
```
### macOS (Homebrew)
```
brew install libebur128 taglib ffmpeg inih
```

### Building
Clone the master repo and create a build directory:
```
git clone https://github.com/complexlogic/rsgain.git
cd rsgain
mkdir build && cd build
```
Generate the Makefile:
```
cmake ..
```
Build and test the program:
```
make
./rsgain
```
Optionally, install to your system directories:
```
sudo make install
```
By default, this will install rsgain with a prefix of `/usr/local`. If you want a different prefix, re-run the CMake generation step with `-DCMAKE_INSTALL_PREFIX=prefix`.

## Windows
The Windows toolchain consists of Visual Studio and vcpkg in addition to Git and CMake. Before starting, make sure that Visual Studio is installed with C++ core desktop features and C++ CMake tools.

Clone the master repo and create a build directory:
```
git clone https://github.com/complexlogic/rsgain.git
cd rsgain
mkdir build
cd build
```
Build the dependencies with vcpkg:
```
git clone https://github.com/microsoft/vcpkg
.\vcpkg\bootstrap-vcpkg.bat -disableMetrics
.\vcpkg\vcpkg install taglib libebur128 getopt inih ffmpeg[avcodec,avformat,swresample] --triplet=x64-windows
```
Generate the Visual Studio project files:
```
cmake .. -DCMAKE_TOOLCHAIN_FILE=".\vcpkg\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET="x64-windows"
```
Build and test the program:
```
cmake --build .
.\Debug\rsgain.exe
```
Optionally, generate a zipped install package:
```
cmake --build . --config Release --target package
```