add_executable(Acceleration
	main.cpp

	Buffers.cpp
)

catch_discover_tests(Acceleration)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
target_link_directories(Acceleration PUBLIC ${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#Linker settings
target_link_libraries(Acceleration
	scopehal
	scopeprotocols
	${YAML_LIBRARIES}
	Catch2::Catch2
	)
