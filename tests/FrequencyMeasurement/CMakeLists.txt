add_executable(FrequencyMeasurement
	main.cpp
)

add_test(
	NAME FrequencyMeasurement
	COMMAND ${CMAKE_CURRENT_BINARY_DIRECTORY}/FrequencyMeasurement)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})

###############################################################################
#Linker settings
target_link_libraries(FrequencyMeasurement
	scopehal
	scopeprotocols
	yaml-cpp
	)
