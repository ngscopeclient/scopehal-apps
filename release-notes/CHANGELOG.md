# ngscopeclient change log

This is a running list of significant bug fixes and new features since the last release, which will eventually be merged into release notes for the next version.

## New features since v0.2.2

* PointSample filter now accepts a scalar input from the graph, allowing the sample position to be dynamically adjusted

## Breaking changes since v0.2.2

We try to maintain compatibility with older versions of ngscopeclient but occasionally we have no choice to change the interface of a block in a way that requires old filter graphs to be updated.

NOTE: This section only lists changes which are potentially breaking to an *end user*. Prior to the version 1.0 release, there is no expectation of API/ABI stability and internal software interfaces may change at any time with no warning.

* None yet

## Bugs fixed since v0.2.2

* Font preferences would have invalid default values because InitializeSearchPaths() was not called before the PreferenceManager constructor, leading to an empty search path and inability to find the default font
* Flickering in stream browser when external trigger channel is present (https://github.com/ngscopeclient/scopehal-apps/pull/1026)

## Other changes since v0.2.2

* None yet

## Known issues

* Deletion of a filter by pressing "delete" with the node selected in the filter graph editor is not always possible, since not all possible consumers are tracked in the filter graph yet. If you try to delete a block and it doesn't go away, stdout and the log viewer dialog should show a message about X unresolved dangling references; you will need to find and close these windows manually. Export-to-file filters have a known reference leak and are currently impossible to delete once added to a session.
