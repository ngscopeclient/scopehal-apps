add_executable(Primitives
	main.cpp

	BlackmanHarrisWindow.cpp
	Convert8BitSamples.cpp
	Convert16BitSamples.cpp
	Sampling.cpp
)

catch_discover_tests(Primitives)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
target_link_directories(Primitives PUBLIC ${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#Linker settings
target_link_libraries(Primitives
	scopehal
	scopeprotocols
	${YAML_LIBRARIES}
	Catch2::Catch2
	)
