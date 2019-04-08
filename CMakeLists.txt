cmake_minimum_required(VERSION 3.3)
project(jtaghal)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_C_FLAGS "-g -O2 -fopenmp -Wall -Wextra")
set(CMAKE_CXX_FLAGS "-g -O2 -fopenmp -Wall -Wextra --std=c++11")

find_package(PkgConfig)
pkg_check_modules(GTKMM gtkmm-3.0)
pkg_check_modules(SIGCXX sigc++-2.0)

add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopehal")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopeprotocols")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/scopemeasurements")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/xptools")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/log")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/scopeclient")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/glscopeclient")
add_subdirectory("${PROJECT_SOURCE_DIR}/src/curvetrace")

set_property(TARGET scopehal PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET log PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET xptools PROPERTY POSITION_INDEPENDENT_CODE ON)
set_property(TARGET scopeprotocols PROPERTY POSITION_INDEPENDENT_CODE ON)
