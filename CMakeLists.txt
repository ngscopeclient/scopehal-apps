cmake_minimum_required(VERSION 3.16)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
project(glscopeclient)

include(CTest)

# Configuration settings
set(BUILD_DOCS CACHE BOOL "Build the documentation")
set(ANALYZE CACHE BOOL "Run static analysis on the code, requires cppcheck and clang-analyzer to be installed")

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # error if compiler doesn't support c++14
set(CMAKE_CXX_EXTENSIONS OFF) # use c++14 instead of gnu++14

# Compiler flags
set(WARNINGS "-Wall -Wextra -Wuninitialized ")
set(WARNINGS "${WARNINGS} -Wshadow -Wpedantic -Wcast-align -Wwrite-strings")
set(WARNINGS "${WARNINGS} -Wmissing-declarations -Wvla")

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(WARNINGS "${WARNINGS} ")
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(WARNINGS "${WARNINGS} -Wno-gnu-zero-variadic-macro-arguments -Wno-unknown-warning-option")
endif()

set(CMAKE_CXX_FLAGS "-g ${WARNINGS} -mtune=native -ffast-math -fsigned-char")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

# clang doesn't properly use the attributes and so requires avx to build
# should get fixed in #475
if((NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64") AND (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang"))
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
endif()

if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif(NOT CMAKE_BUILD_TYPE)

if(SANITIZE)
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fsanitize=address -D_DEBUG")
else()
	set(CMAKE_CXX_FLAGS_DEBUG "-O0 -D_DEBUG")
endif()

if(WIN32)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_USE_MATH_DEFINES -D_POSIX_THREAD_SAFE_FUNCTIONS")
endif()

# Package detection
find_package(PkgConfig)
pkg_check_modules(GTKMM REQUIRED gtkmm-3.0)
pkg_check_modules(SIGCXX REQUIRED sigc++-2.0)
find_package(FFTS)
include(FindOpenMP)
find_package(glfw3 3.2 REQUIRED)

# Use the local FindVulkan script until we can rely on all users having CMake 3.24 available, at which point we can remove it
set(Vulkan_FIND_COMPONENTS glslang shaderc_combined)
include(FindVulkan)

if(NOT Vulkan_FOUND)
	message(FATAL_ERROR "No Vulkan SDK found.")
endif()

if(NOT Vulkan_glslc_FOUND)
	message(FATAL_ERROR "No Vulkan glslc found. This is needed to compile shaders.")
endif()

# Add specific VulkanSDK Lib & Include for glslang (tested on Linux Ubuntu 22.04 LTS)
if(NOT WIN32)
	include_directories($ENV{VULKAN_SDK}/include/glslang/Include)
	link_directories($ENV{VULKAN_SDK}/lib)
endif()

# Includes are incomplete in VulkanSDK 1.3.224.1 so we use the include path from MINGW64 mingw-w64-x86_64-glslang
if(WIN32)
	find_path(Glslang_BUILD_INCLUDE_DIR
	  NAMES include/glslang/Include/glslang_c_interface.h
	  HINTS
		$ENV{GLSLANG_BUILD_PATH}
	  )
	mark_as_advanced(Glslang_BUILD_INCLUDE_DIR)

	if(Glslang_BUILD_INCLUDE_DIR)
		include_directories($ENV{GLSLANG_BUILD_PATH}/include/glslang/Include)
	else()
		message(FATAL_ERROR "Glslang_BUILD_INCLUDE_DIR Not found")
	endif()

	link_directories($ENV{GLSLANG_BUILD_PATH}/lib)
	link_directories($ENV{GLSLANG_BUILD_PATH}/lib/glslang)
endif()

if(NOT WIN32)
	include(GNUInstallDirs)
endif()

# Configure and enable OpenMP
if(NOT OpenMP_CXX_FOUND)
	message(FATAL_ERROR "glscopeclient requires OpenMP but your C++ compiler does not appear to support it.")
endif()
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

# Documentation
if(BUILD_DOCS)
	add_subdirectory("${PROJECT_SOURCE_DIR}/doc")
endif()

# Static analysis
if(ANALYZE)
	find_program(CPPCHECK_PATH cppcheck DOC "Path to cppcheck when ANALYZE is enabled")
	if(CPPCHECK_PATH)
		execute_process(COMMAND ${CPPCHECK_PATH} "--version" OUTPUT_VARIABLE CPPCHECK_VER_STR ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
		string(REPLACE "Cppcheck " "" CPPCHECK_VERSION ${CPPCHECK_VER_STR})
		if(CPPCHECK_VERSION VERSION_GREATER_EQUAL "2")
			set(CMAKE_CXX_CPPCHECK "${CPPCHECK_PATH};-DFT_USE_AUTOCONF_SIZEOF_TYPES;-D__GNUC__;--enable=warning,performance,portability;--suppress=*:*sigc*;--suppress=*:*glibmm*;--suppress=*:*gtkmm*;--inline-suppr;-q;--std=c++11")
			message(STATUS "Found CPPCheck: ${CPPCHECK_PATH} (found version \"${CPPCHECK_VERSION}\")")
		else()
			message(STATUS "Found CPPCheck: ${CPPCHECK_PATH} but ignored it as it was ${CPPCHECK_VERSION} < 2")
		endif()
	else()
		message(STATUS "CPPCheck not found")
	endif()
	# The actual clang-analyzer compiler wrapper doesn't get installed on $PATH, only scan-build which is useless to us
	find_program(CLANGANALYZER_SCANBUILD_PATH scan-build DOC "Path to clang-analyzer's scan-build tool, used as a hint to find the rest of the clang-analyzer")
	get_filename_component(CLANGANALYZER_SCANBUILD_BIN ${CLANGANALYZER_SCANBUILD_PATH} REALPATH)
	get_filename_component(CLANGANALYZER_BIN_PATH ${CLANGANALYZER_SCANBUILD_BIN} DIRECTORY)
	find_program(CLANGANALYZER_CXXANALYZER_PATH "c++-analyzer" HINTS "${CLANGANALYZER_BIN_PATH}/../libexec" DOC "Path to clang-analyzer's c++-analyzer")
	if(CLANGANALYZER_CXXANALYZER_PATH)
		set(CMAKE_CXX_COMPILER_LAUNCHER "${CLANGANALYZER_CXXANALYZER_PATH}")
		message(STATUS "Found clang-analyzer: ${CLANGANALYZER_CXXANALYZER_PATH}")
	else()
		message(STATUS "clang-analyzer not found")
	endif()
endif()

# Main project code
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/graphwidget")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopeexports")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopehal")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopeprotocols")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/xptools")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/log")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/glscopeclient")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/ngscopeclient")

# Unit tests
if(BUILD_TESTING)
	find_package(Catch2 REQUIRED)
	include(Catch)
	add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
endif()

# Example code and other utilities, don't build on non-POSIX yet
if(NOT WIN32)
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/curvetrace")
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/impulse")
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/scopevna")
	#add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/usbcsv")
endif()

# Make sure all of our shared libraries are built relocatable
set_property(TARGET scopehal PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET log PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET xptools PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET scopeprotocols PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET scopeexports PROPERTY POSITION_INDEPENDENT_CODE ON)
