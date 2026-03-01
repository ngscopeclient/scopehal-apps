set(CTEST_PROJECT_NAME ngscopeclient)

# Identify host system
cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)
message(STATUS "Found hostname: ${HOSTNAME}")

# Get hardware info
include(ProcessorCount)
ProcessorCount(N_PROCS)
message(STATUS "Detected ${N_PROCS} processor cores")

# azonenberg's dev box for testing
if(${HOSTNAME} STREQUAL "havequick" )
	set(CTEST_SITE dev-havequick)
	set(CTEST_BUILD_NAME x86_64-linux-debian-13-nvidia)
	set(CTEST_DASHBOARD Experimental)
	message(STATUS "Found known dev config: havequick")

	set(CTEST_BINARY_DIRECTORY ".")
	set(CTEST_SOURCE_DIRECTORY "..")

# CI Debian Trixie
elseif(${HOSTNAME} STREQUAL "debian-stable" )
	set(CTEST_SITE ci-debian-stable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-13-nvidia)
	set(CTEST_DASHBOARD Continuous)
	message(STATUS "Found known CI config: debian-stable")

	set(CTEST_BINARY_DIRECTORY "/home/ci/scopehal-apps/build")
	set(CTEST_SOURCE_DIRECTORY "/home/ci/scopehal-apps/")
	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CTEST_CMAKE_GENERATOR "Unix Makefiles")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON")
else()

	#set(CTEST_SITE ${HOSTNAME})
	#set(N_PROCS 4)
	message(FATAL_ERROR "No test config matched")
endif()

#set(CTEST_BUILD_COMMAND "make -j${N_PROCS}")

ctest_start(${CTEST_DASHBOARD})

#don't do ctest_update, prepping the source tree isn't ctest's job
#ctest_update()

ctest_configure(OPTIONS "${CONFIGURE_OPTIONS}")
ctest_build(FLAGS "-j${N_PROCS}")
ctest_test()
#ctest_coverage()
set(CTEST_DROP_METHOD https)
set(CTEST_DROP_SITE dashboard.ngscopeclient.org)
set(CTEST_DROP_LOCATION /submit.php?project=ngscopeclient)
ctest_submit()
