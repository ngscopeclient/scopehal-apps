# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1

* Filters: 8B/10B decode now tries more aggressively to recover comma sync after bitslipping when decoding jittery or noisy data, rather than giving decode errors for the remainder of the waveform (https://github.com/ngscopeclient/scopehal/issues/1025)
* Filters: Added basic decoding for PCIe message TLPs (hex type and body but no upper layer)
* Filters: Initial implementation of PCIe BDF decoding for destination addresses in configuration write TLPs (https://github.com/ngscopeclient/scopehal/issues/1030)
* General: Added --help and --version command line arguments (https://github.com/ngscopeclient/scopehal-apps/pull/931)

* ThunderScope: updates for SCPI / binary waveform format API changes in TS.NET
* SiniLink: Added driver for ModBus control of XYS3580 and related PSUs (https://github.com/ngscopeclient/scopehal/pull/1003)

## Bugs fixed since v0.1

* Filters: Incorrect loading of CSV files with Windows line endings (https://github.com/ngscopeclient/scopehal/issues/1002)
* Filters: PCIe link training decode got confused if the waveform started with the link in L0 then it dropped (https://github.com/ngscopeclient/scopehal/issues/1024)
* GUI: Crash in protocol analyzer dialog caused by deleting packet manager during a partial filter graph refresh (https://github.com/ngscopeclient/scopehal-apps/issues/925)
* GUI: Crash with vk::OutOfHostMemoryError when application is minimized on a Windows system with an Intel ARC GPU (https://github.com/ngscopeclient/scopehal-apps/issues/893)
* GUI: Crash with YAML::ParserException when instrument path contains a backslash (https://github.com/ngscopeclient/scopehal-apps/issues/915)
* GUI: Changing the color of a waveform in the properties dialog did not redraw in the new color until the waveform was moved or the scope triggered again (https://github.com/ngscopeclient/scopehal-apps/issues/924)
* GUI: DPI scaling issues in the filter graph editor (https://github.com/ngscopeclient/scopehal-apps/issues/868)
* GUI: Typing a new trigger position into the text box in the trigger properties dialog does not actually change the trigger position in hardware (no github ticket)
* GUI: Protocol analyzer dialogs still show the old title if a filter is renamed (https://github.com/ngscopeclient/scopehal-apps/issues/923)
* Packaging: tarballs didn't tag binaries with the hash since no .git folder was included and there was no other way to pass that info along (https://github.com/ngscopeclient/scopehal-apps/pull/910)
* Packaging: Incorrect version dependencies on Debian/Ubuntu packages (https://github.com/ngscopeclient/scopehal-apps/issues/896)
* Tests: Incorrect buffer size calculation in DeEmbedFilter unit test causing intermittent crashes of the test case in CI (no github ticket)

* Agilent: problems with DSO-X 2022A since it only has eight digital channels (https://github.com/ngscopeclient/scopehal/pull/1015)
* LeCroy: Crash with std::out_of_range when acquiring wavefrom on WaveSurfer 3000/3000Z (https://github.com/ngscopeclient/scopehal/issues/1026)
* Siglent: many crashes and malfunctions especially with SDS HD series scopes (https://github.com/ngscopeclient/scopehal/pull/1023)
* Siglent: function generator configuration refreshes too fast, causing device firmware to freeze (https://github.com/ngscopeclient/scopehal/pull/1008)
* ThunderScope: trigger position would occasionally be corrupted and get stuck at -9223 seconds (no github ticket)
* ThunderScope: when waiting for a trigger event the socket TX buffer would never be flushed causing changes to instrument settings to not take effect

## Other changes since v0.1

* GUI: Updated to latest upstream imgui (1.92.4 WIP)
* GUI: Protocol analyzer now displays marker text in the rightmost column if there is no hexdump column, and stretches the column width to leave room for text (https://github.com/ngscopeclient/scopehal-apps/issues/926) rather than defaulting to the leftmost which might be too small to read clearly
* Packaging: allow version to be embedded in tarballs rather than relying on git (https://github.com/ngscopeclient/scopehal/pull/1008)
* Tests: Unit tests now use FFTW instead of FFTS because FFTS had portability issues and a GPL dependency is fine for unit tests we don't redistribute (https://github.com/ngscopeclient/scopehal/issues/757)
