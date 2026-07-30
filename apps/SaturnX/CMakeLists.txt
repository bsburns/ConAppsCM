


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

# Build SX_SV executable
add_executable(SX_SV "SX_SV_main.cpp" "SX_common.h")
target_include_directories(SX_SV
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

target_link_libraries(SX_SV PRIVATE 
    Boost::headers Boost::filesystem cli_lib Threads::Threads)

# Build SX_LCT executable
add_executable(SX_LCT "SX_LCT_main.cpp" "SX_common.h")
target_include_directories(SX_LCT
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

target_link_libraries(SX_LCT PRIVATE 
    Boost::headers Boost::filesystem cli_lib Threads::Threads)
