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
find_package(PNG REQUIRED)

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
	MetricsDialog.cpp
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
	TimebasePropertiesDialog.cpp
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

# Compile the waveform render shader in all its variations
# This function also implicitly adds the HAS_INT64 and DENSE_PACK variants for each input
function(add_render_shader_variant outname)
	cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "OPTIONS")
	set(source shaders/waveform-compute.glsl)
	get_filename_component(base ${source} NAME_WE)
	set(outfile ${CMAKE_CURRENT_BINARY_DIR}/${base}.${outname}.spv)
	add_custom_command(
		OUTPUT ${outfile}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${source}
		COMMENT "Compile compute shader ${base}.${outname} with ${arg_OPTIONS}"
		COMMAND ${Vulkan_GLSLC_EXECUTABLE} --target-env=vulkan1.0 -c ${CMAKE_CURRENT_SOURCE_DIR}/${source} ${arg_OPTIONS} -o ${outfile})
	install(FILES ${outfile} DESTINATION share/glscopeclient/shaders)
	if(NOT outname MATCHES "int64")
		add_render_shader_variant(${outname}.int64 OPTIONS ${arg_OPTIONS} -DHAS_INT64)
	endif()
	if(NOT outname MATCHES "dense")
		add_render_shader_variant(${outname}.dense OPTIONS ${arg_OPTIONS} -DDENSE_PACK)
	endif()
endfunction()

add_render_shader_variant(analog    OPTIONS -DANALOG_PATH)
add_render_shader_variant(digital   OPTIONS -DDIGITAL_PATH)
add_render_shader_variant(histogram OPTIONS -DANALOG_PATH -DNO_INTERPOLATION -DHISTOGRAM_PATH)

add_custom_target(
	ngrendershaders
	COMMAND ${CMAKE_COMMAND} -E true
	SOURCES
		waveform-compute.analog.spv
		waveform-compute.digital.spv
		waveform-compute.histogram.spv
		waveform-compute.analog.int64.spv
		waveform-compute.digital.int64.spv
		waveform-compute.histogram.int64.spv
		waveform-compute.analog.dense.spv
		waveform-compute.digital.dense.spv
		waveform-compute.histogram.dense.spv
		waveform-compute.analog.int64.dense.spv
		waveform-compute.digital.int64.dense.spv
		waveform-compute.histogram.int64.dense.spv
	)

add_dependencies(ngscopeclient
	ngfonts
	ngicons
	ngrendershaders
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
	glfw
	PNG::PNG
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

