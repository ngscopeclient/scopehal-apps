#!/bin/sh
sudo pacman -Syu --noconfirm \
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
	vulkan-swrast \
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
	glslang

ctest -S ctest-custom.cmake
