cmake_minimum_required(VERSION 3.20)
include_directories(cmake) #WORKAROUND needed for find_package(Vulkan) for Vulkan SDK

#WORKAROUND: Vulkan SDK is incompatible with MinGW due to C++ linkage


# Check for Vulkan SDK env var and prepend it to CMAKE_PREFIX_PATH if set to prefer Vulkan SDK packages if available
if(DEFINED ENV{VULKAN_SDK})
	if(WIN32 AND (NOT MSVC))
		message(WARNING "Detected VULKAN_SDK at $ENV{VULKAN_SDK}.")
		message(WARNING "Compiling and linking against the Vulkan SDK is not supported with MSYS. The build system "
			      "will ignore the SDK and you will need to have the Vulkan packages installed from the MSYS repositories.")
	else()
		message("Detected and using VULKAN_SDK at $ENV{VULKAN_SDK}")
		cmake_path(CONVERT "$ENV{VULKAN_SDK}" TO_CMAKE_PATH_LIST VULKAN_SDK_PATH NORMALIZE)
		list(PREPEND CMAKE_PREFIX_PATH ${VULKAN_SDK_PATH})
	endif()
endif()

project(ngscopeclient)

include(CTest)

# Configuration settings
set(BUILD_DOCS CACHE BOOL "Build the documentation")
set(ANALYZE CACHE BOOL "Run static analysis on the code, requires cppcheck and clang-analyzer to be installed")
set(DISABLE_PCH CACHE BOOL "Disable precompiled headers as this may break certain configurations")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON) # error if compiler doesn't support c++17
set(CMAKE_CXX_EXTENSIONS OFF) # use c++17 instead of gnu++17

# PCH needs to be disabled before targets are created/added/PCH is added to them
if(DISABLE_PCH)
	set(CMAKE_DISABLE_PRECOMPILE_HEADERS ON)
endif()

# Compiler warnings for GCC/Clang
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
	add_compile_options(-Wall -Wextra -Wuninitialized)
	add_compile_options(-Wshadow -Wpedantic -Wcast-align -Wwrite-strings)
	add_compile_options(-Wmissing-declarations -Wvla)
    add_compile_options(-Werror -Wno-error=deprecated-declarations -Wno-error=unused-parameter -Wno-error=unused-variable -Wno-error=unused-result)
endif()

# Compiler warnings specific to GCC or Clang
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 13)
	add_compile_options(-Woverloaded-virtual=1)
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Woverloaded-virtual -Wpointer-sign -Wno-gnu-zero-variadic-macro-arguments -Wno-unknown-warning-option)
endif()
add_compile_options(-g -mtune=native)
add_compile_options("$<$<CONFIG:RELEASE>:-O3>")
add_compile_options("$<$<CONFIG:DEBUG>:-O0;-D_DEBUG>")

# Default build type
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

if(SANITIZE)
	add_compile_options(-fsanitize=address -fsanitize=undefined)
	add_link_options(-fsanitize=address -fsanitize=undefined)
endif()

if(WIN32)
	add_compile_options(-D_USE_MATH_DEFINES -D_POSIX_THREAD_SAFE_FUNCTIONS)
endif()

# Detect Apple Silicon as FFTS is not available for it
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
	add_compile_options(-D_APPLE_SILICON)
	set(APPLE_SILICON TRUE)
endif()


# Git is used for git-describe based version generation if we have it
find_package(Git)

# Package detection

find_package(PkgConfig MODULE REQUIRED)

# yaml-cpp is used in scopehal and ngscopeclient
find_package(yaml-cpp REQUIRED)
#WORKAROUND Needed for Debian Bullseye, which does not provide a yaml-cpp::yaml-cpp target
if(NOT TARGET yaml-cpp::yaml-cpp)
	find_library(YAML_CPP_LIBRARIES_FILES NAMES ${YAML_CPP_LIBRARIES})
	if(YAML_CPP_LIBRARIES_FILES MATCHES ".so$")
		add_library(yaml-cpp::yaml-cpp SHARED IMPORTED)
	elseif(YAML_CPP_LIBRARIES_FILES MATCHES ".a$")
		add_library(yaml-cpp::yaml-cpp STATIC IMPORTED)
	else()
		message(FATAL_ERROR "Unexpected partially-installed yaml-cpp, is it installed correctly?")
	endif()
	set_property(TARGET yaml-cpp::yaml-cpp PROPERTY IMPORTED_LOCATION ${YAML_CPP_LIBRARIES_FILES})
	#WORKAROUND The cmake file for yaml-cpp on debian bullseye is broken and provides a wrong YAML_CPP_INCLUDE_DIR
	find_path(YAML_CPP_INCLUDEFILES_DIR yaml-cpp/yaml.h REQUIRED)
	cmake_path(GET YAML_CPP_INCLUDEFILES_DIR PARENT_PATH YAML_CPP_INCLUDE_DIR)
	set_property(TARGET yaml-cpp::yaml-cpp PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${YAML_CPP_INCLUDEFILES_DIR})
endif()

pkg_check_modules(SIGCXX QUIET IMPORTED_TARGET sigc++-3.0) # look for latest version first
if(NOT SIGCXX_FOUND)
	pkg_check_modules(SIGCXX QUIET IMPORTED_TARGET sigc++-2.0) # look for older version too
endif()
if(NOT SIGCXX_FOUND)
	message(FATAL_ERROR "Unable to find any version of sigc++; this is required to build ngscopeclient.")
endif()

# cairomm is used by EyeMask
pkg_check_modules(CAIROMM QUIET IMPORTED_TARGET cairomm-1.16) # look for latest version first
if(NOT CAIROMM_FOUND)
	pkg_check_modules(CAIROMM QUIET IMPORTED_TARGET cairomm-1.0) # look for older version too
endif()
if(NOT CAIROMM_FOUND)
	message(FATAL_ERROR "Unable to find any version of cairomm; this is required to build ngscopeclient.")
endif()

# We still use gtk on Linux for the file browser dialog in "native" mode)
if(LINUX)
	pkg_check_modules(GTK QUIET IMPORTED_TARGET REQUIRED gtk+-3.0)
endif()

# find ffts, except on Apple Silicon where it's not supported
if(NOT APPLE_SILICON)
	# ffts ships a broken pkgconfig file on some platforms so we are not going to use it
	find_path(LIBFFTS_INCLUDE_DIRS ffts.h PATH_SUFFIXES ffts REQUIRED)
	find_library(LIBFFTS_LIBRARIES NAMES ffts libffts REQUIRED)
endif()

#WORKAROUND Fedora 38 does not include Threads from the glslang .cmake file, fixed in Fedora 39+
find_package(Threads MODULE REQUIRED)

# Configure and enable OpenMP
find_package(OpenMP MODULE)
if(NOT OpenMP_CXX_FOUND)
	message(FATAL_ERROR "ngscopeclient requires OpenMP but your C++ compiler does not appear to support it.")
endif()

# GLFW is required
find_package(glfw3 3.2 REQUIRED)

# Find Vulkan-related packages
#WORKAROUND glslang does not look for SPIRV-Tools-opt but needs it on macOS, harmless elsewhere
find_package(SPIRV-Tools-opt REQUIRED)
if(DEFINED VULKAN_SDK_PATH)
	#WORKAROUND The Vulkan SDK ships incomplete cmake config files.
	# Work around this, which does require FindVulkan.cmake.
	# FindVulkan does not find glslang correctly on macOS
	find_package(Vulkan REQUIRED COMPONENTS glslc shaderc_combined SPIRV-Tools)
	find_package(glslang REQUIRED)
	add_library(Vulkan::Loader ALIAS Vulkan::Vulkan)
else()
	find_package(glslang REQUIRED)
	find_package(VulkanHeaders REQUIRED)
	find_package(VulkanLoader QUIET)
	if(NOT VulkanLoader_FOUND) #WORKAROUND Alpine Linux has missing cmake files for VulkanLoader
		find_library(VulkanLoader_LIB libvulkan.so REQUIRED)
		cmake_path(GET VulkanLoader_LIB FILENAME VulkanLoader_LIB_NAME)
		add_library(Vulkan::Loader SHARED IMPORTED)
		set_property(TARGET Vulkan::Loader PROPERTY IMPORTED_LOCATION ${VulkanLoader_LIB})
	endif()
endif()
# The following should work regardless of Vulkan SDK or individual components
# shaderc does not provide a .cmake file, but does provide a pkgconfig file
# pkgconfig file is not provided by Vulkan SDK, however
pkg_check_modules(SHADERC shaderc QUIET IMPORTED_TARGET)
# This is needed due to shaderc not providing a programmatic way to derive the path of the glslc executable
if(SHADERC_FOUND)
	cmake_path(GET SHADERC_INCLUDE_DIRS PARENT_PATH SHADERC_PREFIX)
endif()
# This should find glslc from either shaderc or the Vulkan SDK
find_program(Vulkan_GLSLC_EXECUTABLE glslc HINTS SHADERC_PREFIX)

if(NOT Vulkan_GLSLC_EXECUTABLE)
	message(FATAL_ERROR "glslc not found. This is needed to compile shaders. Please install shaderc or load the Vulkan SDK.")
else()
	message("-- Found glslc: ${Vulkan_GLSLC_EXECUTABLE}")
endif()

# This is needed due to VkFFT not using a standard path for glslang_c_interface.h
get_target_property(glslang_INCLUDE_DIR glslang::glslang INTERFACE_INCLUDE_DIRECTORIES)
# Find MoltenVK on macOS, no need to link it later
if(APPLE)
	find_library(MOLTENVK_LIBRARIES NAMES MoltenVK libMoltenVK)
	if( NOT MOLTENVK_LIBRARIES )
		message( FATAL_ERROR "scopehal-apps requires MoltenVK to be installed in order to function on macOS.")
	endif()
endif()

if(NOT WIN32)
	include(GNUInstallDirs)
endif()

# Documentation
add_subdirectory("${PROJECT_SOURCE_DIR}/doc")
if(NOT BUILD_DOCS)
	set_property(TARGET doc PROPERTY EXCLUDE_FROM_ALL ON)
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
		message(FATAL_ERROR "CPPCheck not found, required for ANALYZE")
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
		message(FATAL_ERROR "clang-analyzer not found, required for ANALYZE")
	endif()
endif()

# Main project code
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopehal")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopeprotocols")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/xptools")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/log")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/ngscopeclient")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/nativefiledialog-extended")

# Unit tests
if(BUILD_TESTING)
	find_package(Catch2 REQUIRED)
	include(Catch)
	#Catch2 v3.x.y have a breaking change:
	# Must include catch2/catch_all.hpp instead of catch2/catch.hpp
	# So we set a compile flag to let the code know.
	if(NOT Catch2_VERSION MATCHES "^[0-2]\\.")
		add_compile_options(-D_CATCH2_V3)
	endif()
	add_subdirectory("${PROJECT_SOURCE_DIR}/tests")
endif()

# Example code and other utilities, don't build on non-POSIX yet
if(NOT WIN32)
	add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/curvetrace")
	#add_subdirectory("${PROJECT_SOURCE_DIR}/src/examples/usbcsv")
endif()

# Make sure all of our shared libraries are built relocatable
set_property(TARGET scopehal PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET log PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET xptools PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET scopeprotocols PROPERTY POSITION_INDEPENDENT_CODE ON)
