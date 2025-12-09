# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1

* ThunderScope: updates for SCPI / binary waveform format API changes in TS.NET
* SiniLink: Added driver for ModBus control of XYS3580 and related PSUs (https://github.com/ngscopeclient/scopehal/pull/1003)

## Bugs fixed since v0.1

* Fixed crash in protocol analyzer dialog caused by deleting packet manager during a partial filter graph refresh (https://github.com/ngscopeclient/scopehal-apps/issues/925)
* (partial) Incorrect version dependencies on Debian/Ubuntu packages (https://github.com/ngscopeclient/scopehal-apps/issues/896)
* (partial) DPI scaling issues in the filter graph editor (https://github.com/ngscopeclient/scopehal-apps/issues/868)
* Crash with vk::OutOfHostMemoryError when application is minimized on a Windows system with an Intel ARC GPU (https://github.com/ngscopeclient/scopehal-apps/issues/893)
* Incorrect loading of CSV files with Windows line endings (https://github.com/ngscopeclient/scopehal/issues/1002)
* Incorrect buffer size calculation in DeEmbedFilter unit test causing intermittent crashes of the test case in CI (no github ticket)
* ThunderScope: trigger position would occasionally be corrupted and get stuck at -9223 seconds (no github ticket)
* Typing a new trigger position into the text box in the trigger properties dialog does not actually change the trigger position in hardware (no github ticket)

## Other changes since v0.1

* Updated to latest upstream imgui (1.92.4 WIP)
* Unit tests now use FFTW instead of FFTS because FFTS had portability issues and a GPL dependency is fine for unit tests we don't redistribute (https://github.com/ngscopeclient/scopehal/issues/757)
