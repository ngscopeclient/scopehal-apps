# ngscopeclient and scopehal-apps

This is the top level repository for ngscopeclient, as well as the unit tests for libscopehal.

Project website: [https://www.ngscopeclient.org](https://www.ngscopeclient.org)

## Policies

* [C++ coding policy](https://github.com/azonenberg/coding-policy/blob/master/cpp-coding-policy.md)
* [Code of Conduct](https://github.com/ngscopeclient/scopehal-apps/blob/master/CODE_OF_CONDUCT.md)
* We are a proudly AI-free project. Only human created code is acceptable for contribution.

## Installation 

Refer to the "getting started" chapter of the User manual
* [User manual GettingStarted (HTML)](https://www.ngscopeclient.org/manual/GettingStarted.html)
* [User manual (PDF)](https://www.ngscopeclient.org/downloads/ngscopeclient-manual.pdf)

## Compilation instructions (Linux,macOS,Windows)

* [User manual Compilation (HTML)](https://www.ngscopeclient.org/manual/GettingStarted.html#compilation)

## Compiling a forked repo

If you want to contribute changes to scopehal-apps, you should make them in a
forked repo so that you can create pull request with your changes.

Following these steps:

* Fork a bunch of GitHub repos to your own GitHub account

  Right now, the build system requires that your GitHub account has cloned version of the
  VkFFT, xptools and logtools. It's best to clone the version that under the ngscopeclient
  account, this ensure that you're working with the same version as the core development
  team.

  * [ngscopeclient/VkFFT](https://github.com/ngscopeclient/VkFFT)
  * [ngscopeclient/xptools](https://github.com/ngscopeclient/xptools)
  * [ngscopeclient/logtools](https://github.com/ngscopeclient/logtools)

* Fork the `scopehal-apps` repo (this one!)  to your own account
* Clone your personal repo to your development machine

  `git clone --recursive git@github.com:<your github username>/scopehal-apps.git scopehal-apps`

* Follow the regular compilation instructions.

  If you want to create executables with debug symbols, make sure to change option `MAKE_BUILD_TYPE`
  from `Release` to `Debug` when running `cmake`.

It is possible that `cmake` errors out with the following error message:

```
CMake Error at CMakeLists.txt:82 (message):
  Unrecognized version tag 53152c7c / 53152c7c, can't create a VERSIONINFO
  from it.  Must be of format v1.2, v1.2.3, v1.2-rc3, v1.2.3-rc4
```

You can fix this by creating a dummy tag in your local repo:

`git tag v0.0.0`

## Special comments

The following standard comments are used throughout the code to indicate things that could use attention, but are
not worthy of being tracked as a GitHub issue yet.

* `//TODO`: unimplemented feature, potential optimization point, etc.
* `//FIXME`: known minor problem, temporary workaround, or something that needs to be reworked later
* `//FIXME-CXX20`: places where use of C++ 20 features would simplify the code, but nothing can be done as long as we are targeting platforms which only support C++ 17
