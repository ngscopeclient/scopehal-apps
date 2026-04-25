#!/bin/sh
pacman -S --needed git wget mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-toolchain
pacman -S --needed mingw-w64-ucrt-x86_64-libsigc++ mingw-w64-ucrt-x86_64-yaml-cpp mingw-w64-ucrt-x86_64-glfw mingw-w64-ucrt-x86_64-catch mingw-w64-ucrt-x86_64-hidapi mingw-w64-ucrt-x86_64-libpng
pacman -S --needed mingw-w64-ucrt-x86_64-vulkan-headers mingw-w64-ucrt-x86_64-vulkan-loader mingw-w64-ucrt-x86_64-shaderc mingw-w64-ucrt-x86_64-glslang mingw-w64-ucrt-x86_64-spirv-tools
pacman -S --needed mingw-w64-ucrt-x86_64-fftw
ctest -S ctest-custom.cmake
