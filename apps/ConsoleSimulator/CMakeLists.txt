
add_executable(MySim ConSim_main.cpp )
target_link_libraries(MySim PRIVATE Threads::Threads simulator_lib cli_lib )