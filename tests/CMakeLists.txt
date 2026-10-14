add_subdirectory("Acceleration")
add_subdirectory("Filters")
add_subdirectory("Primitives")
add_subdirectory("Vulkan")

# Do hardware-in-loop testing
if(ENABLE_HIL_TESTS)
	message(STATUS "Hardware-in-loop tests enabled")

	#Expect HIL device configuration to be passed as CMake configs
	if(NOT HIL_SIGGEN_PATH)
		message(FATAL_ERROR "Must specify HIL_SIGGEN_PATH if ENABLE_HIL_TESTS is set")
	endif()
	if(NOT HIL_TS_PATH)
		message(FATAL_ERROR "Must specify HIL_TS_PATH if ENABLE_HIL_TESTS is set")
	endif()

	message(STATUS "HIL test signal generator UART is at ${HIL_SIGGEN_PATH}, ThunderScope is at ${HIL_TS_PATH}")
	add_subdirectory("HIL")

else()
	message(STATUS "Hardware-in-loop tests disabled")
endif()
