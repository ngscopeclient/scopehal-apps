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

using namespace std;

int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	string hostname;
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
		else if(s == "--host")
			hostname = argv[++i];
		else
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Load the IBIS file
	IBISParser ibis;
	ibis.Load("/nfs4/share/datasheets/Xilinx/7_series/kintex-7/kintex7.ibs");
	/*LogDebug("IBIS file loaded! Component is %s by %s\n",
		ibis.m_component.c_str(),
		ibis.m_manufacturer.c_str());*/

	//Look up one particular I/O standard
	auto model = ibis.m_models["LVDS_HP_O"];
	if(!model)
		LogFatal("couldn't find model\n");

	//Look up some properties of this buffer
	float vcc = model->m_voltages[CORNER_TYP];
	auto& pulldown = model->m_pulldown[CORNER_TYP];
	auto& pullup = model->m_pullup[CORNER_TYP];
	float cap = model->m_dieCapacitance[CORNER_TYP];

	//Find the rising and falling edge waveform terminated to the lowest voltage (ground or so)
	VTCurves* rising = model->GetHighestRisingWaveform();
	VTCurves* falling = model->GetHighestFallingWaveform();

	const float dt = 5e-12;

	//PRBS-31 generator
	uint32_t prbs = 0x5eadbeef;

	//Play rising/falling waveforms
	size_t ui_ticks = 160;	//1.25 Gbps
	size_t last_ui_start	= 0;
	size_t ui_start			= 0;
	LogDebug("ns, v, current_v, last_v, delta, current_edge_started, last_ui_start\n");
	bool current_bit		= false;
	bool last_bit			= false;
	float	current_v_old	= 0;
	bool current_edge_started	= false;
	for(size_t nstep=0; nstep<10000; nstep ++)
	{
		float time = dt*nstep;

		//Advance to next UI
		if(0 == (nstep % ui_ticks))
		{
			last_bit		= current_bit;

			if(nstep != 0)
			{
				//PRBS-31 generator
				uint32_t next = ( (prbs >> 31) ^ (prbs >> 28) ) & 1;
				prbs = (prbs << 1) | next;
				current_bit = next ? true : false;

				//Keep the old edge going
				current_edge_started = false;
			}

			ui_start		= nstep;
		}

		//Get phase of current and previous UI
		size_t	current_phase	= nstep - ui_start;
		size_t	last_phase		= nstep - last_ui_start;

		//Get value for current and previous edge
		float current_v;
		if(current_bit)
			current_v = rising->InterpolateVoltage(CORNER_TYP, current_phase*dt);
		else
			current_v = falling->InterpolateVoltage(CORNER_TYP, current_phase*dt);

		float last_v;
		if(last_bit)
			last_v = rising->InterpolateVoltage(CORNER_TYP, last_phase*dt);
		else
			last_v = falling->InterpolateVoltage(CORNER_TYP, last_phase*dt);

		//See if the current UI's edge has started
		float delta = current_v - current_v_old;
		if(current_phase < 1)
			delta = 0;
		if( (fabs(delta) > 0.001) && (last_bit != current_bit) )
		{
			last_ui_start	= ui_start;
			current_edge_started = true;
		}

		//If so, use the new value. If propagation delay isn't over, keep the old edge going
		float v;
		if(current_edge_started)
			v = current_v;
		else
			v = last_v;

		current_v_old	= current_v;

		//Look up the old bit value and continue the transition a bit longer if possible.
		LogDebug("%f, %f, %f, %f, %f, %d, %zu\n", time*1e9, v, current_v + 1, last_v + 2, delta, current_edge_started, last_ui_start);
	}

	return 0;
}
