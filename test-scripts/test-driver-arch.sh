#!/bin/bash
# Install dependencies
# do not use vulkan-swrast like github actions script did - the test VM has a nvidia card
sudo pacman -Syu --noconfirm --needed \
	git \
	gcc \
	cmake \
	make \
	pkgconf \
	gtk3 \
	libsigc++ \
	yaml-cpp \
	catch2 \
	glfw \
	curl \
	hidapi \
	ccache \
	ninja \
	lsb-release \
	fftw \
	texlive-bin \
	texlive-binextra \
	texlive-luatex \
	texlive-plaingeneric \
	texlive-latex \
	texlive-latexrecommended \
	texlive-latexextra \
	texlive-fontsextra \
	spirv-headers \
	vulkan-headers \
	vulkan-icd-loader \
	shaderc \
	glslang \
	vulkan-validation-layers \
	vulkan-tools

# Enable Vulkan validation layers
export VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation
source ./test-scripts/Validation.sh

# Build and run tests
ctest -S ctest-custom.cmake

# Make the CPack .tgz package
cd build
make package

# Copy the package to the output path
mkdir ~/artifacts
mv *.tgz ~/artifacts/
mv doc/*.pdf ~/artifacts/

# Write the hostname to the output path for debugging
hostname > ~/artifacts/builder-hostname
