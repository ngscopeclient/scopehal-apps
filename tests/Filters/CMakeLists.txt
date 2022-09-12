add_executable(Filters
	main.cpp

	Filter_DeEmbed.cpp
	Filter_FIR.cpp
	Filter_FFT.cpp
	Filter_Subtract.cpp
	Filter_Upsample.cpp

	FrequencyMeasurement.cpp
)

catch_discover_tests(Filters)

include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
target_link_directories(Filters PUBLIC ${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#Linker settings
target_link_libraries(Filters
	scopehal
	scopeprotocols
	${YAML_LIBRARIES}
	Catch2::Catch2
	)
