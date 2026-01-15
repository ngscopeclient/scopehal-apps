# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1.1

* Core: Changed rate limiting sleep in InstrumentThread loop from 10ms to 1ms to avoid bogging down high performance instruments like the ThunderScope
* Core: Scopesession loading now uses multithreaded IO for significant performance gains especially when many channels and deep history are involved
* Drivers: Added support for many more PicoScope models
* Drivers: ThunderScope now overlaps socket IO and GPU processing of waveforms giving a significant increase in WFM/s rate
* Filters: Added GPU acceleration for several filters (https://github.com/ngscopeclient/scopehal/issues/977) including:
  * AC Couple (10x speedup)
  * Average (5.6x speedup)
  * CDR PLL (7.5x speedup)
  * DDJ (16x speedup)
  * Ethernet - 100baseTX (10x speedup)
  * Eye pattern (25x speedup)
  * Histogram (12x speedup)
  * TIE (5.3x speedup)
* Filters: CDR PLL now outputs the input signal sampled by the recovered clock in a second data stream (https://github.com/ngscopeclient/scopehal/issues/991)
* Filters: FFT now works with arbitrary length input rather than truncating to next lowest power of two
* Filters: Peak detector for FFT etc now does quadratic interpolation for sub-sample peak fitting
* Filters: Horizontal bathtub curve now works properly with MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* Filters: PcapNG export now has an additional mode selector for use with named pipes, allowing live streaming of PcapNG formatted data to WireShark
* GUI: enabled mouseover BER measurements on MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* GUI: Filter graph editor now allows filters and instrument channels to display error messages when their configuration is invalid or something goes wrong. Not all drivers/filters take advantage of this yet.

## Breaking changes since v0.1.1

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

* Many filter no longer take the input signal and recovered clock as separate inputs. Instead, they take the new sampled output from the CDR block. This eliminates redundant sampling and is significantly faster but was not possible to do in a fully backwards compatible fashion. The list of affected filters is:
  * 100baseTX
  * DDJ

## Bugs fixed since v0.1.1

* Drivers: LeCroy allowed some APIs intended for analog inputs to be called on the trigger channel as well, confusing the scope
* Filters: broken CSV import with \r\n line endings (https://github.com/ngscopeclient/scopehal-apps/issues/939)
* Filters: Eye pattern mask testing would use stale mask geometry after selecting a new mask until the window was resized (https://github.com/ngscopeclient/scopehal/issues/1042)
* Filters: PcapNG export did not handle named pipes correctly (no github ticket)
* Filters: FFT waveforms were shifted one bin to the right of the correct position and also sometimes had incorrect bin size calculation due to rounding
* Filters: Frequency and period measurement had a rounding error during integer-to-floating-point conversion causing half a cycle of the waveform to be dropped under some circumstances leading to an incorrect result, with worse error at low frequencies and short memory depths. This only affected the "summary" output not the trend plot.
* GUI: Crash when closing a session (https://github.com/ngscopeclient/scopehal-apps/issues/934)
* GUI: Pressing middle mouse on the Y axis to autoscale would fail, setting the full scale range to zero volts, if the waveform was resident in GPU memory and the CPU-side copy of the buffer was stale
* GUI: History dialog allowed zero or negative values for history depth (https://github.com/ngscopeclient/scopehal-apps/issues/940)
* Session files: Windows build could not load session files containing sample rates or memory depths in excess of 2^32

## Other changes since v0.1.1

* Core: Updated to vkFFT v1.3.4 (https://github.com/ngscopeclient/scopehal-apps/issues/866)
* GUI: General UI overhaul of stream browser to make things more intuitive and reduce the number of clicks needed to perform common tasks
* GUI: Adjusted tooltip layout code to prevent mouse cursor from blocking the first character of a tooltip
* Filters: FFT and waterfall now use uHz rather than Hz as internal frequency unit for improved resolution at the cost of not being able to represent frequencies in excess of 9.22 THz
