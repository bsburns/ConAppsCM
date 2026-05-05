add_library(my_lib lib.cpp)

target_include_directories(my_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/include/Simulator
        ${PROJECT_SOURCE_DIR}/external_libraries
)

target_compile_features(my_lib PUBLIC cxx_std_20)

add_subdirectory(cli)
add_subdirectory(Simulator)