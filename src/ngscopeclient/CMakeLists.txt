#Set up include paths
include_directories(SYSTEM ${SIGCXX_INCLUDE_DIRS} ${GTKMM_INCLUDE_DIRS})
link_directories(${SIGCXX_LIBRARY_DIRS})

###############################################################################
#C++ compilation
add_executable(ngscopeclient
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

