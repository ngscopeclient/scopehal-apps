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
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_ERRORS 999)
set(CTEST_CUSTOM_MAXIMUM_NUMBER_OF_WARNINGS 999)

# Ignore this warning generated during static analysis
list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION "linker input file unused because linking not done")

set(STATIC_ANALYSIS 0)

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
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=ON" "-DCPACK_GENERATOR=TGZ")

# CI Debian Trixie
elseif(${HOSTNAME} STREQUAL "debian-stable" )
	set(CTEST_SITE ci-debian-stable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-13-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-stable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=ON" "-DCPACK_GENERATOR=DEB")

# CI Debian Trixie aarch64
elseif(${HOSTNAME} STREQUAL "debian-stable-aarch64" )
	set(CTEST_SITE ci-debian-stable-aarch64)
	set(CTEST_BUILD_NAME aarch64-linux-debian-13-llvmpipe)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-stable-aarch64")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=ON" "-DCPACK_GENERATOR=DEB")

# CI Debian Bookworm
elseif(${HOSTNAME} STREQUAL "debian-oldstable" )
	set(CTEST_SITE ci-debian-oldstable)
	set(CTEST_BUILD_NAME x86_64-linux-debian-12-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: debian-oldstable")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=ON" "-DCPACK_GENERATOR=DEB")

# CI Ubuntu Noble
elseif(${HOSTNAME} STREQUAL "ubuntu-oldlts" )
	set(CTEST_SITE ci-ubuntu-oldlts)
	set(CTEST_BUILD_NAME x86_64-linux-ubuntu-24-04-nvidia)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: ubuntu-oldlts")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=ON" "-DCPACK_GENERATOR=DEB")

# CI Ubuntu Resolute
elseif(${HOSTNAME} MATCHES "ubuntu-lts" )
	set(CTEST_SITE ci-ubuntu-lts)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: ubuntu-lts")

	if($ENV{SANITIZE})
		set(CTEST_BUILD_NAME x86_64-linux-ubuntu-26-04-llvmpipe-asan)
		message(STATUS "Building debug version with sanitizers")
		set(CTEST_BUILD_CONFIGURATION "DebugNoOpt")
		set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DSANITIZE=ON")
	elseif($ENV{ANALYZE})
		set(CTEST_BUILD_NAME x86_64-linux-ubuntu-26-04-llvmpipe-analysis)
		message(STATUS "Building for static analysis")

		set(STATIC_ANALYSIS 1)

		# scan-build exclude doesn't seem to work under cmake so just filter out the warnings in the library files
		list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION "src/imgui/")
		list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION "src/imgui-node-editor/")
		list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION "src/ImGuiFileDialog/")
		list(APPEND CTEST_CUSTOM_WARNING_EXCEPTION "src/nativefiledialog-extended/")

		set(CTEST_BUILD_CONFIGURATION "DebugNoOpt")
		set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DANALYZE=ON")
	else()
		set(CTEST_BUILD_NAME x86_64-linux-ubuntu-26-04-llvmpipe)
		message(STATUS "Building optimized release version")
		set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")

		# extractbb seems to not be found even though we installed texlive-binaries, so for now turn it off
		# to avoid build problems
		set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=OFF" "-DCPACK_GENERATOR=DEB")
	endif()

# CI Fedora 43
elseif(${HOSTNAME} STREQUAL "fedora" )
	set(CTEST_SITE ci-fedora)
	set(CTEST_BUILD_NAME x86_64-linux-fedora-43-llvmpipe)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_GIT_COMMAND "/usr/bin/git")
	message(STATUS "Found known CI config: fedora")

	# texlive is crashing with a buffer overflow so for now turn doc build off
	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DBUILD_DOCS=OFF" "-DCPACK_GENERATOR=RPM")

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
	set(CONFIGURE_OPTIONS "-DBUILD_TESTING=ON" "-DWIXPATH=C:\\PROGRA~2\\WIXTOO~1.14\\bin" "-DCPACK_GENERATOR=WIX") # don't build docs on windows for now, TODO figure that out

# CI MacOS ARM64, for whatever reason it reports the FQDN not just the hostname
elseif(${HOSTNAME} STREQUAL "macos.cidmz.poulsbo.antikernel.net" )
	set(CTEST_SITE ci-macos)
	set(CTEST_BUILD_NAME aarch64-macos-15-6-moltenvk)
	set(CTEST_DASHBOARD Continuous)
	set(CTEST_CMAKE_GENERATOR "Ninja")
	set(CTEST_BINARY_DIRECTORY "/Users/ci/Documents/scopehal-apps/build")
	set(CTEST_SOURCE_DIRECTORY "/Users/ci/Documents/scopehal-apps/")
	message(STATUS "Found known CI config: macos")

	set(CTEST_BUILD_CONFIGURATION "RelWithDebInfo")
	set(CONFIGURE_OPTIONS
		"-DDISABLE_PCH=ON"
		"-DBUILD_TESTING=ON"
		"-DBUILD_DOCS=OFF"
		"-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/libomp")

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

#Don't run unit tests if doing static analysis
if(NOT ${STATIC_ANALYSIS})
	ctest_test()
endif()

#ctest_coverage()
set(CTEST_DROP_METHOD https)
set(CTEST_DROP_SITE dashboard.ngscopeclient.org)
set(CTEST_DROP_LOCATION /submit.php?project=ngscopeclient)
ctest_submit()
