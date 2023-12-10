#Set up include paths
include_directories(SYSTEM ${CAIROMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
link_directories(${CAIROMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#C++ compilation
add_executable(curvetrace
	main.cpp
)

###############################################################################
#Linker settings
target_link_libraries(curvetrace
	scopehal
	scopeprotocols
	${CAIROMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

