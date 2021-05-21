# Use GLVND on Linux if available
if(POLICY CMP0072)
	cmake_policy(SET CMP0072 NEW)
endif()

# GLEW and OpenGL are required
find_package(GLEW REQUIRED)
find_package(OpenGL REQUIRED)
find_package(Yaml REQUIRED)

# Git is used for git-describe based version generation if we have it
find_package(Git)

# Additional libraries on Windows
if(WIN32)
	set(WIN_LIBS shlwapi)

	# For some reason, find_package does not manage to find glew32 on Windows. Hardcoding for now.
	# note by @spookyvision: maybe the addition of GLEW::GLEW below has fixed it and this special case 
	# can be removed? TODO test
	set(GLEW_LIBRARIES "glew32")
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
	set(GLSCOPECLIENT_VERSION "v0.1-unknown")
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
	FileSystem.cpp
	Framebuffer.cpp
	HaltConditionsDialog.cpp
	HistoryWindow.cpp
	InstrumentConnectionDialog.cpp
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
	ScopeSyncWizard.cpp
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
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	${OPENGL_LIBRARY}
	${WIN_LIBS}
	${GLEW_LIBRARIES}
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
	kernels
	COMMENT "Copying kernels..."
	COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/lib/scopeprotocols/kernels ${CMAKE_BINARY_DIR}/src/glscopeclient/kernels)
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

add_dependencies(glscopeclient
	gradients
	kernels
	shaders
	styles
	icons
)

# Windows portable zip/MSI installer build
if(WIN32)
	add_custom_target(
		dist_windows_x64
		COMMENT "Creating dist/windows_x64..."
		DEPENDS glscopeclient
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/gradients ${CMAKE_BINARY_DIR}/dist/windows_x64/gradients
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/lib/scopeprotocols/kernels ${CMAKE_BINARY_DIR}/dist/windows_x64/kernels
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/shaders ${CMAKE_BINARY_DIR}/dist/windows_x64/shaders
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/styles ${CMAKE_BINARY_DIR}/dist/windows_x64/styles
		COMMAND ${CMAKE_COMMAND} -E copy_directory
				${CMAKE_SOURCE_DIR}/src/glscopeclient/icons ${CMAKE_BINARY_DIR}/dist/windows_x64/icons
		COMMAND ${CMAKE_COMMAND} -E copy
				${CMAKE_SOURCE_DIR}/src/LICENSE 
				${CMAKE_BINARY_DIR}/lib/graphwidget/libgraphwidget.dll
				${CMAKE_BINARY_DIR}/lib/log/liblog.dll
				${CMAKE_BINARY_DIR}/lib/scopehal/libscopehal.dll
				${CMAKE_BINARY_DIR}/lib/scopeprotocols/libscopeprotocols.dll
				${CMAKE_BINARY_DIR}/src/glscopeclient/glscopeclient.exe
				${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND bash -c \"cp -R /mingw64/share/licenses ${CMAKE_BINARY_DIR}/dist/windows_x64\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/lib
		COMMAND bash -c \"cp -R /mingw64/lib/gdk-pixbuf-2.0 ${CMAKE_BINARY_DIR}/dist/windows_x64/lib\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/share/icons
		COMMAND bash -c \"cp -R /mingw64/share/icons/hicolor ${CMAKE_BINARY_DIR}/dist/windows_x64/share/icons\"
		COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/dist/windows_x64/share/glib-2.0
		COMMAND bash -c \"cp -R /mingw64/share/glib-2.0/schemas ${CMAKE_BINARY_DIR}/dist/windows_x64/share/glib-2.0\"
		COMMAND bash -c \"cd ${CMAKE_BINARY_DIR}/dist/windows_x64 && glib-compile-schemas ./share/glib-2.0/schemas\" 
		COMMAND bash -c \"ldd ${CMAKE_BINARY_DIR}/dist/windows_x64/glscopeclient.exe | grep '\/mingw64\/bin\/.*dll' -o  | xargs -I{} cp /mingw64/bin/gdbus.exe /mingw64/lib/libffts.dll \\\"{}\\\" ${CMAKE_BINARY_DIR}/dist/windows_x64\")

	add_custom_target(
		portable_zip_windows_x64 ALL
		COMMENT "Creating portable zip from dist/windows_x64..."
		DEPENDS dist_windows_x64
		WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/dist/windows_x64
		COMMAND ${CMAKE_COMMAND} -E tar "cf" "${CMAKE_BINARY_DIR}/dist/glscopeclient-${GLSCOPECLIENT_VERSION}-windows-x64-portable.zip" --format=zip -- .)

    if(WIXPATH)
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
install(DIRECTORY ${CMAKE_SOURCE_DIR}/lib/scopeprotocols/kernels
	DESTINATION share/glscopeclient)

install(FILES ${CMAKE_SOURCE_DIR}/src/glscopeclient/glscopeclient.desktop
	DESTINATION share/applications)
