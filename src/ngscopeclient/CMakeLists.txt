#Set up include paths
include_directories(
	SYSTEM
	${SIGCXX_INCLUDE_DIRS}
	${GTKMM_INCLUDE_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../implot/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui-node-editor/
	${CMAKE_CURRENT_SOURCE_DIR}/../ImGuiFileDialog/
	${CMAKE_CURRENT_SOURCE_DIR}/../nativefiledialog-extended/src/include/
	)
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})
find_package(glfw3 REQUIRED)
find_package(PNG REQUIRED)

# use custom config for imguifiledialog
add_compile_definitions(CUSTOM_IMGUIFILEDIALOG_CONFIG="../ngscopeclient/IGFDConfig.h")

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
	../imgui-node-editor/imgui_node_editor.cpp
	../imgui-node-editor/imgui_node_editor_api.cpp
	../imgui-node-editor/imgui_canvas.cpp
	../imgui-node-editor/crude_json.cpp
	../ImGuiFileDialog/ImGuiFileDialog.cpp

	pthread_compat.cpp

	AddGeneratorDialog.cpp
	AddInstrumentDialog.cpp
	AddLoadDialog.cpp
	AddMultimeterDialog.cpp
	AddPowerSupplyDialog.cpp
	AddRFGeneratorDialog.cpp
	AddScopeDialog.cpp
	ChannelPropertiesDialog.cpp
	Dialog.cpp
	EmbeddableDialog.cpp
	EmbeddedTriggerPropertiesDialog.cpp
	FileBrowser.cpp
	FilterGraphEditor.cpp
	FilterPropertiesDialog.cpp
	FontManager.cpp
	FunctionGeneratorDialog.cpp
	GuiLogSink.cpp
	HistoryDialog.cpp
	HistoryManager.cpp
	IGFDFileBrowser.cpp
	LoadDialog.cpp
	LoadThread.cpp
	LogViewerDialog.cpp
	MainWindow.cpp
	MainWindow_Menus.cpp
	MeasurementsDialog.cpp
	MetricsDialog.cpp
	MultimeterDialog.cpp
	MultimeterThread.cpp
	NFDFileBrowser.cpp
	PacketManager.cpp
	PersistenceSettingsDialog.cpp
	PowerSupplyDialog.cpp
	PowerSupplyThread.cpp
	Preference.cpp
	PreferenceDialog.cpp
	PreferenceManager.cpp
	PreferenceSchema.cpp
	PreferenceTree.cpp
	ProtocolAnalyzerDialog.cpp
	RFGeneratorDialog.cpp
	RFSignalGeneratorThread.cpp
	ScopeThread.cpp
	SCPIConsoleDialog.cpp
	Session.cpp
	TextureManager.cpp
	TimebasePropertiesDialog.cpp
	TriggerPropertiesDialog.cpp
	VulkanWindow.cpp
	WaveformArea.cpp
	WaveformGroup.cpp
	WaveformThread.cpp

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


get_target_property(SPIRV_SHADERS protocolshaders SOURCES)

add_custom_target(
	ngprotoshaders
	COMMENT "Copying protocol shaders..."
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_SHADERS} ${CMAKE_BINARY_DIR}/src/ngscopeclient/shaders
	)
add_dependencies(ngprotoshaders
	protocolshaders)

add_dependencies(ngscopeclient
	ngfonts
	ngicons
	ngrendershaders
	ngprotoshaders
	)

add_subdirectory(shaders)

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
	nfd
	glfw
	PNG::PNG
	cairomm-1.0
	cairo
	${SIGCXX_LIBRARIES}
	)

