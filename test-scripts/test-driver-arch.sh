#!/bin/sh
# do not use vulkan-swrast the test VM has a nvidia card
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
	vulkan-tools

ctest -S ctest-custom.cmake
