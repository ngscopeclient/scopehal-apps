add_executable(Primitives
	main.cpp

	Convert8BitSamples.cpp
	Convert16BitSamples.cpp
	Sampling.cpp
)

target_link_libraries(Primitives
	scopehal
	scopeprotocols
	Catch2::Catch2
	)

#Needed because Windows does not support RPATH and will otherwise not be able to find DLLs when catch_discover_tests runs the executable
if(WIN32)
add_custom_command(TARGET Primitives POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Primitives> $<TARGET_FILE_DIR:Primitives>
	COMMAND_EXPAND_LISTS
	)
endif()

catch_discover_tests(Primitives)
