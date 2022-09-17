# Use GLVND on Linux if available
if(POLICY CMP0072)
	cmake_policy(SET CMP0072 NEW)
endif()

# GLEW and OpenGL are required
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)

# Git is used for git-describe based version generation if we have it
find_package(Git)

# Additional libraries on Windows
if(WIN32)
	set(WIN_LIBS shlwapi)
	# The packaging for yaml-cpp on mingw64 seems to be broken, not providing a
	# libyaml-cpp.dll.a to be found by find_library inside of find_package(YAML)
	# when using FindYAML.cmake, just set the library directly
	set(YAML_LIBRARIES yaml-cpp)
else()
	find_package(YAML REQUIRED)
endif()

#Set up include paths
include_directories(SYSTEM ${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${YAML_INCLUDES})
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

#Set up versioning (with a dummy string for now if Git isn't present)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} describe --always --tags
		WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
		OUTPUT_VARIABLE GLSCOPECLIENT_VERSION
		OUTPUT_STRIP_TRAILING_WHITESPACE)
else()
	set(GLSCOPECLIENT_VERSION "unknown")
endif()
include_directories(${CMAKE_CURRENT_BINARY_DIR})
configure_file(glscopeclient-version.h.in glscopeclient-version.h)

###############################################################################
#C++ compilation
add_executable(glscopeclient
	pthread_compat.cpp
	ChannelPropertiesDialog.cpp
	FileProgressDialog.cpp
	FilterDialog.cpp
	FilterGraphEditor.cpp
	FilterGraphEditorWidget.cpp
	Framebuffer.cpp
	FunctionGeneratorDialog.cpp
	HaltConditionsDialog.cpp
	HistoryWindow.cpp
	InstrumentConnectionDialog.cpp
	MultimeterConnectionDialog.cpp
	MultimeterDialog.cpp
	OscilloscopeWindow.cpp
	Program.cpp
	Preference.cpp
	PreferenceTree.cpp
	PreferenceManager.cpp
	PreferenceDialog.cpp
	PreferenceSchema.cpp
	ProtocolAnalyzerWindow.cpp
	ProtocolTreeModel.cpp
	ScopeApp.cpp
	ScopeInfoWindow.cpp
	ScopeSyncWizard.cpp
	SCPIConsoleDialog.cpp
	Shader.cpp
	ShaderStorageBuffer.cpp
	Texture.cpp
	TimebasePropertiesDialog.cpp
	Timeline.cpp
	TriggerPropertiesDialog.cpp
	VertexArray.cpp
	VertexBuffer.cpp
	WaveformArea.cpp
	WaveformArea_events.cpp
	WaveformArea_rendering.cpp
	WaveformArea_cairo.cpp
	WaveformGroup.cpp
	WaveformGroupPropertiesDialog.cpp
	WaveformProcessingThread.cpp

	main.cpp
)

###############################################################################
#Linker settings
target_link_libraries(glscopeclient
	scopehal
	scopeprotocols
	scopeexports
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	${OPENGL_LIBRARY}
	${WIN_LIBS}
	GLEW::GLEW
	${YAML_LIBRARIES}
	)

###############################################################################
#Copy the resources to the build directory

add_custom_target(
	gradients
	COMMENT "Copying gradients..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glscopeclient/gradients ${CMAKE_BINARY_DIR}/src/glscopeclient/gradients)
add_custom_target(
	shaders
	COMMENT "Copying shaders..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glscopeclient/shaders ${CMAKE_BINARY_DIR}/src/glscopeclient/shaders)
add_custom_target(
	styles
	COMMENT "Copying styles..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glscopeclient/styles ${CMAKE_BINARY_DIR}/src/glscopeclient/styles)
add_custom_target(
	icons
	COMMENT "Copying icons..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/src/glscopeclient/icons ${CMAKE_BINARY_DIR}/src/glscopeclient/icons)

file(GLOB SPIRV_SHADERS "${CMAKE_BINARY_DIR}/lib/scopeprotocols/shaders/*")
add_custom_target(
	protoshaders
	COMMENT "Copying SPIR-V shaders..."
	COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SPIRV_SHADERS} ${CMAKE_BINARY_DIR}/src/glscopeclient/shaders
	)

# can't copy protocol shaders until they've been compiled
# also rely on shaders target to make sure the dest dir exists
add_dependencies(protoshaders
	scopeprotocols
	shaders
	)

add_dependencies(glscopeclient
	gradients
	protoshaders
	shaders
	styles
	icons
)

# Windows portable zip/MSI installer build
if(WIXPATH AND WIN32)
	add_custom_target(
		dist_windows_x64
		COMMENT "Creating dist/windows_x64..."
		DEPENDS glscopeclient
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/gradients ${CMAKE_BINARY_DIR}/dist/windows_x64/gradients
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/shaders ${CMAKE_BINARY_DIR}/dist/windows_x64/shaders
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/styles ${CMAKE_BINARY_DIR}/dist/windows_x64/styles
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/icons ${CMAKE_BINARY_DIR}/dist/windows_x64/icons
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/masks ${CMAKE_BINARY_DIR}/dist/windows_x64/masks
		COMMAND ${CMAKE_COMMAND} -E copy
				${CMAKE_SOURCE_DIR}/src/LICENSE
				${CMAKE_BINARY_DIR}/lib/graphwidget/libgraphwidget.dll
				${CMAKE_BINARY_DIR}/lib/log/liblog.dll
				${CMAKE_BINARY_DIR}/lib/scopehal/libscopehal.dll
				${CMAKE_BINARY_DIR}/lib/scopeprotocols/libscopeprotocols.dll
				${CMAKE_BINARY_DIR}/src/glscopeclient/glscopeclient.exe
				${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND bash -c \"cp -R ${CMAKE_BINARY_DIR}/lib/scopeprotocols/shaders ${CMAKE_BINARY_DIR}/dist/windows_x64\"
		COMMAND bash -c \"cp -R /mingw64/share/licenses ${CMAKE_BINARY_DIR}/dist/windows_x64\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/lib
		COMMAND bash -c \"cp -R /mingw64/lib/gdk-pixbuf-2.0 ${CMAKE_BINARY_DIR}/dist/windows_x64/lib\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/share/icons
		COMMAND bash -c \"cp -R /mingw64/share/icons/hicolor ${CMAKE_BINARY_DIR}/dist/windows_x64/share/icons\"
		COMMAND bash -c \"cp -R /mingw64/share/icons/Adwaita ${CMAKE_BINARY_DIR}/dist/windows_x64/share/icons\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/share/glib-2.0
		COMMAND bash -c \"cp -R /mingw64/share/glib-2.0/schemas ${CMAKE_BINARY_DIR}/dist/windows_x64/share/glib-2.0\"
		COMMAND bash -c \"cd ${CMAKE_BINARY_DIR}/dist/windows_x64 && glib-compile-schemas ./share/glib-2.0/schemas\"
		COMMAND bash -c \"ldd ${CMAKE_BINARY_DIR}/dist/windows_x64/glscopeclient.exe | grep '\/mingw64\/bin\/.*dll' -o  | xargs -I{} cp /mingw64/bin/gdbus.exe \\\"{}\\\" ${CMAKE_BINARY_DIR}/dist/windows_x64\")

	add_custom_target(
		portable_zip_windows_x64 ALL
		COMMENT "Creating portable zip from dist/windows_x64..."
		DEPENDS dist_windows_x64
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_BINARY_DIR}/dist/glscopeclient-${GLSCOPECLIENT_VERSION}-windows-x64-portable.zip" --format=zip -- .)

	configure_file(wix/glscopeclient.wxs.in ${CMAKE_BINARY_DIR}/glscopeclient.wxs)
	add_custom_target(
		msi_windows_x64 ALL
		COMMENT "Creating Windows x64 MSI..."
		DEPENDS dist_windows_x64
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/src/glscopeclient/wix/LICENSE.rtf ${CMAKE_BINARY_DIR}
		COMMAND ${WIXPATH}/heat dir dist/windows_x64 -gg -ke -scom -sreg -srd -cg AllFiles -dr APPLICATIONFOLDER -sfrag -template fragment -out glscopeclient-files.wxs
		COMMAND ${WIXPATH}/candle -nologo glscopeclient-files.wxs -arch x64 -out glscopeclient-files.wixobj
		COMMAND ${WIXPATH}/candle -nologo glscopeclient.wxs -arch x64 -out glscopeclient.wixobj -ext WixUIExtension -ext WixUtilExtension
		COMMAND ${WIXPATH}/light -nologo glscopeclient-files.wixobj glscopeclient.wixobj -b dist/windows_x64 -out dist/glscopeclient-${GLSCOPECLIENT_VERSION}-windows-x64.msi -ext WixUIExtension -ext WixUtilExtension)
else()
	message("Skipping MSI package build; define WIXPATH to enable")
endif()

###############################################################################
# Installation

install(TARGETS glscopeclient RUNTIME)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/glscopeclient/gradients
	DESTINATION share/glscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/glscopeclient/shaders
	DESTINATION share/glscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/glscopeclient/styles
	DESTINATION share/glscopeclient)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/src/glscopeclient/icons
	DESTINATION share/glscopeclient)

install(FILES ${CMAKE_SOURCE_DIR}/src/glscopeclient/glscopeclient.desktop
	DESTINATION share/applications)
