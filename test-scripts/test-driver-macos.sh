#!/bin/sh

# why is this needed when $PATH should be correct in the runner VM already
export PATH=/opt/homebrew/bin:/opt/homebrew:sbin:$PATH

#this was in github actions scripts because they had a broken version of cmake preinstalled
#i think it's safe to skip?
#brew uninstall cmake

brew install \
	pkg-config \
	libsigc++@2 \
	glfw \
	cmake \
	yaml-cpp \
	catch2 \
	libomp \
	vulkan-headers \
	vulkan-loader \
	spirv-tools \
	glslang \
	shaderc \
	molten-vk \
	ninja \
	hidapi \
	fftw

ctest -S ctest-custom.cmake
