


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

# Build SXTP_RX executable
add_executable(SXTP_RX "SXTP_RX_main.cpp" "SXTP_common.h")
if (MSVC)
  target_link_options(SXTP_RX PRIVATE /PROFILE)
endif()

target_include_directories(SXTP_RX
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
)

target_link_libraries(SXTP_RX PRIVATE 
    Boost::headers Boost::filesystem cli_lib Threads::Threads)

# Build SXTP_TX executable
add_executable(SXTP_TX "SXTP_TX_main.cpp" "SXTP_common.h")
target_include_directories(SXTP_TX
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
        ${PROJECT_SOURCE_DIR}/include/PacketHeader
)

target_link_libraries(SXTP_TX PRIVATE 
    Boost::headers Boost::filesystem cli_lib Threads::Threads)
