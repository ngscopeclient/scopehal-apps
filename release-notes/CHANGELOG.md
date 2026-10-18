# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.3

None yet

## Breaking changes since v0.3

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

NOTE: This section only lists changes which are potentially breaking to an *end user*. Prior to the version 1.0 release, there is no expectation of API/ABI stability and internal software interfaces may change at any time with no warning.

None yet

## Bugs fixed since v0.3

* Core: ensure scratch buffers are freed before shutting down Vulkan (fixes potential crash during exit)
* Core: FlowGraphNode::DetachInputs would crash if some inputs were null and others were not
* Drivers: ThunderScope now checks sequence numbers after single-shot trigger to prevent stale data being left in buffer
* Filters: PRBS decode gave occasional incorrect outputs with very short inputs
* Fixed several issues related to triggering and memory depth control in Keysight Infiniium driver
* Fixed several initialization bugs in R&S RTP driver

## Other changes since v0.3

* Added several new hardware-in-loop tests for ThunderScope + FPGA board running filters on real waveforms

## Known issues

* Deletion of a filter by pressing "delete" with the node selected in the filter graph editor is not always possible, since not all possible consumers are tracked in the filter graph yet. If you try to delete a block and it doesn't go away, stdout and the log viewer dialog should show a message about X unresolved dangling references; you will need to find and close these windows manually. Export-to-file filters have a known reference leak and are currently impossible to delete once added to a session.
