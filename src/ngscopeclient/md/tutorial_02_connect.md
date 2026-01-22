# Connect to Oscilloscope

Now it's time to actually connect to the demo scope. To do this, you'll need to:

  * Give your scope a unique nickname for display. "Demo" is a reasonable choice for the simulator.
  * Select the driver appropriate for your hardware (check the manual or website if you're not sure). The simulator driver is "demo".
  * Select the transport. This is how the driver communicates with the instrument, so the right choice will depend on whether your hardware is connected via USB, GPIB, Ethernet, etc. For the demo scope, use "null".
  * For a real scope, fill in the path so the transport knows how to talk to the instrument. This can be an IP address, COM port name, /dev/usbtmcX device, etc. as appropriate. The simulator ignores this, so leave it blank.
  * Click "Add" to add the instrument to your session.
