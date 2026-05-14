#!/bin/bash
sudo dnf upgrade -y
sudo dnf install -y \
	git \
	ccache \
	gcc \
	g++ \
	cmake \
	make \
	pkgconf \
	cairomm-devel \
	gtk3-devel \
	libsigc++30-devel \
	yaml-cpp-devel \
	catch-devel \
	glfw-devel \
	hidapi-devel \
	ninja-build \
	fedora-packager \
	rpmdevtools \
	mesa-vulkan-drivers \
	lsb-release \
	fftw3-devel \
	texlive-dvipng \
	texlive \
	texlive-tex4ht \
	texlive-makecell \
	texlive-tocloft \
	texlive-inconsolata \
	texlive-gensymb \
	texlive-newtx \
	texlive-upquote\
	texlive-make4ht \
	texlive-luaxml \
	vulkan-headers \
	vulkan-loader-devel \
	libshaderc-devel \
	glslang-devel \
	glslc \
	spirv-tools-devel \
	vulkan-validation-layers

export VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation
source ./test-scripts/Validation.sh
ctest -S ctest-custom.cmake

# Make the CPack .rpm package
cd build
make package

# Copy the package to the output path
mkdir ~/artifacts
mv *.rpm ~/artifacts/
mv doc/*.pdf ~/artifacts/
