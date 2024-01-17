# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)

# Set the C++ compiler to g++
set(CMAKE_CXX_COMPILER g++)

# Set compiler flags, including C++20 standard, Wall, Wextra, Werror, and Wpedantic
set(CMAKE_CXX_FLAGS "-std=c++2a -Wall -Wextra -Werror -Wpedantic")

# Enable verbose makefile for detailed build output
set(CMAKE_VERBOSE_MAKEFILE on)

# Globbing to collect all .cpp files in the current directory
file(GLOB SOURCES "*.cpp")

# Include project source directory for header files
include_directories(${PROJECT_SOURCE_DIR})

# Create a static library named libcommon using the collected source files
add_library(libcommon STATIC ${SOURCES})

# Create a list of libraries, starting with libcommon and pthread
list(APPEND LIBS libcommon)
list(APPEND LIBS pthread)

# Create executables with corresponding source files
add_executable(thread_example thread_example.cpp)
add_executable(mem_pool_example mem_pool_example.cpp)
add_executable(lf_queue_example lf_queue_example.cpp)
add_executable(logging_example logging_example.cpp)
add_executable(socket_example socket_example.cpp)

# Link the executables with the created library and additional libraries
target_link_libraries(thread_example PUBLIC ${LIBS})
target_link_libraries(mem_pool_example PUBLIC ${LIBS})
target_link_libraries(lf_queue_example PUBLIC ${LIBS})
target_link_libraries(logging_example PUBLIC ${LIBS})
target_link_libraries(socket_example PUBLIC ${LIBS})
