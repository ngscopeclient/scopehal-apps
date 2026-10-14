add_executable(HIL
	main.cpp
	BasicMetrics.cpp
	WaveformCapture.cpp
)

target_link_libraries(HIL
	scopehal
	scopeprotocols
	Catch2::Catch2
	)

#Needed because Windows does not support RPATH and will otherwise not be able to find DLLs when catch_discover_tests runs the executable
if(WIN32)
add_custom_command(TARGET HIL POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:HIL> $<TARGET_FILE_DIR:HIL>
	COMMAND_EXPAND_LISTS
	)
endif()

catch_discover_tests(
	HIL
	EXTRA_ARGS --siggen ${HIL_SIGGEN_PATH} --scope ${HIL_TS_PATH}
	PROPERTIES RUN_SERIAL
	)
