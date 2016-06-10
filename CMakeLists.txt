cmake_minimum_required(VERSION 2.6)
project(gb_emulator)

# Add compiler flags here (e.g. -O3, -Wall, etc.)
# This is also needed to use C++11. "-std=c++11"
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -std=c++11 -Wall -Wextra -g")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -std=c++11 -O3")

# Set the location of the final executable to be where cmake/make are called
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Locate SDL2
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${gb_emulator_SOURCE_DIR}")
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})

# Tell the compiler where to find the headers
include_directories(include)
include_directories(include/Sound)

# All the code is in the src subdirectory
file(GLOB_RECURSE SOURCES "src/*.cpp")

#Ignore the file "GL_Render.cpp"
list(REMOVE_ITEM SOURCES "${PROJECT_SOURCE_DIR}/src/GL_Render.cpp")

# The first argument will be the name of the executable (e.g. "[NAME].exe")
add_executable(gb_emulator ${SOURCES})

# Link with SDL2
target_link_libraries(gb_emulator ${SDL2_LIBRARY})

# Copy the SDL2 dll from 'libs' into the executable directory 
add_custom_command(TARGET gb_emulator POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy_if_different
	"${PROJECT_SOURCE_DIR}/libs/SDL2.dll"
	$<TARGET_FILE_DIR:gb_emulator>)
