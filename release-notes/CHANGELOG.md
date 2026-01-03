# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1.1

* Filters: Eye pattern is now GPU accelerated for the common case (DDR clock on uniformly sampled input) and runs about 25x faster than before (no github ticket)
* Filters: CDR PLL is now GPU accelerated for the common case (no gating, deep waveform) and runs about 7.5x faster than before (https://github.com/ngscopeclient/scopehal/issues/977)
* GUI: enabled mouseover BER measurements on MLT-3 / PAM-3 eyes as well as NRZ. PAM-4 and higher order don't work yet.

## Bugs fixed since v0.1.1

## Other changes since v0.1.1
