add_executable(Primitives
	main.cpp

	Sampling.cpp
)

catch_discover_tests(Primitives)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})

###############################################################################
#Linker settings
target_link_libraries(Primitives
	scopehal
	scopeprotocols
	yaml-cpp
	Catch2::Catch2
	)
