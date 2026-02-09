# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1.1

* Core: Changed rate limiting sleep in InstrumentThread loop from 10ms to 1ms to avoid bogging down high performance instruments like the ThunderScope
* Core: Scopesession loading now uses multithreaded IO for significant performance gains especially when many channels and deep history are involved
* Drivers: Added support for many more PicoScope models
* Drivers: Added R&S RTB2000 driver (https://github.com/ngscopeclient/scopehal/pull/1048/)
* Drivers: ThunderScope now overlaps socket IO and GPU processing of waveforms giving a significant increase in WFM/s rate
* Filters: Added GPU acceleration and/or optimized many more filters (https://github.com/ngscopeclient/scopehal/issues/977) including:
  * 8B/10B (IBM) (12.1x speedup)
  * AC Couple (10x speedup)
  * Average (5.6x speedup)
  * Base (17x speedup)
  * CDR PLL (7.5x speedup)
  * Clip (4x speedup)
  * Constellation (7.5x speedup)
  * DDJ (16x speedup)
  * Downconvert (5.8x speedup)
  * Downsample (22.2x speedup with AA filter disabled, 16.3x with filter enabled)
  * Duty Cycle (8x speedup for analog input, 5x for digital)
  * Emphasis (19.5x speedup)
  * Emphasis Removal (13.2x speedup)
  * Envelope (14.5x speedup)
  * Ethernet - 100baseTX (10x speedup)
  * Exponential Moving Average (35x speedup)
  * Eye pattern (25x speedup)
  * Fall (15.8x speedup)
  * Histogram (12x speedup)
  * I/Q Demux (18.9x speedup)
  * Invert (27.3x speedup)
  * PAM Edge Detector (21.7x speedup)
  * TIE (5.3x speedup)
  * Vector Frequency (1040x speedup)
  * Vector Phase (243x speedup)
* Filters: 100baseT1 now has configurable decision thresholds for better decoding of weak signals
* Filters: CDR PLL now outputs the input signal sampled by the recovered clock in a second data stream (https://github.com/ngscopeclient/scopehal/issues/991)
* Filters: CDR PLL now has an "edges" input allowing you to replace the default thresholding edge detector with an external block, e.g. for locking to a PAM signal. This was possible in the past by passing the PAM edge detector block to the input, but this would result in the sampled data output just being a copy of the edge detector. By splitting these, the CDR can now output sampled data from PAM signals as well.
* Filters: FFT now works with arbitrary length input rather than truncating to next lowest power of two
* Filters: Peak detector for FFT etc now does quadratic interpolation for sub-sample peak fitting
* Filters: Horizontal bathtub curve now works properly with MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* Filters: PcapNG export now has an additional mode selector for use with named pipes, allowing live streaming of PcapNG formatted data to WireShark
* GUI: Added performance counters for CPU/GPU copies to better identify bottlenecks
* GUI: enabled mouseover BER measurements on MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* GUI: Filter graph editor now allows filters and instrument channels to display error messages when their configuration is invalid or something goes wrong. Not all drivers/filters take advantage of this yet.

## Breaking changes since v0.1.1

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

NOTE: This section only list changes which are potentially breaking to an *end user*. Prior to the version 1.0 release, there is no expectation of API/ABI stability and internal software interfaces may change at any time with no warning.

* Many filters no longer take the input signal and recovered clock as separate inputs. Instead, they take the new sampled output from the CDR PLL (or I/Q Demux) block. This eliminates redundant sampling and is significantly faster but was not possible to do in a fully backwards compatible fashion. The list of affected filters is:
  * 100baseTX
  * 8B/10B (IBM)
  * Constellation
  * DDJ
  * Ethernet - 100baseT1
  * Ethernet - 100baseT1 Link training
  * I/Q Demux
* The clock output of the I/Q Demux filter was removed as it was redundant.
* The FSK Decoder filter was removed as it basically did the same thing as the Threshold filter

## Bugs fixed since v0.1.1

* Core: Vulkan initialization code would break on cards that advertised 32-bit atomic float support, but did not support atomic addition (https://github.com/ngscopeclient/scopehal-apps/issues/947)
* Core: Boolean properties of filter graph blocks were serialized to scopesessions with a trailing space, which caused them to load incorrectly
* Drivers: LeCroy allowed some APIs intended for analog inputs to be called on the trigger channel as well, confusing the scope
* Drivers: LeCroy "force trigger" button did not work if the trigger wasn't already armed (https://github.com/ngscopeclient/scopehal-apps/issues/1053)
* Filters: broken CSV import with \r\n line endings (https://github.com/ngscopeclient/scopehal-apps/issues/939)
* Filters: Eye pattern mask testing would use stale mask geometry after selecting a new mask until the window was resized (https://github.com/ngscopeclient/scopehal/issues/1042)
* Filters: Fall Time measurement had numerical stability issues with deep waveforms
* Filters: PcapNG export did not handle named pipes correctly (no github ticket)
* Filters: FFT waveforms were shifted one bin to the right of the correct position and also sometimes had incorrect bin size calculation due to rounding
* Filters: Frequency and period measurement had a rounding error during integer-to-floating-point conversion causing half a cycle of the waveform to be dropped under some circumstances leading to an incorrect result, with worse error at low frequencies and short memory depths. This only affected the "summary" output not the trend plot.
* Filters: Upsample filter incorrectly calculated sample indexes on waveforms with more than 2^21 points
* GUI: Crash when closing a session (https://github.com/ngscopeclient/scopehal-apps/issues/934)
* GUI: Pressing middle mouse on the Y axis to autoscale would fail, setting the full scale range to zero volts, if the waveform was resident in GPU memory and the CPU-side copy of the buffer was stale
* GUI: History dialog allowed zero or negative values for history depth (https://github.com/ngscopeclient/scopehal-apps/issues/940)
* GUI: Eye patterns and constellations would forget the selected color ramp when moved to a new location (https://github.com/ngscopeclient/scopehal-apps/issues/556)
* Session files: Windows build could not load session files containing sample rates or memory depths in excess of 2^32

## Other changes since v0.1.1

* Core: Updated to vkFFT v1.3.4 (https://github.com/ngscopeclient/scopehal-apps/issues/866)
* GUI: General UI overhaul of stream browser to make things more intuitive and reduce the number of clicks needed to perform common tasks
* GUI: Adjusted tooltip layout code to prevent mouse cursor from blocking the first character of a tooltip
* Filters: FFT and waterfall now use uHz rather than Hz as internal frequency unit for improved resolution at the cost of not being able to represent frequencies in excess of 9.22 THz
