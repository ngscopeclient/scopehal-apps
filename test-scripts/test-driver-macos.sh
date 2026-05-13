#!/bin/bash

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
	fftw \
	vulkan-validationlayers

export VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation
source ./test-scripts/Validation.sh
ctest -S ctest-custom.cmake

# Make the CPack .dmg package
cpack -G Bundle

# Copy the package to the output path
mkdir ~/artifacts
mv *.dmg ~/artifacts/
mv doc/*.pdf ~/artifacts/

# Write the hostname to the output path for debugging
hostname > ~/artifacts/builder-hostname
