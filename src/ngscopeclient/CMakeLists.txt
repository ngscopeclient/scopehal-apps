#Set up include paths
include_directories(
	SYSTEM
	${SIGCXX_INCLUDE_DIRS}
	${GTKMM_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/cpp
	)
link_directories(${SIGCXX_LIBRARY_DIRS})

###############################################################################
#C++ compilation
add_executable(ngscopeclient
	../imgui/imgui.cpp
	../imgui/imgui_demo.cpp
	../imgui/imgui_draw.cpp
	../imgui/imgui_tables.cpp
	../imgui/imgui_widgets.cpp
	../imgui/backends/imgui_impl_glfw.cpp
	../imgui/backends/imgui_impl_vulkan.cpp
	../imgui/misc/cpp/imgui_stdlib.cpp

	Dialog.cpp
	VulkanWindow.cpp
	pthread_compat.cpp

	AddInstrumentDialog.cpp
	AddPowerSupplyDialog.cpp
	AddScopeDialog.cpp
	PowerSupplyDialog.cpp

	MainWindow.cpp
	PowerSupplyThread.cpp
	Session.cpp
	ScopeThread.cpp
	WaveformArea.cpp
	WaveformGroup.cpp

	main.cpp
)

###############################################################################
#Linker settings
target_link_libraries(ngscopeclient
	scopehal
	scopeprotocols
	scopeexports
	glfw
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

