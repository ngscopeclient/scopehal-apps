set(CTEST_PROJECT_NAME ngscopeclient)

# Identify host system
cmake_host_system_information(RESULT HOSTNAME QUERY HOSTNAME)
message(STATUS "Found hostname: ${HOSTNAME}")

# Get hardware info
include(ProcessorCount)
ProcessorCount(N_PROCS)
message(STATUS "Detected ${N_PROCS} processor cores")

# default git location unless otherwise specified
set(CTEST_GIT_COMMAND "/usr/bin/git")

# default build location unless otherwise specified
set(CTEST_BINARY_DIRECTORY "/home/ci/scopehal-apps/build")
set(CTEST_SOURCE_DIRECTORY "/home/ci/scopehal-apps/")

# default generator unless otherwise specified
set(CTEST_CMAKE_GENERATOR "Unix Makefiles")

# Increase maximum output size for uploading
set(CTEST_CUSTOM_MAXIMUM_FAILED_TEST_OUTPUT_SIZE "131072")
set(CTEST_CUSTOM_MAXIMUM_PASSED_TEST_OUTPUT_SIZE "131072")

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
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-stable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON")

# CI Debian Bookworm
elseif(${HOSTNAME} STREQUAL "debian-oldstable" )
	set(CTEST_SITE ci-debian-oldstable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-12)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-oldstable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON")

# CI Ubuntu Noble
elseif(${HOSTNAME} STREQUAL "ubuntu-oldlts" )
	set(CTEST_SITE ci-ubuntu-oldlts)
	set(CTEST_BUILD_NAME x86_64-linux-ubuntu-24-04)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: ubuntu-oldlts")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON")

else()

	#set(CTEST_SITE ${HOSTNAME})
	#set(N_PROCS 4)
	message(FATAL_ERROR "No test config matched")
endif()

ctest_start(${CTEST_DASHBOARD})

#report version number but don't do anything else
set(CTEST_UPDATE_VERSION_ONLY 1)
ctest_update()

ctest_configure(OPTIONS "${CONFIGURE_OPTIONS}")
ctest_build(FLAGS "-j${N_PROCS}")
ctest_test()
#ctest_coverage()
set(CTEST_DROP_METHOD https)
set(CTEST_DROP_SITE dashboard.ngscopeclient.org)
set(CTEST_DROP_LOCATION /submit.php?project=ngscopeclient)
ctest_submit()
