
# Find the Boost package (headers are implicitly required)
find_package(Boost REQUIRED)

add_executable(bUDPS bUDP-Server.cpp)

target_include_directories(bUDPS
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

# Link against the Modern CMake header target
target_link_libraries(bUDPS PRIVATE Boost::headers cli_lib Threads::Threads)


add_executable(bUDPC bUDP-Client.cpp)

target_include_directories(bUDPC
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
        ${PROJECT_SOURCE_DIR}/external_libraries
)
# Link against the Modern CMake header target
target_link_libraries(bUDPC PRIVATE Boost::headers cli_lib)
