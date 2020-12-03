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
	set(GLEW_LIBRARIES "glew32")
endif()

#Set up include paths
include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS} ${GLEW_INCLUDE_DIRS} ${YAML_INCLUDES})
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
	ChannelPropertiesDialog.cpp
	FileProgressDialog.cpp
	FilterDialog.cpp
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

add_dependencies(glscopeclient
	gradients
	shaders
	styles
	icons
)
