# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.2.2

* PointSample filter now accepts a scalar input from the graph, allowing the sample position to be dynamically adjusted
* Color preferences are now attached to the GUI theme, so changing theme will select a new set of color preferences (https://github.com/ngscopeclient/scopehal-apps/issues/526). If you already have an existing preferences file, you will need to reset the "appearances" preference settings to default in order to get the new light-mode colors.
* Initial Keysight Infiniium driver

## Breaking changes since v0.2.2

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

NOTE: This section only lists changes which are potentially breaking to an *end user*. Prior to the version 1.0 release, there is no expectation of API/ABI stability and internal software interfaces may change at any time with no warning.

* Changed data type of XY sweep gate signal, scalar pulse delay input and output, and scalar stairstep updated output from analog scalar to digital scalar since they are actually digital values

## Bugs fixed since v0.2.2

* Filter graph editor would steal right-click events meant for dialogs floating above it (https://github.com/ngscopeclient/scopehal-apps/issues/1031)
* Crash dragging waveform between groups (https://github.com/ngscopeclient/scopehal-apps/issues/983, https://github.com/ngscopeclient/scopehal-apps/pull/1030)
* CSV import was broken on MacOS (https://github.com/ngscopeclient/scopehal-apps/issues/1112)
* Font preferences would have invalid default values because InitializeSearchPaths() was not called before the PreferenceManager constructor, leading to an empty search path and inability to find the default font
* Flickering in stream browser when external trigger channel is present (https://github.com/ngscopeclient/scopehal-apps/pull/1026)
* Scalar Stairstep filter would fail to generate "updated" signal when trigger was stopped and restarted

## Other changes since v0.2.2

* The "agilent" oscilloscope driver is now aliased as "keysight" and can be used under either name with any supported scope

## Known issues

* Deletion of a filter by pressing "delete" with the node selected in the filter graph editor is not always possible, since not all possible consumers are tracked in the filter graph yet. If you try to delete a block and it doesn't go away, stdout and the log viewer dialog should show a message about X unresolved dangling references; you will need to find and close these windows manually. Export-to-file filters have a known reference leak and are currently impossible to delete once added to a session.
