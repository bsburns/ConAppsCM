cmake_minimum_required(VERSION 3.28)
project(ConsoleApplications)

#find_package(nlohmann_json REQUIRED)

# Get the latest abbreviated commit hash
execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Pass the hash to your C++ code as a macro
add_definitions("-DGIT_HASH=\"${GIT_HASH}\"")

# Execute the shell command 'hostname' and save it to a CMake variable
execute_process(COMMAND hostname OUTPUT_VARIABLE BUILD_MACHINE OUTPUT_STRIP_TRAILING_WHITESPACE)
# Pass that variable as a preprocessor definition to your C++ code
add_definitions("-DBUILD_MACHINE=\"${BUILD_MACHINE}\"")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Prefer pthread flag if available
set(THREADS_PREFER_PTHREAD_FLAG ON)

# Find and configure threading support
find_package(Threads REQUIRED)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

add_subdirectory(src)
add_subdirectory(apps)
