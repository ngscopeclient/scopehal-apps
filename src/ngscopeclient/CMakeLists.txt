#Set up include paths
include_directories(
	SYSTEM
	${SIGCXX_INCLUDE_DIRS}
	${GTKMM_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../implot/
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

	../implot/implot.cpp
	../implot/implot_demo.cpp
	../implot/implot_items.cpp

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

# implot has a bunch of warnings, turn them off to avoid cluttering the build
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set_source_files_properties(
	../implot/implot.cpp
	../implot/implot_demo.cpp
	../implot/implot_items.cpp
	PROPERTIES COMPILE_FLAGS -Wno-missing-declarations)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set_source_files_properties(
	../implot/implot.cpp
	../implot/implot_demo.cpp
	../implot/implot_items.cpp
	PROPERTIES COMPILE_FLAGS -Wno-missing-declarations)
endif()


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

