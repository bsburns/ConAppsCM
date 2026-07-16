
# Find the Boost package (headers are implicitly required)
if (WIN32)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_DEBUG ON)
    set(Boost_DIR "C:\\local\\boost_1_91_0\\bin\\x64\\lib\\cmake\\Boost-1.91.0")
    #set(BOOST_INCLUDEDIR "C:\\local\\boost_1_91_0")
    #set(BOOST_LIBRARYDIR "C:\\local\\boost_1_91_0\\bin\\x64\lib")
    add_compile_definitions(BOOST_ALL_NO_LIB)
endif()
find_package(Boost REQUIRED COMPONENTS filesystem)

# Include Boost headers
#include_directories("C:\\local\\boost_1_91_0\\boost")

add_executable(bUDPS bUDP-Server.cpp bStripes.cpp "FecEngine.h" "common.h" "FecEngine.cpp")

target_include_directories(bUDPS
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

# Link against the Modern CMake header target
target_link_libraries(bUDPS PRIVATE Boost::headers Boost::filesystem cli_lib Threads::Threads)


add_executable(bUDPC bUDP-Client.cpp "FecEngine.h" "common.h" "FecEngine.cpp")

target_include_directories(bUDPC
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
        ${PROJECT_SOURCE_DIR}/external_libraries
)
# Link against the Modern CMake header target
target_link_libraries(bUDPC PRIVATE Boost::headers cli_lib)

add_executable(testC test-Client.cpp "FecEngine.h" "common.h" "FecEngine.cpp")

target_include_directories(testC
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
        ${PROJECT_SOURCE_DIR}/external_libraries
)
# Link against the Modern CMake header target
target_link_libraries(testC PRIVATE Boost::headers cli_lib)

add_executable(testS test-Server.cpp "FecEngine.h" "common.h" "FecEngine.cpp")

target_include_directories(testS
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
        ${PROJECT_SOURCE_DIR}/external_libraries
)
# Link against the Modern CMake header target
target_link_libraries(testS PRIVATE Boost::headers cli_lib)
