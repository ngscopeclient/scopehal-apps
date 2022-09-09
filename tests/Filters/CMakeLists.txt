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

###############################################################################
#Linker settings
target_link_libraries(Filters
	scopehal
	scopeprotocols
	yaml-cpp
	Catch2::Catch2
	)
