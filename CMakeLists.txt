# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.0)

# Set the project name
project(LowLatencyApp)

# Set C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)

# Set the C++ compiler to g++
set(CMAKE_CXX_COMPILER g++)

# Set compiler flags, including C++20 standard and various warnings
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")

# Enable verbose makefile output
set(CMAKE_VERBOSE_MAKEFILE on)

# Add subdirectories for common, exchange, and trading components
add_subdirectory(common)
add_subdirectory(exchange)
add_subdirectory(trading)

# Create a list of libraries to link
list(APPEND LIBS libexchange)
list(APPEND LIBS libtrading)
list(APPEND LIBS libcommon)
list(APPEND LIBS pthread)

# Include directories for the project and its components
include_directories(${PROJECT_SOURCE_DIR})
include_directories(${PROJECT_SOURCE_DIR}/exchange)
include_directories(${PROJECT_SOURCE_DIR}/trading)

# Create an executable for exchange_main with its source file
add_executable(exchange_main exchange/exchange_main.cpp)

# Link the exchange_main executable with the specified libraries
target_link_libraries(exchange_main PUBLIC ${LIBS})

# Create an executable for trading_main with its source file
add_executable(trading_main trading/trading_main.cpp)

# Link the trading_main executable with the specified libraries
target_link_libraries(trading_main PUBLIC ${LIBS})
