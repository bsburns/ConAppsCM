	
add_executable(UDPClient UDPClient.cpp )
target_include_directories(UDPClient
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/external_libraries
)

if (WIN32)
	target_link_libraries(UDPClient PRIVATE Threads::Threads cli_lib Ws2_32)
else()
	target_link_libraries(UDPClient PRIVATE Threads::Threads cli_lib )
endif()