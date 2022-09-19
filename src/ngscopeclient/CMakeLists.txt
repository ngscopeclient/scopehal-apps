#Set up include paths
include_directories(
	SYSTEM
	${SIGCXX_INCLUDE_DIRS}
	${GTKMM_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../implot/
	)
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})
find_package(glfw3 REQUIRED)

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

	pthread_compat.cpp

	AddGeneratorDialog.cpp
	AddInstrumentDialog.cpp
	AddMultimeterDialog.cpp
	AddPowerSupplyDialog.cpp
	AddRFGeneratorDialog.cpp
	AddScopeDialog.cpp
	ChannelPropertiesDialog.cpp
	Dialog.cpp
	FunctionGeneratorDialog.cpp
	GuiLogSink.cpp
	LogViewerDialog.cpp
	MainWindow.cpp
	MainWindow_Menus.cpp
	MultimeterDialog.cpp
	MultimeterThread.cpp
	PowerSupplyDialog.cpp
	PowerSupplyThread.cpp
	Preference.cpp
	PreferenceManager.cpp
	PreferenceSchema.cpp
	PreferenceTree.cpp
	RFGeneratorDialog.cpp
	RFSignalGeneratorThread.cpp
	ScopeThread.cpp
	SCPIConsoleDialog.cpp
	Session.cpp
	TextureManager.cpp
	VulkanWindow.cpp
	WaveformArea.cpp
	WaveformGroup.cpp

	main.cpp
)

add_custom_target(
	ngfonts
	COMMENT "Copying fonts..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/ngscopeclient/fonts ${CMAKE_BINARY_DIR}/src/ngscopeclient/fonts)

add_custom_target(
	ngicons
	COMMENT "Copying icons..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/ngscopeclient/icons ${CMAKE_BINARY_DIR}/src/ngscopeclient/icons)

add_dependencies(ngscopeclient
	ngfonts
	ngicons
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
	png
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

