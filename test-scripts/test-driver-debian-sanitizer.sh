#!/bin/bash
sudo apt -y update
sudo apt -y full-upgrade
sudo apt -y install build-essential git cmake pkgconf libgtk-3-dev libsigc++-2.0-dev libyaml-cpp-dev catch2 libglfw3-dev curl xz-utils libhidapi-dev lsb-release dpkg-dev file libvulkan-dev glslang-dev glslang-tools spirv-tools glslc liblxi-dev libtirpc-dev texlive texlive-binaries texlive-fonts-extra texlive-extra-utils libfftw3-dev vulkan-validationlayers

export VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation
source ./test-scripts/Validation.sh
export SANITIZE=1
ctest -S ctest-custom.cmake
