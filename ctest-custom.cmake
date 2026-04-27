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

# CI Arch
elseif(${HOSTNAME} STREQUAL "arch" )
	set(CTEST_SITE ci-arch)
	set(CTEST_BUILD_NAME x86_64-linux-arch-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: arch")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Debian Trixie
elseif(${HOSTNAME} STREQUAL "debian-stable" )
	set(CTEST_SITE ci-debian-stable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-13-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-stable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Debian Bookworm
elseif(${HOSTNAME} STREQUAL "debian-oldstable" )
	set(CTEST_SITE ci-debian-oldstable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-12-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-oldstable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Ubuntu Noble
elseif(${HOSTNAME} STREQUAL "ubuntu-oldlts" )
	set(CTEST_SITE ci-ubuntu-oldlts)
	set(CTEST_BUILD_NAME x86_64-linux-ubuntu-24-04-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: ubuntu-oldlts")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Ubuntu Resolute
elseif(${HOSTNAME} STREQUAL "ubuntu-lts" )
	set(CTEST_SITE ci-ubuntu-lts)
	set(CTEST_BUILD_NAME x86_64-linux-ubuntu-26-04-llvmpipe)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: ubuntu-lts")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Fedora 43
elseif(${HOSTNAME} STREQUAL "fedora" )
	set(CTEST_SITE ci-fedora)
	set(CTEST_BUILD_NAME x86_64-linux-fedora-43-llvmpipe)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: fedora")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI Windows 11
elseif(${HOSTNAME} STREQUAL "win11" )
	set(CTEST_SITE ci-win11)
	set(CTEST_BUILD_NAME x86_64-windows-11-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "c:\\msys64\\usr\\bin\\git.exe")
	set(CTEST_BINARY_DIRECTORY "c:\\Users\\ci\\scopehal-apps\\build")
	set(CTEST_SOURCE_DIRECTORY "c:\\Users\\ci\\scopehal-apps")
	set(CTEST_CMAKE_GENERATOR "Ninja")
	message(STATUS "Found known CI config: win11")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=ON")

# CI MacOS ARM64, for whatever reason it reports the FQDN not just the hostname
elseif(${HOSTNAME} STREQUAL "macos.cidmz.poulsbo.antikernel.net" )
	set(CTEST_SITE ci-macos)
	set(CTEST_BUILD_NAME arm64-macos-15-6-moltenvk)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_CMAKE_GENERATOR "Ninja")
	message(STATUS "Found known CI config: win11")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON -DBUILD_DOCS=OFF -DCMAKE_PREFIX_PATH=\"$(brew --prefix)/opt/libomp\"")

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
