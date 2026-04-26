#!/bin/sh
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
	vulkan-headers \
	vulkan-loader-devel \
	libshaderc-devel \
	glslang-devel \
	glslc \
	spirv-tools-devel

ctest -S ctest-custom.cmake
