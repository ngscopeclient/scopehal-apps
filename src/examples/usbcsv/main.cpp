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
#include "../scopehal/MockOscilloscope.h"
#include "../scopeprotocols/scopeprotocols.h"

using namespace std;

bool ProcessWaveform(MockOscilloscope* scope, const string& fname, USB2PacketDecoder* pdecode);
USB2PacketDecoder* CreateFilterGraph(Oscilloscope* scope);
string SymbolToString(USB2PacketSymbol sym);
int64_t Round(int64_t number, int64_t step);

enum USBState {
			   SETUP,
			   IN,
			   WAIT,
};


int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	string fname = "";

	//Parse command-line arguments
	for(int i=1; i<argc; i++)
	{
		string s(argv[i]);

		//Let the logger eat its args first
		if(ParseLoggerArguments(i, argc, argv, console_verbosity))
			continue;

		else
			fname = argv[i];
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Initialize object creation tables
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();
	InitializePlugins();

	//Create a dummy scope to use for import
	auto scope = new MockOscilloscope("CSV Import", "Generic", "12345");
	scope->m_nickname = "import";

	//Load the first waveform from the batch.
	//We need to do this before setting up the filter graph so that we have channel names and types.
	LogDebug("Loading first waveform\n");
	if(!scope->LoadCSV(fname))
	{
		LogError("Failed to load CSV %s\n", fname.c_str());
		return 0;
	}

	//Set up the decodes
	auto pdecode = CreateFilterGraph(scope);

	//Load and decode each waveform in the batch.
	//Can loop this.
	if(!ProcessWaveform(scope, fname, pdecode))
		return 1;

	//Clean up
	pdecode->Release();
	delete scope;
	return 0;
}

USB2PacketDecoder* CreateFilterGraph(Oscilloscope* scope)
{
	//Decode the PMA layer (differential voltages to J/K/SE0/SE1 line states)
	auto pma = Filter::CreateFilter(USB2PMADecoder::GetProtocolName());
	pma->GetParameter("Speed").SetIntVal(USB2PMADecoder::SPEED_LOW);

	//As you can see the channels are switched. This is an historical mistake.
	pma->SetInput(0, StreamDescriptor(scope->GetChannel(1)));
	pma->SetInput(1, StreamDescriptor(scope->GetChannel(0)));

	//Decode the PCS layer (line states to data bytes and sync/end events)
	auto pcs = Filter::CreateFilter(USB2PCSDecoder::GetProtocolName());
	pcs->SetInput(0, StreamDescriptor(pma));

	//Decode the packet layer (bytes to packet fields)
	auto pack = Filter::CreateFilter(USB2PacketDecoder::GetProtocolName());
	pack->SetInput(0, StreamDescriptor(pcs));
	pack->AddRef();
	return dynamic_cast<USB2PacketDecoder*>(pack);
}

//I created this because my LA only samples at a certain rate
//and femtoseconds is too much precision.
int64_t Round(int64_t number, int64_t step)
{
	int64_t leftover = number % step;
	int64_t base = number - leftover;

	if (leftover > step / 2)
	{
		base += step;
	}
	return base;
}


bool ProcessWaveform(MockOscilloscope* scope, const string& fname, USB2PacketDecoder* pdecode)
{
	//Import the waveform
	//LogNotice("Loading waveform \"%s\"\n", fname.c_str());
	//LogIndenter li;

	if(!scope->LoadCSV(fname))
	{
		LogError("Failed to load CSV %s\n", fname.c_str());
		return false;
	}

	//Run the filter graph
	FilterGraphExecutor ex;
	ex.RunBlocking(Filter::GetAllInstances());

	auto waveform = dynamic_cast<USB2PacketWaveform*>(pdecode->GetData(0));
	if(!waveform)
	{
		LogError("Decode failed\n");
		return false;
	}

	Unit fs(Unit::UNIT_FS);

	//This number is the number of femtoseconds between each sample
	//in USB captures I have taken.
	int64_t step = 8e7;

	//Print the protocol analyzer data
	{
		size_t len = waveform->m_samples.size();
		USBState state = USBState::SETUP;
		std::vector<size_t> elts;
		std::vector<size_t> setup;
		bool collect = false;

		for(size_t i=0; i<len;)
		{
			// scope->m_channels[0]->m_streamData[0]->m_offsets.size()

			for (; i<len; i++)
			{
				string sym = SymbolToString(waveform->m_samples[i]);
				if (sym == "SETUP")
				{
					state = USBState::SETUP;
					setup.clear();
				}
				else if (state == USBState::SETUP && sym == "Data")
				{
					setup.push_back(i);
				}
				else if (sym == "IN")
				{
					state = USBState::IN;
				}
				else if (state == USBState::IN && (sym[0] == 'D' || sym == "NAK")) // All the packets are: DATA0, DATA1, Data
				{
					elts.push_back(i);
					collect = true;
				}
				else if (collect)
				{
					elts.push_back(i);
					state = USBState::WAIT;
					collect = false;
					break;
				}

			}

			if (elts.size() == 0)
			{
				LogError("No packets found.\n");
				return false;
			}

			auto first = elts[0];
			auto last = elts[elts.size()-1];

			int64_t left = Round(waveform->m_offsets[first] * waveform->m_timescale, step)/step;
			int64_t right = Round(waveform->m_offsets[last] * waveform->m_timescale, step)/step;
			string pid = SymbolToString(waveform->m_samples[first]);

			LogNotice("%lu %lu %s ", left, right, pid.c_str());

			for (auto loc : elts)
			{
				auto sym = waveform->m_samples[loc];
				LogNotice("%02x ", sym.m_data);
			}

			LogNotice("| ");
			for (auto loc : setup)
			{
				auto sym = waveform->m_samples[loc];
				LogNotice("%02x ", sym.m_data);
			}

			LogNotice("\n");

			elts.clear();
		}
	}

	return true;
}

string SymbolToString(USB2PacketSymbol sym)
{
	string type = "";
	switch(sym.m_type)
		{
	    case USB2PacketSymbol::TYPE_PID:
			switch(sym.m_data & 0x0f)
				{
			    case USB2PacketSymbol::PID_RESERVED:
					type += "(reserved)";
					break;

			    case USB2PacketSymbol::PID_OUT:
					type += "OUT";
					break;

				case USB2PacketSymbol::PID_ACK:
					type += "ACK";
					break;

				case USB2PacketSymbol::PID_DATA0:
					type += "DATA0";
					break;

				case USB2PacketSymbol::PID_PING:
					type += "PING";
					break;

				case USB2PacketSymbol::PID_SOF:
					type += "SOF";
					break;

				case USB2PacketSymbol::PID_NYET:
					type += "NYET";
					break;

				case USB2PacketSymbol::PID_DATA2:
					type += "DATA2";
					break;

				case USB2PacketSymbol::PID_SPLIT:
					type += "SPLIT";
					break;

				case USB2PacketSymbol::PID_IN:
					type += "IN";
					break;

				case USB2PacketSymbol::PID_NAK:
					type += "NAK";
					break;

				case USB2PacketSymbol::PID_DATA1:
					type += "DATA1";
					break;

				case USB2PacketSymbol::PID_PRE_ERR:
					type += "PRE_ERR";
					break;

				case USB2PacketSymbol::PID_SETUP:
					type += "SETUP";
					break;

				case USB2PacketSymbol::PID_STALL:
					type += "STALL";
					break;

				case USB2PacketSymbol::PID_MDATA:
					type += "MDATA";
					break;
				}
			break;

		case USB2PacketSymbol::TYPE_ADDR:
			type = "Addr";
			break;

		case USB2PacketSymbol::TYPE_ENDP:
			type = "ENDP";
			break;

		case USB2PacketSymbol::TYPE_CRC5_GOOD:
			type = "CRC5(good)";
			break;

		case USB2PacketSymbol::TYPE_CRC5_BAD:
			type = "CRC5(bad)";
			break;

		case USB2PacketSymbol::TYPE_CRC16_GOOD:
			type = "CRC16(good)";
			break;

		case USB2PacketSymbol::TYPE_CRC16_BAD:
			type = "CRC16(bad)";
			break;

		case USB2PacketSymbol::TYPE_NFRAME:
			type = "NFRAME";
			break;

		case USB2PacketSymbol::TYPE_DATA:
			type = "Data";
			break;

		case USB2PacketSymbol::TYPE_ERROR:
			type = "ERROR";
			break;
		}

	return type;
}
