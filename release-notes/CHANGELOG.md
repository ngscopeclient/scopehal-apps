# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1.1

* Filters: CDR PLL is now GPU accelerated for the common case (no gating, deep waveform) and runs about 7.5x faster than before (https://github.com/ngscopeclient/scopehal/issues/977)
* Filters: CDR PLL now outputs the input signal sampled by the recovered clock in a second data stream.
* Filters: 100baseTX Ethernet is now GPU accelerated for a subset of processing and runs about 2.5x faster than before
* Filters: Eye pattern is now GPU accelerated for the common case (DDR clock on uniformly sampled input) and runs about 25x faster than before (no github ticket)
* Filters: Horizontal bathtub curve now works properly with MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* GUI: enabled mouseover BER measurements on MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.

## Breaking changes since v0.1.1

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

* Many serial protocol filters (currently 100baseTX) no longer take the input signal and recovered clock as separate inputs. Instead, they take the new sampled output from the CDR block. This eliminates redundant sampling and is significantly faster but was not possible to do in a fully backwards compatible fashion.

## Bugs fixed since v0.1.1

## Other changes since v0.1.1
