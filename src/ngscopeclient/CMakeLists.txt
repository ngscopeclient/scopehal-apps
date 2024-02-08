# At the moment, this is meant to be built as part of scopehal-apps.
# Standalone build/install is a potential long-term goal.
#If there is interest in a standalone build, please reach out. 

# Vulkan is required for compute shaders used in rendering, but is pulled in from the top level CMakeLists.

find_package(PNG REQUIRED)

#Set up versioning (with a dummy string for now if Git isn't present)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always --tags
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE NGSCOPECLIENT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
	set(NGSCOPECLIENT_VERSION "unknown")
endif()

configure_file(ngscopeclient-version.h.in ngscopeclient-version.h)

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

	AboutDialog.cpp
	AddBERTDialog.cpp
	AddGeneratorDialog.cpp
	AddInstrumentDialog.cpp
	AddLoadDialog.cpp
	AddMiscDialog.cpp
	AddMultimeterDialog.cpp
	AddPowerSupplyDialog.cpp
	AddRFGeneratorDialog.cpp
	AddScopeDialog.cpp
	AddSDRDialog.cpp
	AddSpectrometerDialog.cpp
	AddVNADialog.cpp
	BERTDialog.cpp
	BERTInputChannelDialog.cpp
	BERTOutputChannelDialog.cpp
	BERTThread.cpp
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
	KDialogFileBrowser.cpp
	LoadDialog.cpp
	LoadThread.cpp
	LogViewerDialog.cpp
	MainWindow.cpp
	MainWindow_Menus.cpp
	ManageInstrumentsDialog.cpp
	MeasurementsDialog.cpp
	MetricsDialog.cpp
	MiscInstrumentThread.cpp
	MultimeterDialog.cpp
	MultimeterThread.cpp
	NFDFileBrowser.cpp
	NotesDialog.cpp
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
	ScopeDeskewWizard.cpp
	ScopeThread.cpp
	SCPIConsoleDialog.cpp
	Session.cpp
	TextureManager.cpp
	TimebasePropertiesDialog.cpp
	TriggerGroup.cpp
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

get_target_property(SPIRV_SHADERS2 halshaders SOURCES)
add_custom_target(
	nghalshaders
	COMMENT "Copying scopehal shaders..."
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_SHADERS2} ${CMAKE_BINARY_DIR}/src/ngscopeclient/shaders
	)
add_dependencies(nghalshaders
	halshaders)

add_dependencies(ngscopeclient
	ngfonts
	ngicons
	ngrendershaders
	ngprotoshaders
	nghalshaders
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
#Set up include paths
target_include_directories(ngscopeclient
	PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}
	)
target_include_directories(ngscopeclient
	SYSTEM PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui/misc/cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui_markdown
	${CMAKE_CURRENT_SOURCE_DIR}/../implot/
	${CMAKE_CURRENT_SOURCE_DIR}/../imgui-node-editor/
	${CMAKE_CURRENT_SOURCE_DIR}/../ImGuiFileDialog/
	${CMAKE_CURRENT_SOURCE_DIR}/../nativefiledialog-extended/src/include/
	)

#Linker settings
target_link_libraries(ngscopeclient
	scopehal
	scopeprotocols
	nfd
	PNG::PNG
	)

#Needed to run from tree without install because Windows does not support RPATH and will otherwise not be able to find DLLs
if(WIN32)
add_custom_command(TARGET ngscopeclient POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:ngscopeclient> $<TARGET_FILE_DIR:ngscopeclient>
	COMMAND_EXPAND_LISTS
	)
endif()
###############################################################################
# Installation
install(TARGETS ngscopeclient RUNTIME)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ngscopeclient/fonts
	DESTINATION share/ngscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ngscopeclient/icons
	DESTINATION share/ngscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ngscopeclient/masks
	DESTINATION share/ngscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/ngscopeclient/shaders
	DESTINATION share/ngscopeclient)

install(FILES ${CMAKE_SOURCE_DIR}/src/ngscopeclient/ngscopeclient.desktop
	DESTINATION share/applications)

# ngscopeclient Windows portable zip/MSI installer build
if(WIXPATH AND WIN32)
	# Run the command to get /mingw64/bin full path using where gcc
	execute_process(
		COMMAND where gcc
		RESULT_VARIABLE GCC_RESULT
		OUTPUT_VARIABLE GCC_OUTPUT
		ERROR_QUIET
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	# Check the result of the command
	if(NOT GCC_RESULT EQUAL 0)
		message(FATAL_ERROR "gcc not found!")
	endif()
	# Get the directory path from gcc output which corresponds to full path related to /mingw64/bin
	get_filename_component(MINGW64_BIN_PATH ${GCC_OUTPUT} DIRECTORY)
	# Print the path (optional)
	message("MINGW64_BIN_PATH: ${MINGW64_BIN_PATH}")
	add_custom_target(
		ngscopeclient_dist_windows_x64
		COMMENT "Creating dist/ngscopeclient_windows_x64..."
		DEPENDS ngscopeclient
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/ngscopeclient/fonts ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/fonts
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/ngscopeclient/icons ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/icons
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/ngscopeclient/masks ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/masks

		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_BINARY_DIR}/lib/scopeprotocols/shaders ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/shaders
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_BINARY_DIR}/src/ngscopeclient/shaders ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/shaders

		COMMAND ${CMAKE_COMMAND} -E copy
				${CMAKE_SOURCE_DIR}/src/LICENSE
				${CMAKE_BINARY_DIR}/lib/log/liblog.dll
				${CMAKE_BINARY_DIR}/lib/scopehal/libscopehal.dll
				${CMAKE_BINARY_DIR}/lib/scopeprotocols/libscopeprotocols.dll
				${CMAKE_BINARY_DIR}/src/ngscopeclient/ngscopeclient.exe
				${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64
		COMMAND bash -c \"cp -R /mingw64/share/licenses ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64\"
		COMMAND bash -c \"wget https://github.com/bvernoux/mingw-bundledlls/releases/download/v0.2.4/mingw-bundledlls-0.2.4-1-x86_64.pkg.tar.zst && pacman -U --noconfirm mingw-bundledlls-*.pkg.tar.zst && rm -f mingw-bundledlls-*.pkg.tar.zst\"
		COMMAND bash -c \"export MINGW_BUNDLEDLLS_SEARCH_PATH='${MINGW64_BIN_PATH}\;../../lib/log\;../../lib/scopeexports\;../../lib/scopehal\;../../lib/scopeprotocols\;../../lib/xptools' && mingw-bundledlls ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64/ngscopeclient.exe --copy\")

	add_custom_target(
		ngscopeclient_portable_zip_windows_x64 ALL
		COMMENT "Creating portable zip from dist/ngscopeclient_dist_windows_x64..."
		DEPENDS ngscopeclient_dist_windows_x64
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist/ngscopeclient_windows_x64
		COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_BINARY_DIR}/dist/ngscopeclient-${NGSCOPECLIENT_VERSION}-windows-x64-portable.zip" --format=zip -- .)

	configure_file(wix/ngscopeclient.wxs.in ${CMAKE_BINARY_DIR}/ngscopeclient.wxs)
	add_custom_target(
		msi_windows_x64_ng ALL
		COMMENT "Creating ngscopeclient Windows x64 MSI..."
		DEPENDS ngscopeclient_dist_windows_x64
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/ngscopeclient/wix/LICENSE.rtf ${CMAKE_BINARY_DIR}
		COMMAND ${WIXPATH}/heat dir dist/ngscopeclient_windows_x64 -gg -ke -scom -sreg -srd -cg AllFiles -dr APPLICATIONFOLDER -sfrag -template fragment -out ngscopeclient-files.wxs
		COMMAND ${WIXPATH}/candle -nologo ngscopeclient-files.wxs -arch x64 -out ngscopeclient-files.wixobj
		COMMAND ${WIXPATH}/candle -nologo ngscopeclient.wxs -arch x64 -out ngscopeclient.wixobj -ext WixUIExtension -ext WixUtilExtension
		COMMAND ${WIXPATH}/light -nologo ngscopeclient-files.wixobj ngscopeclient.wixobj -b dist/ngscopeclient_windows_x64 -out dist/ngscopeclient-${NGSCOPECLIENT_VERSION}-windows-x64.msi -ext WixUIExtension -ext WixUtilExtension)
elseif(WIN32)
	message("Skipping MSI package build; define WIXPATH to enable")
endif()

