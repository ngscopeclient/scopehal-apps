add_executable(Filters
	main.cpp

	Filter_Add.cpp
	Filter_ACRMS.cpp
	Filter_EyePattern.cpp
	Filter_FIR.cpp
	Filter_Subtract.cpp
	Filter_Upsample.cpp

	FrequencyMeasurement.cpp
)

target_link_libraries(Filters
	scopehal
	scopeprotocols
	Catch2::Catch2
	)

pkg_search_module(FFTW fftw3f IMPORTED_TARGET)

if (${FFTW_FOUND})
	message("Found FFTW3, enabling unit tests which depend on it")
	target_link_libraries(Filters PkgConfig::FFTW)
	target_sources(Filters PRIVATE
			Filter_DeEmbed.cpp
			Filter_FFT.cpp
	)
else ()
	message("Didn't find FFTW3, disabling tests which depend on it")
endif ()

#Needed because Windows does not support RPATH and will otherwise not be able to find DLLs when catch_discover_tests runs the executable
if(WIN32)
add_custom_command(TARGET Filters POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:Filters> $<TARGET_FILE_DIR:Filters>
	COMMAND_EXPAND_LISTS
	)
endif()

catch_discover_tests(Filters)

add_dependencies(Filters
	ngprotoshaders
	nghalshaders
	)
