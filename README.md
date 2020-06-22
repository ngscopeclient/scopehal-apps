# scopehal-cmake
CMake build wrapper for scopehal and scopehal-apps

[C++ coding policy](https://github.com/azonenberg/coding-policy/blob/master/cpp-coding-policy.md)

## Installation

### Linux
Install dependencies on ubuntu with
```
sudo apt-get install build-essential cmake pkg-config libglm-dev libgtkmm-3.0-dev libsigc++-2.0-dev libyaml-cpp-dev liblxi-dev texlive texlive-fonts-extra
```

Install FFTS library
```
git clone https://github.com/anthonix/ffts.git
cd ffts
mkdir build
cd build
cmake ../
make
sudo make install
```

Build scopehal and scopehal-apps
```
git clone https://github.com/azonenberg/scopehal-cmake.git --recurse-submodules
cd scopehal-cmake
mkdir build
cd build
cmake ../
make
```

The executable is found unter `build/src/glscopeclient/glscopeclient` and the manual under `build/doc/glscopeclient-manual.pdf`

### Windows (experimental)

Refer to [the manual](https://www.antikernel.net/temp/glscopeclient-manual.pdf) for instructions regarding compilation on Windows systems.
