#Set up include paths
include_directories(${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#C++ compilation
add_executable(scopeclient
	DMMWindow.cpp
	OscilloscopeView.cpp
	OscilloscopeWindow.cpp
	ProtocolDecoderDialog.cpp
	PSUWindow.cpp
	ScopeConnectionDialog.cpp

	main.cpp
)

###############################################################################
#Linker settings
target_link_libraries(scopeclient
	scopehal
	scopeprotocols
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

