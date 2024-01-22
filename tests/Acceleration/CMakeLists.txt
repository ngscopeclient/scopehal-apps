add_executable(Acceleration
	main.cpp

	Buffers.cpp
)

target_link_libraries(Acceleration
	scopehal
	scopeprotocols
	Catch2::Catch2
	)

#Needed because Windows does not support RPATH and will otherwise not be able to find DLLs when catch_discover_tests runs the executable
if(WIN32)
add_custom_command(TARGET Acceleration POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Acceleration> $<TARGET_FILE_DIR:Acceleration>
	COMMAND_EXPAND_LISTS
	)
endif()

catch_discover_tests(Acceleration)
