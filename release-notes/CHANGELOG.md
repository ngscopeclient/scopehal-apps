# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.1.1

* Core: added official support and binary release for Debian aarch64
* Core: Changed rate limiting sleep in InstrumentThread loop from 10ms to 1ms to avoid bogging down high performance instruments like the ThunderScope
* Core: Scopesession loading now uses multithreaded IO for significant performance gains especially when many channels and deep history are involved
* Core: Significant rewrite of Vulkan queue allocation logic to reduce unnecessary locking and mutex contention on GPUs without a lot of Vulkan queues (most non-NVIDIA platforms)
* Core: Restructured int8 to float32 conversion to enable GPU acceleration on all Vulkan-capable GPUs without needing CPU fallback, as well as significantly improve performance on already-supported platforms. Not yet deployed to all drivers (https://github.com/ngscopeclient/scopehal/issues/1083)
* Core: Allow instruments (currently only demo scope and ThunderScope implemented) to declare that they support high-rate updates to channel DC offset, allowing for more responsive updates while dragging the axis
* Core: Replaced filter graph input validation flow so that nodes can declare what kinds of input they want in a human-readable format, making it easier to understand why an attempted connection wasn't allowed
* Core: Added new "sparsev2" serialization format for improved speed loading and saving sparse waveforms. Only used for 8B/10B so far, other waveform types will switch in the future.
* Drivers: Added support for many more PicoScope models
* Drivers: Added Agilent 34401A 6.5 digit DMM driver (https://github.com/ngscopeclient/scopehal/pull/1076/)
* Drivers: Added R&S RTB2000 driver (https://github.com/ngscopeclient/scopehal/pull/1048/)
* Drivers: Added Rigol MHO900/98 series (https://github.com/ngscopeclient/scopehal/pull/1085)
* Drivers: Added Teledyne LeCroy SDA6000A support (https://github.com/ngscopeclient/scopehal/issues/1065)
* Drivers: ThunderScope now overlaps socket IO and GPU processing of waveforms giving a significant increase in WFM/s rate
* Drivers: Demo scope now uses xorshift32 instead of glibc LCG for better statistical properties on the simulated noise
* Drivers: Added Antikernel Labs GPIO driver (FPGA debug IP)
* Drivers: Added Antikernel Labs 8b10b SERDES ILA driver (FPGA debug IP)
* Drivers: Added Antikernel Labs ILA driver (FPGA debug IP)
* Drivers: Added Antikernel Labs VIO driver (FPGA debug IP)
* Filters: Added GPU acceleration and/or optimized many more filters (https://github.com/ngscopeclient/scopehal/issues/977) including but not limited to the list below. Typical performance improvements (RTX 2080 Ti vs Xeon 6144):
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
  * Ethernet - 100baseT1 (35x speedup)
  * Ethernet - 100baseTX (10x speedup)
  * Exponential Moving Average (35x speedup)
  * Eye pattern (25x speedup)
  * Fall (15.8x speedup)
  * Histogram (12x speedup)
  * I/Q Demux (18.9x speedup)
  * Invert (27.3x speedup)
  * Maximum (35x speedup)
  * Minimum (24.8x speedup)
  * Multiple (44.2x speedup for vector x scalar, 75x for vector x vector)
  * Moving Average (93.5x speedup)
  * Noise (50x speedup)
  * PAM Edge Detector (21.7x speedup)
  * PRBS generator (21.6x speedup)
  * PRBS checker (211x speedup)
  * Sine (286x speedup)
  * TIE (5.3x speedup)
  * Vector Frequency (1040x speedup)
  * Vector Magnitude (73x speedup)
  * Vector Phase (243x speedup)
* Filters: Added AMBA APB protocol decoder
* Filters: Added digital-to-analog filter to convert a digital scalar value (e.g. from a VIO) to an analog value
* Filters: Added Ethernet clause 73 (base-KX copper backplane) autonegotiation decoder (https://github.com/ngscopeclient/scopehal/pull/1074)
* Filters: Invert now works on digital signals as well as analog (https://github.com/ngscopeclient/scopehal/issues/1088)
* Filters: 100baseT1 now has configurable decision thresholds for better decoding of weak signals
* Filters: CDR PLL now outputs the input signal sampled by the recovered clock in a second data stream (https://github.com/ngscopeclient/scopehal/issues/991)
* Filters: CDR PLL now has an "edges" input allowing you to replace the default thresholding edge detector with an external block, e.g. for locking to a PAM signal. This was possible in the past by passing the PAM edge detector block to the input, but this would result in the sampled data output just being a copy of the edge detector. By splitting these, the CDR can now output sampled data from PAM signals as well.
* Filters: FFT now works with arbitrary length input rather than truncating to next lowest power of two
* Filters: Improved floating point numerical stability in many blocks when working with deep memory
* Filters: Peak detector for FFT etc now does quadratic interpolation for sub-sample peak fitting
* Filters: Horizontal bathtub curve now works properly with MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* Filters: PcapNG export now has an additional mode selector for use with named pipes, allowing live streaming of PcapNG formatted data to WireShark
* GUI: Filter palette now uses case insensitive sorting rather than putting all capital letters before lowercase
* GUI: Added performance counters for CPU/GPU copies to better identify bottlenecks
* GUI: enabled mouseover BER measurements on MLT-3 / PAM-3 eyes as well as NRZ. No PAM-4 or higher support yet.
* GUI: Filter graph editor now allows filters and instrument channels to display error messages when their configuration is invalid or something goes wrong. Not all drivers/filters take advantage of this yet.
* GUI: "add instrument" dialog now includes automatic enumeration of attached HID and UART devices (https://github.com/ngscopeclient/scopehal-apps/pull/968)
* GUI: lots of improvements to drag-and-drop of channels between plots (https://github.com/ngscopeclient/scopehal-apps/pull/972)
* GUI: Added new NGSCOPECLIENT_UI_SCALE and NGSCOPECLIENT_FONT_SCALE environment variables to override automatic detection of DPI scaling (https://github.com/ngscopeclient/scopehal-apps/issues/953)
* GUI: Filter graph editor now allows deletion of filter blocks by selecting a block and hitting "delete"
* GUI: added performance counter for per-scope waveform rate to better profile multi-scope sessions
* Serialization: trace alpha and persistence decay settings are now saved in session files (https://github.com/ngscopeclient/scopehal-apps/issues/936)
* GUI: Properties dialogs don't auto-spawn when filters (other than import filters) are created, to avoid unnecessary clutter
* GUI: Removed hard-to-find persistence settings dialog and just made persistence a slider on the toolbar

## Breaking changes since v0.1.1

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

NOTE: This section only list changes which are potentially breaking to an *end user*. Prior to the version 1.0 release, there is no expectation of API/ABI stability and internal software interfaces may change at any time with no warning.

* Many filters no longer take the input signal and recovered clock as separate inputs. Instead, they take the new sampled output from the CDR PLL (or I/Q Demux) block. This eliminates redundant sampling and is significantly faster but was not possible to do in a fully backwards compatible fashion. The list of affected filters is:
  * 100baseTX
  * 128b/130b
  * 64b/66b
  * 8b/10b (IBM)
  * 8b/10b (TMDS)
  * Constellation
  * DDJ
  * Ethernet - 100baseT1
  * Ethernet - 100baseT1 Link training
  * I/Q Demux
  * PRBS Checker
* The clock output of the I/Q Demux filter was removed as it was redundant.
* The FSK Decoder filter was removed as it basically did the same thing as the Threshold filter

## Bugs fixed since v0.1.1

* Core: Vulkan initialization code would break on cards that advertised 32-bit atomic float support, but did not support atomic addition (https://github.com/ngscopeclient/scopehal-apps/issues/947)
* Core: missing mutex lock causing threading issues on GPUs with less Vulkan queues than active threads (common on AMD cards)
* Core: Boolean properties of filter graph blocks were serialized to scopesessions with a trailing space, which caused them to load incorrectly
* Core: Fixed some numerical precision errors in conversion of 64-bit integer values to strings and back (https://github.com/ngscopeclient/scopehal/issues/1073)
* Core: Fixed hexadecimal values in Y axis sometimes being truncated
* Drivers: LeCroy allowed some APIs intended for analog inputs to be called on the trigger channel as well, confusing the scope
* Drivers: LeCroy "force trigger" button did not work if the trigger wasn't already armed (https://github.com/ngscopeclient/scopehal-apps/issues/1053)
* Filters: broken CSV import with \r\n line endings (https://github.com/ngscopeclient/scopehal-apps/issues/939)
* Filters: CSV import now uses 64-bit internal precision when parsing timestamps, reducing loss of precision when loading multimillion line files
* Filters: Eye pattern mask testing would use stale mask geometry after selecting a new mask until the window was resized (https://github.com/ngscopeclient/scopehal/issues/1042)
* Filters: Fall Time measurement had numerical stability issues with deep waveforms
* Filters: I/Q demux would sometimes align incorrectly when decoding 100baseT1
* Filters: PcapNG export did not handle named pipes correctly (no github ticket)
* Filters: FFT waveforms were shifted one bin to the right of the correct position and also sometimes had incorrect bin size calculation due to rounding
* Filters: Frequency and period measurement had a rounding error during integer-to-floating-point conversion causing half a cycle of the waveform to be dropped under some circumstances leading to an incorrect result, with worse error at low frequencies and short memory depths. This only affected the "summary" output not the trend plot.
* Filters: Upsample filter incorrectly calculated sample indexes on waveforms with more than 2^21 points
* GUI: Opening a session for offline analysis would add the instruments in it to your recent list (https://github.com/ngscopeclient/scopehal-apps/issues/1005)
* GUI: Crash when closing a session (https://github.com/ngscopeclient/scopehal-apps/issues/934)
* GUI: Pressing middle mouse on the Y axis to autoscale would fail, setting the full scale range to zero volts, if the waveform was resident in GPU memory and the CPU-side copy of the buffer was stale
* GUI: History dialog allowed zero or negative values for history depth (https://github.com/ngscopeclient/scopehal-apps/issues/940)
* GUI: Eye patterns and constellations would forget the selected color ramp when moved to a new location (https://github.com/ngscopeclient/scopehal-apps/issues/556)
* GUI: crash when autoscaling an empty waveform (https://github.com/ngscopeclient/scopehal-apps/issues/978)
* GUI: newly added measurement filters would not refresh until the next trigger (https://github.com/ngscopeclient/scopehal-apps/issues/997)
* Session files: Windows build could not load session files containing sample rates or memory depths in excess of 2^32

## Other changes since v0.1.1

* Core: Updated to vkFFT v1.3.4 (https://github.com/ngscopeclient/scopehal-apps/issues/866)
* Core: Updated to Dear Imgui 1.92.8-docking (from 1.92.5-docking). NOTE: ID hashing changes made in 1.92.6 may break some window layouts in saved .scopesessions.
* Core: Updated to latest upstream imgui_markdown
* GUI: General UI overhaul of stream browser to make things more intuitive and reduce the number of clicks needed to perform common tasks
* GUI: Adjusted tooltip layout code to prevent mouse cursor from blocking the first character of a tooltip
* Filters: FFT and waterfall now use uHz rather than Hz as internal frequency unit for improved resolution at the cost of not being able to represent frequencies in excess of 9.22 THz

## Known issues

* Deletion of a filter by pressing "delete" with the node selected in the filter graph editor is not always possible, since not all possible consumers are tracked in the filter graph yet. If you try to delete a block and it doesn't go away, stdout and the log viewer dialog should show a message about X unresolved dangling references; you will need to find and close these windows manually. Export-to-file filters have a known reference leak and are currently impossible to delete once added to a session.
