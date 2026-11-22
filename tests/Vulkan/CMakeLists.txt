add_executable (VulkanTest
	main.cpp
	)

target_link_libraries(VulkanTest
	scopehal
	scopeprotocols)

add_test(NAME VulkanSanityCheck COMMAND VulkanTest)

# No catch_discover_tests here, but still needed for the test to actually execute
if(WIN32)
add_custom_command(TARGET VulkanTest POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:VulkanTest> $<TARGET_FILE_DIR:VulkanTest>
	COMMAND_EXPAND_LISTS
	)
endif()

if(${SECOND_TEST})
	set_property(TEST VulkanSanityCheck APPEND PROPERTY ENVIRONMENT "SCOPEHAL_VULKAN_DEVICE_OVERRIDE=${SECOND_TEST_DEVICE}")
endif()
