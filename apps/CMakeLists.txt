
add_executable(MySim ConsoleSimulator.cpp )
target_link_libraries(MySim PRIVATE Threads::Threads simulator_lib cli_lib )