
# Find the Boost package (headers are implicitly required)
if (WIN32)
    set(Boost_DIR "C:\\local\\boost_1_91_0\\bin\\x64\\lib\\cmake\\Boost-1.91.0")
endif()
find_package(Boost REQUIRED)
# Include Boost headers
#include_directories(${Boost_INCLUDE_DIRS})

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
