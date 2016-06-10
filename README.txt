========================
Overview
========================
This is a cross-platform emulator for the original Gameboy and Gameboy Color platforms
written in C++11. 

The emulator itself is designed to work with different environments, but the only one
that is currently implemented is an SDL2 window. This should allow the emulator to
work with Windows / Linux / OSX in theory. It's been tested on Windows 10 and Ubuntu 14.04.

========================
Compilation instructions
========================
This project uses CMake for compilation, so ensure that it is installed on the target platform 
first. To actually compile the code, do the following:

cd ./build
cmake -G "MSYS Makefiles" ..
make

The binary will be placed in the current directory (ROOT/build). This location can be 
changed by modifying the file CMakeLists.txt at the root level. This line:

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

is what will need to be changed. 

To set compiler flags, change this line in the root CMakeLists.txt:

set(CMAKE_CXX_FLAGS_[DEBUG, RELEASE] "")

Note that different flags can be used for debug and release configurations. The debug variant is the
default. In order to compile a release variant, use the following command:

cmake -G "MSYS Makefiles" -DCMAKE_BUILD_TYPE=Release ..

NOTE: Netbeans is currently set up to use Cmake, so for normal development, you can use an 
IDE if you want to.

========================
SDL Information
========================
The CMakeLists.txt file contains information to locate the SDL2 installation, but it needs 
FindSDL2.cmake in order to work properly. Make sure that file is still present.

NOTE: For Windows users, an environment variable "SDL2" has to be set for compilation to be 
successful. It needs to be set to the library that will be used. For example, 64 bit:

SDL2 = "C:\SDL\SDL2-2.0.4\x86_64-w64-mingw32"

NOTE: For Linux users, make sure to have SDL 2.0.4 installed on your system. Currently,

apt-get install libsdl2.0.0

will install an older version, so you might have to install it from source. See here:

https://wiki.libsdl.org/Installation

