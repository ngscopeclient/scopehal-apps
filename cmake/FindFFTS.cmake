if(NOT LIBFFTS_FOUND)
  pkg_check_modules (LIBFFTS_PKG ffts)

  find_path(LIBFFTS_INCLUDE_DIR NAMES ffts.h
	PATHS
	${LIBFFTS_PKG_INCLUDE_DIRS}
	/usr
	/usr/local
	$ENV{HOME}/.local
	/mingw64

	PATH_SUFFIXES
	include/ffts
  )
  find_library(LIBFFTS_LIBRARIES NAMES ffts libffts_static
	PATHS
	${LIBFFTS_PKG_LIBRARY_DIRS}
	/usr/lib
	/usr/local/lib
	$ENV{HOME}/.local/lib
	/mingw64/lib/
  )

if(LIBFFTS_INCLUDE_DIR AND LIBFFTS_LIBRARIES)
  set(LIBFFTS_FOUND TRUE CACHE INTERNAL "libffts found")
  message(STATUS "Found libffts: ${LIBFFTS_INCLUDE_DIR}, ${LIBFFTS_LIBRARIES}")
else(LIBFFTS_INCLUDE_DIR AND LIBFFTS_LIBRARIES)
  set(LIBFFTS_FOUND FALSE CACHE INTERNAL "libffts found")
  message(STATUS "libffts not found. Please clone/build and install it from https://github.com/anthonix/ffts.git")
endif(LIBFFTS_INCLUDE_DIR AND LIBFFTS_LIBRARIES)

mark_as_advanced(LIBFFTS_INCLUDE_DIR LIBFFTS_LIBRARIES)

endif(NOT LIBFFTS_FOUND)
