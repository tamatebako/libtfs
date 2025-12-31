cmake_minimum_required(VERSION 3.24)
project(libtfs_example VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find libtfs package
find_package(libtfs CONFIG REQUIRED)

# Create example executable
add_executable(archive_reader main.cpp)
target_link_libraries(archive_reader PRIVATE libtfs::tfs)

# Copy test archives to build directory
file(COPY test_archives DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Installation
install(TARGETS archive_reader
  RUNTIME DESTINATION bin
)