add_executable (VulkanTest
	main.cpp
	)

target_link_libraries(
	VulkanTest
	scopehal
	scopeprotocols)

add_test(NAME VulkanSanityCheck COMMAND VulkanTest)
