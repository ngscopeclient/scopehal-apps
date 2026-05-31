#!/bin/bash
# Install dependencies
sudo apt -y update
sudo apt -y full-upgrade
sudo apt -y install build-essential git cmake pkgconf libgtk-3-dev libsigc++-2.0-dev libyaml-cpp-dev catch2 libglfw3-dev curl xz-utils libhidapi-dev lsb-release dpkg-dev file libvulkan-dev glslang-dev glslang-tools spirv-tools glslc liblxi-dev libtirpc-dev texlive texlive-binaries texlive-fonts-extra texlive-extra-utils libfftw3-dev vulkan-validationlayers

# Enable Vulkan validation layers
export VK_LOADER_LAYERS_ENABLE=VK_LAYER_KHRONOS_validation
source ./test-scripts/Validation.sh

# Build and run tests
ctest -S ctest-custom.cmake

# Make the CPack .deb package
cd build
make package

# Copy the package to the output path
mkdir ~/artifacts
mv *.deb ~/artifacts/
mv doc/*.pdf ~/artifacts/
