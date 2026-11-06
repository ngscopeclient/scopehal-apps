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
	if(NOT HIL_RELAY_PATH)
		message(FATAL_ERROR "Must specify HIL_RELAY_PATH if ENABLE_HIL_TESTS is set")
	endif()

	message(STATUS "HIL FPGA board is at ${HIL_SIGGEN_PATH}, ThunderScope is at ${HIL_TS_PATH}, relay at ${HIL_RELAY_PATH}")
	add_subdirectory("HIL")

else()
	message(STATUS "Hardware-in-loop tests disabled")
endif()

# Update test configuration if we are doing a second round of testing
if(${SECOND_TEST})
	message(STATUS "Reconfiguring tests with Vulkan device override")

	get_property(TEST_LIST DIRECTORY Acceleration PROPERTY TESTS)
	set_property(TEST ${TEST_LIST} DIRECTORY Acceleration APPEND PROPERTY
		ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
	message(STATUS "Acceleration override list: ${TEST_LIST}")

	get_property(TEST_LIST DIRECTORY Filters PROPERTY TESTS)
	set_property(TEST ${TEST_LIST} DIRECTORY Filters APPEND PROPERTY
		ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
	message(STATUS "Filters override list: ${TEST_LIST}")

	get_property(TEST_LIST DIRECTORY Primitives PROPERTY TESTS)
	set_property(TEST ${TEST_LIST} DIRECTORY Primitives APPEND PROPERTY
		ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
	message(STATUS "Primitives override list: ${TEST_LIST}")

	get_property(TEST_LIST DIRECTORY Vulkan PROPERTY TESTS)
	set_property(TEST ${TEST_LIST} DIRECTORY Vulkan APPEND PROPERTY
		ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
	message(STATUS "Vulkan override list: ${TEST_LIST}")

	if(ENABLE_HIL_TESTS)
		get_property(TEST_LIST DIRECTORY HIL PROPERTY TESTS)
		set_property(TEST ${TEST_LIST} DIRECTORY HIL APPEND PROPERTY
			ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
	endif()

endif()
