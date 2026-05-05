# Recursively find all .cpp files in src/
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

add_library(simulator_lib ${SOURCES})

target_include_directories(simulator_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Simulator
        ${PROJECT_SOURCE_DIR}/external_libraries
)

target_link_libraries(simulator_lib Threads::Threads)
