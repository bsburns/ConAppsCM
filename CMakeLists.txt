cmake_minimum_required(VERSION 3.28)
project(ConsoleApplications)

#find_package(nlohmann_json REQUIRED)

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
