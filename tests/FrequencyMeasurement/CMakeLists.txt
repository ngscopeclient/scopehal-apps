add_executable(FrequencyMeasurement
	main.cpp
)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})

###############################################################################
#Linker settings
target_link_libraries(FrequencyMeasurement
	scopehal
	scopeprotocols
	yaml-cpp
	)
