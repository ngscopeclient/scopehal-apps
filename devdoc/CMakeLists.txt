
# Developer documentation
if(BUILD_DEVDOCS)
	find_package(Doxygen
		REQUIRED dot mscgen dia)

	set(DOXYGEN_LOOKUP_CACHE_SIZE 2)

	doxygen_add_docs(devdocs
		${PROJECT_SOURCE_DIR}/devdoc
		${PROJECT_SOURCE_DIR}/src/ngscopeclient
		${PROJECT_SOURCE_DIR}/lib/log
		${PROJECT_SOURCE_DIR}/lib/xptools
		${PROJECT_SOURCE_DIR}/lib/scopehal
		${PROJECT_SOURCE_DIR}/lib/scopeprotocols
		WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
		COMMENT "Generate developer documentation"
	)
endif()

