# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)

# Set the C++ compiler to g++
set(CMAKE_CXX_COMPILER g++)

# Set compiler flags, including C++2a standard, Wall, Wextra, Werror, and Wpedantic
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")

# Enable verbose output for the makefile
set(CMAKE_VERBOSE_MAKEFILE on)

# Globbing to collect all .cpp files recursively in subdirectories
file(GLOB SOURCES "*/*.cpp")

# Include the project source directory and the 'exchange' subdirectory
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/exchange)

# Create a static library named 'libexchange' using the collected source files
add_library(libexchange STATIC ${SOURCES})
