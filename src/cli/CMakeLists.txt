# Recursively find all .cpp files in src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(cli_lib ${SOURCES})

target_include_directories(cli_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/cli
        ${PROJECT_SOURCE_DIR}/include/utilities
        ${PROJECT_SOURCE_DIR}/include/Simulator
        ${PROJECT_SOURCE_DIR}/external_libraries
)

target_link_libraries(cli_lib Threads::Threads)
