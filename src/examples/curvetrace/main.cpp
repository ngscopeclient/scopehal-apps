/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
* All rights reserved.                                                                                                 *
*                                                                                                                      *
* Redistribution and use in source and binary forms, with or without modification, are permitted provided that the     *
* following conditions are met:                                                                                        *
*                                                                                                                      *
*    * Redistributions of source code must retain the above copyright notice, this list of conditions, and the         *
*      following disclaimer.                                                                                           *
*                                                                                                                      *
*    * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the       *
*      following disclaimer in the documentation and/or other materials provided with the distribution.                *
*                                                                                                                      *
*    * Neither the name of the author nor the names of any contributors may be used to endorse or promote products     *
*      derived from this software without specific prior written permission.                                           *
*                                                                                                                      *
* THIS SOFTWARE IS PROVIDED BY THE AUTHORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED   *
* TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL *
* THE AUTHORS BE HELD LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES        *
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR       *
* BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT *
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE       *
* POSSIBILITY OF SUCH DAMAGE.                                                                                          *
*                                                                                                                      *
***********************************************************************************************************************/

/**
	@file
	@author Andrew D. Zonenberg
	@brief Program entry point
 */

#include "../scopehal/scopehal.h"
#include "../scopehal/Instrument.h"
#include "../scopehal/RohdeSchwarzHMC804xPowerSupply.h"
#include "../scopehal/RohdeSchwarzHMC8012Multimeter.h"

using namespace std;

int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string spsu = "";
	string sdmm = "";

	//Test configuration
	int channel = 0;
	double voltageMax = 4.0;
	double currentMax = 0.02;
	double voltageStep = 0.001;

	//Parse command-line arguments
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		if(s == "--help")
		{
			//not implemented
			return 0;
		}
		else if(s == "--psu")
			spsu = argv[++i];
		else if(s == "--dmm")
			sdmm = argv[++i];
		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	RohdeSchwarzHMC804xPowerSupply psu(new SCPISocketTransport(spsu));
	RohdeSchwarzHMC8012Multimeter dmm(new SCPISocketTransport(sdmm));

	//Initial configuration
	LogDebug("Initial output configuration\n");
	psu.SetPowerOvercurrentShutdownEnabled(channel, false);
	psu.SetPowerVoltage(channel, 0);
	psu.SetPowerCurrent(channel, currentMax);
	psu.SetPowerChannelActive(channel, true);
	psu.SetMasterPowerEnable(true);

	if(dmm.GetMeterMode() != Multimeter::DC_CURRENT)
		dmm.SetMeterMode(Multimeter::DC_CURRENT);

	//The actual curve tracing
	LogNotice("Step,V,I\n");
	for(int i=0;i*voltageStep < voltageMax; i++)
	{
		double v = i*voltageStep;
		psu.SetPowerVoltage(channel, v);
		LogNotice("%5d,%5.3f,",i,v);

		//wait 25ms to stabilize
		std::this_thread::sleep_for(std::chrono::microseconds(25 * 1000));

		LogNotice("%5.7f\n", dmm.GetCurrent());

		if(psu.IsPowerConstantCurrent(channel))
			break;
	}

	//Clean up
	psu.SetPowerVoltage(channel, 0);
	psu.SetPowerChannelActive(channel, false);

	//Make sure all writes have committed before we close the socket
	psu.GetSerial();
	std::this_thread::sleep_for(std::chrono::microseconds(50 * 1000));

	return 0;
}
