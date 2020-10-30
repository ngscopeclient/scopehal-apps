# scopehal-apps

Applications for libscopehal

[C++ coding policy](https://github.com/azonenberg/coding-policy/blob/master/cpp-coding-policy.md)

## Installation

### Linux
#### Install dependencies. 

On Debian/Ubuntu:

```
sudo apt-get install build-essential cmake pkg-config libglm-dev libgtkmm-3.0-dev libsigc++-2.0-dev libyaml-cpp-dev liblxi-dev texlive texlive-fonts-extra libglew-dev catch2
```

If you are using an older stable release (such as Debian Buster), you may need to install catch2 from source (https://github.com/catchorg/Catch2).  Alternatively, you may pass `-DBUILD_TESTING=OFF` to CMake to disable unit testing, which is not necessary if you are not contributing to the codebase. 

#### Install FFTS library.

```
git clone https://github.com/anthonix/ffts.git
cd ffts
mkdir build
cd build
cmake .. -DENABLE_SHARED=ON
make -j4
sudo make install
```

#### Build scopehal and scopehal-apps.

```
git clone https://github.com/azonenberg/scopehal-apps.git --recurse-submodules
cd scopehal-apps
mkdir build
cd build
cmake ..
make
```

The executable is found under `build/src/glscopeclient/glscopeclient` and the manual under `build/doc/glscopeclient-manual.pdf`

#### Install scopehal and scopehal-apps.

There is currently no process for installing `scopehal` or `scopehal-apps`. At this moment, `glscopeclient` is intended to be run from the source directory (`src/glscopeclient`). We welcome your help in setting up a proper install process! 

### Windows (experimental)

Refer to [the manual](https://www.antikernel.net/temp/glscopeclient-manual.pdf) for instructions regarding compilation on Windows systems.
