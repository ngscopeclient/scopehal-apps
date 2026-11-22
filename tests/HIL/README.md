# Hardware-in-loop tests

These tests are special because they require access to physical instruments to perform end-to-end testing. As a result, they are disabled by default.

## Hardware setup

Required equipment:

* ThunderScope (either PCIe or Thunderbolt model is acceptable)
* Probe demo board (https://github.com/azonenberg/electronics-training/tree/master/oscilloscope-probing/lab/boards/demo-board)
* Host PCs for ThunderScope and demo board. May be the same machine you are running tests on, or elsewhere on a LAN. If using network remoting, connection from TS to test machine should be >= 10G Ethernet for best performance.

Cable connections (WILL CHANGE when relay board comes in)
* Thunderbolt/PCIe from TS to host PC
* USB-C from host PC to demo board USB
* 4x SMA-BNC from TS input channels 1-4 to right side SMA out 1-4 of demo board

## Enabling HIL tests

To enable these tests, run CMake passing the following (in addition to the normal arguments to enable testing and configure build type etc):

* `-DENABLE_HIL_TESTS=ON` (enables HIL tests)
* `-DHIL_SIGGEN_PATH=host:port` (host/port of signal generator board UART, typically bridged via socat - direct /dev/ttyUSB* not currently supported)
* `-DHIL_TS_PATH=host:port1:port` (host/port of ThunderScope TS.NET server)
