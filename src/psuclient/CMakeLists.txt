#Set up include paths
include_directories(SYSTEM ${GTKMM_INCLUDE_DIRS} ${SIGCXX_INCLUDE_DIRS})
link_directories(${GTKMM_LIBRARY_DIRS} ${SIGCXX_LIBRARY_DIRS})

###############################################################################
#C++ compilation
add_executable(psuclient
	MainWindow.cpp
	main.cpp
)

###############################################################################
#Linker settings
target_link_libraries(psuclient
	scopehal
	${GTKMM_LIBRARIES}
	${SIGCXX_LIBRARIES}
	)

