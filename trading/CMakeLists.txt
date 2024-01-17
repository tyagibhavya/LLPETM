# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)

# Set the C++ compiler to g++
set(CMAKE_CXX_COMPILER g++)

# Set compilation flags, including C++2a standard, enable additional warnings, and treat warnings as errors
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")

# Enable verbose makefile for detailed build information
set(CMAKE_VERBOSE_MAKEFILE on)

# Use file() to create a list of source files by matching all .cpp files in subdirectories
file(GLOB SOURCES "*/*.cpp")

# Include directories necessary for compilation
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/trading)

# Create a static library named libtrading using the listed source files
add_library(libtrading STATIC ${SOURCES} strategy/risk_manager.cpp)
