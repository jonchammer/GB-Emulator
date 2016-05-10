========================
Compilation instructions
========================
This project uses CMake for compilation, so ensure that it is installed on the target platform first. To actually compile the code, do the following:

cd ./build
cmake -G "MSYS Makefiles" ..
make

The binary will be placed in the current directory (ROOT/build). This location can be changed by modifying the file CMakeLists.txt at the root level. This line:

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

is what will need to be changed. To set compiler flags, change this line in the root CMakeLists.txt:

set(CMAKE_CXX_FLAGS "")

For example,

set(CMAKE_CXX_FLAGS "-Wall -O3")

NOTE: Netbeans is currently set up to use Cmake, so for normal development, you can use the IDE if you want to.

===============
SDL Information
===============
The CMakeLists.txt file contains information to locate the SDL2 installation, but it needs FindSDL2.cmake in order to work properly. Make sure that file is still present.

NOTE: For Windows users, an environment variable "SDL2" has to be set for compilation to be successful. It needs to be set to the library that will be used. For example, 64 bit:

SDL2 = "C:\SDL\SDL2-2.0.4\x86_64-w64-mingw32"

NOTE: For Linux users, make sure to have SDL 2.0.4 installed on your system. Currently,

apt-get install libsdl2.0.0

will install an older version, so you might have to install it from source. See here:

https://wiki.libsdl.org/Installation

