#!/bin/bash
pacman -S --needed --noconfirm \
	git wget mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-toolchain \
	mingw-w64-ucrt-x86_64-libsigc++ mingw-w64-ucrt-x86_64-yaml-cpp mingw-w64-ucrt-x86_64-glfw \
	mingw-w64-ucrt-x86_64-catch mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-libpng \
	mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader \
	mingw-w64-ucrt-x86_64-shaderc mingw-w64-ucrt-x86_64-glslang mingw-w64-ucrt-x86_64-spirv-tools \
	mingw-w64-ucrt-x86_64-fftw
source ./test-scripts/Validation.sh
ctest -S ctest-custom.cmake

# Make the CPack .msi package
cd build
cpack

# Copy the package to the output path
ARTIFACT_PATH=artifacts/
mkdir ARTIFACT_PATH
mv *.msi ARTIFACT_PATH
mv doc/*.pdf ARTIFACT_PATH
