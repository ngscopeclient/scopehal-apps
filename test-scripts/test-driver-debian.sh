#!/bin/sh
sudo apt install build-essential git cmake pkgconf libgtk-3-dev libsigc++-2.0-dev libyaml-cpp-dev catch2 libglfw3-dev curl xzip libhidapi-dev lsb-release dpkg-dev file libvulkan-dev glslang-dev glslang-tools spirv-tools glslc liblxi-dev libtirpc-dev texlive texlive-fonts-extra texlive-extra-utils
rm -rf build
mkdir build
ctest -S ctest-custom.cmake
