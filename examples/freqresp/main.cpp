/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
*                                                                                                                      *
* Copyright (c) 2012-2019 Andrew D. Zonenberg                                                                          *
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
#include "../scopehal/LeCroyVICPOscilloscope.h"
#include "../scopeprotocols/scopeprotocols.h"
#include "../scopemeasurements/scopemeasurements.h"

using namespace std;

int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string sfgen = "";
	string sscope = "";

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
		else if(s == "--fgen")
			sfgen = argv[++i];
		else if(s == "--scope")
			sscope = argv[++i];
		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Connect to the scopes and sanity check
	LeCroyVICPOscilloscope fgen(sfgen, 1861);
	LeCroyVICPOscilloscope scope(sscope, 1861);
	if(0 == (fgen.GetInstrumentTypes() & Instrument::INST_FUNCTION))
	{
		LogError("not a function generator\n");
		return 1;
	}
	if(0 == (scope.GetInstrumentTypes() & Instrument::INST_OSCILLOSCOPE))
	{
		LogError("not an oscilloscope\n");
		return 1;
	}

	//Set up the scope
	scope.DisableChannel(0);
	scope.DisableChannel(1);
	scope.EnableChannel(2);	//reference signal
	scope.EnableChannel(3);	//signal through probe

	//Measure the input frequency
	FrequencyMeasurement freq_meas;
	freq_meas.SetInput(0, scope.GetChannel(2));
	PkPkVoltageMeasurement pp_ref;
	pp_ref.SetInput(0, scope.GetChannel(2));
	PkPkVoltageMeasurement pp_probe;
	pp_probe.SetInput(0, scope.GetChannel(3));

	//Main loop
	//fgen.SetFunctionChannelActive(0, true);
	LogNotice("freq_mhz,ref_mv,probe_mv,gain_db\n");
	//for(float mhz = 0.5; mhz <= 10; mhz += 0.01)
	while(true)
	{
		//Configure the function generator and wait a little while (there's some lag on the output)
		//fgen.SetFunctionChannelFrequency(0, mhz * 1.0e6f);
		usleep(1000 * 50);

		float actual_mhz 	= 0;
		float amp_ref 		= 0;
		float amp_probe		= 0;
		float gain_db		= 0;
		float navg = 5;
		for(int j=0; j<navg; j++)
		{
			//Acquire a waveform (TODO average a few?)
			scope.StartSingleTrigger();
			bool triggered = false;
			for(int i=0; i<50; i++)
			{
				if(scope.PollTrigger() == Oscilloscope::TRIGGER_MODE_TRIGGERED)
				{
					triggered = true;
					break;
				}
				usleep(10 * 1000);
			}
			if(!triggered)
			{
				//LogError("Scope never triggered\n");
				continue;
			}
			if(!scope.AcquireData())
			{
				LogError("Couldn't acquire data\n");
				break;
			}

			//Update the measurements
			freq_meas.Refresh();
			pp_ref.Refresh();
			pp_probe.Refresh();

			//Done
			actual_mhz += freq_meas.GetValue() * 1e-6f;
			amp_ref += pp_ref.GetValue();
			amp_probe += pp_probe.GetValue();
			gain_db += 20 * log10(amp_probe / amp_ref);
		}

		LogNotice("%f,%f,%f,%f\n",
			actual_mhz / navg,
			amp_ref * 1000 / navg,
			amp_probe * 1000 / navg,
			gain_db / navg);
		fflush(stdout);
	}

	return 0;
}
