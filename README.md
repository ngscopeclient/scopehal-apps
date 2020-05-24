# scopehal-cmake
CMake build wrapper for scopehal and scopehal-apps

## Installation
Install dependencies on ubuntu with
```
sudo apt-get install build-essential cmake pkg-config libglm-dev libgtkmm-3.0-dev libsigc++-2.0-dev libyaml-cpp-dev
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
