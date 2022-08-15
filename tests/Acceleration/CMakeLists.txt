add_executable(Acceleration
	main.cpp

	Buffers.cpp
)

catch_discover_tests(Acceleration)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})

###############################################################################
#Linker settings
target_link_libraries(Acceleration
	scopehal
	scopeprotocols
	yaml-cpp
	Catch2::Catch2
	)
