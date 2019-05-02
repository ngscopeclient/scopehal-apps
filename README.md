# scopehal-cmake
CMake build wrapper for scopehal and scopehal-apps

## How to build the host software on Linux:

### Prerequisites for GNU/Linux (Debian/Ubuntu):

`sudo apt-get install build-essential cmake pkg-config libglm-dev libgtkmm-3.0-dev libsigc++-2.0-dev`

#### Build and Install FFTS (required for scopehal-cmake)
`cd ~`

`git clone https://github.com/anthonix/ffts.git`

`cd ffts`

`mkdir build`

`cd build`

`cmake ../`

`sudo make install`

### Build host software on Linux:

`cd ~`

`git clone https://github.com/azonenberg/scopehal-cmake.git --recurse-submodules`

`cd scopehal-cmake`

`cmake ./`

`make`

`sudo make install`

#### Run glscopeclient

`cd ~/scopehal-cmake/src/glscopeclient/`

Example with Lecroy Oscilloscope available on network waverunner.mynetwork.net:

`./glscopeclient --debug WR8:lecroy_vicp:waverunner.mynetwork.net`
