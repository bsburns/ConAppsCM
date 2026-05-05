target_include_directories(my_lib
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

#find_package(nlohmann_json REQUIRED)

add_executable(my_app2 main.cpp)
add_executable(MySim ConsoleSimulator.cpp)
target_link_libraries(my_app2 PRIVATE my_lib Threads::Threads)
target_link_libraries(MySim PRIVATE Threads::Threads simulator_lib cli_lib )