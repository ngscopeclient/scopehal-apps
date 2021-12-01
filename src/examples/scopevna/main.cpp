/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2021 Andrew D. Zonenberg                                                                          *
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
	@file	main.cpp
	@brief	Oscilloscope-as-VNA tool
 */

#include "../../../lib/scopehal/scopehal.h"
#include "../../../lib/scopeprotocols/scopeprotocols.h"

using namespace std;

void OnWaveform(Oscilloscope* scope);
void OnDone(int signal);

FILE* g_fpOut = NULL;
bool g_quitting = false;

float g_lastFreq = 0;
bool g_shifting = false;

Filter* g_thresholdFilter = NULL;
Filter* g_refMixerFilter = NULL;
Filter* g_dutMixerFilter = NULL;

Filter* g_refIfIFilter = NULL;
Filter* g_refIfQFilter = NULL;
Filter* g_dutIfIFilter = NULL;
Filter* g_dutIfQFilter = NULL;

Filter* g_dutMagnitudeFilter = NULL;
Filter* g_refMagnitudeFilter = NULL;

Filter* g_dutPhaseFilter = NULL;
Filter* g_refPhaseFilter = NULL;
Filter* g_phaseDiffFilter = NULL;

void BuildFilterGraph(Oscilloscope* scope);

int main(int argc, char* argv[])
{
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	string scopepath;
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
		else if(s[0] == '-')
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
		else
			scopepath = argv[i];
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Don't use OpenCL for now
	g_disableOpenCL = true;

	//Initialize object creation tables for predefined libraries
	TransportStaticInit();
	DriverStaticInit();
	ScopeProtocolStaticInit();

	//Parse arguments
	char nick[128];
	char driver[128];
	char trans[128];
	char args[128];
	if(4 != sscanf(scopepath.c_str(), "%127[^:]:%127[^:]:%127[^:]:%127s", nick, driver, trans, args))
	{
		args[0] = '\0';
		if(3 != sscanf(scopepath.c_str(), "%127[^:]:%127[^:]:%127[^:]", nick, driver, trans))
		{
			LogError("Invalid scope string %s\n", scopepath.c_str());
			return 1;
		}
	}

	//Connect to scope
	SCPITransport* transport = SCPITransport::CreateTransport(trans, args);
	if(transport == NULL)
		return 1;
	if(!transport->IsConnected())
	{
		LogError("Failed to connect to instrument using connection string %s\n", scopepath.c_str());
		return 1;
	}
	Oscilloscope* scope = Oscilloscope::CreateOscilloscope(driver, transport);
	if(scope == NULL)
		return 1;
	scope->m_nickname = nick;

	//Initial scope configuration: not interleaved, 20 Gsps, 2M points
	//Probe on 0, ref on 1
	scope->EnableChannel(0);
	scope->EnableChannel(1);
	scope->SetSampleRate(20000000000UL);
	scope->SetSampleDepth(2000000UL);
	scope->Start();

	//Signal handler for shutdown
	signal(SIGINT, OnDone);

	//Open the output S-parameter file
	g_fpOut = fopen("/tmp/test.s2p", "w");
	fprintf(g_fpOut, "# HZ S MA R 50.0\n");

	//Create the filter graph where all our fun happens
	BuildFilterGraph(scope);

	//Main loop
	while(!g_quitting)
	{
		//Wait for trigger
		if(scope->PollTrigger() != Oscilloscope::TRIGGER_MODE_TRIGGERED)
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			continue;
		}

		//Grab the data
		scope->AcquireData();
		scope->PopPendingWaveform();
		OnWaveform(scope);
	}

	/*
	g_thresholdFilter->Release();
	g_refMixerFilter->Release();
	g_dutMixerFilter->Release();
	g_refIfIFilter->Release();
	g_refIfQFilter->Release();
	g_dutIfIFilter->Release();
	g_dutIfQFilter->Release();
	g_refMagnitudeFilter->Release();
	g_dutMagnitudeFilter->Release();
	g_refPhaseFilter->Release();
	g_dutPhaseFilter->Release();
	g_phaseDiffFilter->Release();
	*/
}

void OnDone(int /*ignored*/)
{
	LogNotice("Cleaning up\n");
	fclose(g_fpOut);
	g_quitting = true;
}

void BuildFilterGraph(Oscilloscope* scope)
{
	auto refchan = scope->GetChannel(1);
	auto dutchan = scope->GetChannel(0);

	//Threshold the reference waveform (with 10 mV hysteresis to prevent problems at really low frequencies)
	g_thresholdFilter = Filter::CreateFilter("Threshold");
	g_thresholdFilter->SetInput(0, StreamDescriptor(refchan, 0));
	g_thresholdFilter->GetParameter("Threshold").SetFloatVal(0);
	g_thresholdFilter->GetParameter("Hysteresis").SetFloatVal(0.01);

	//Mix the reference and DUT waveform with coherent LOs
	g_refMixerFilter = Filter::CreateFilter("Downconvert");
	g_refMixerFilter->SetInput(0, StreamDescriptor(refchan, 0));

	g_dutMixerFilter = Filter::CreateFilter("Downconvert");
	g_dutMixerFilter->SetInput(0, StreamDescriptor(dutchan, 0));

	//Band-pass filter the mixed I/Q to remove images, harmonics, and other out-of-band stuff
	g_refIfIFilter = Filter::CreateFilter("FIR Filter");
	g_refIfIFilter->SetInput(0, StreamDescriptor(g_refMixerFilter, 0));
	g_refIfIFilter->GetParameter("Filter Type").ParseString("Band pass");

	g_refIfQFilter = Filter::CreateFilter("FIR Filter");
	g_refIfQFilter->SetInput(0, StreamDescriptor(g_refMixerFilter, 1));
	g_refIfQFilter->GetParameter("Filter Type").ParseString("Band pass");

	g_dutIfIFilter = Filter::CreateFilter("FIR Filter");
	g_dutIfIFilter->SetInput(0, StreamDescriptor(g_dutMixerFilter, 0));
	g_dutIfIFilter->GetParameter("Filter Type").ParseString("Band pass");

	g_dutIfQFilter = Filter::CreateFilter("FIR Filter");
	g_dutIfQFilter->SetInput(0, StreamDescriptor(g_dutMixerFilter, 1));
	g_dutIfQFilter->GetParameter("Filter Type").ParseString("Band pass");

	g_refMagnitudeFilter = Filter::CreateFilter("Vector Magnitude");
	g_refMagnitudeFilter->SetInput(0, StreamDescriptor(g_refIfIFilter, 0));
	g_refMagnitudeFilter->SetInput(1, StreamDescriptor(g_refIfQFilter, 0));

	g_dutMagnitudeFilter = Filter::CreateFilter("Vector Magnitude");
	g_dutMagnitudeFilter->SetInput(0, StreamDescriptor(g_dutIfIFilter, 0));
	g_dutMagnitudeFilter->SetInput(1, StreamDescriptor(g_dutIfQFilter, 0));

	g_refPhaseFilter = Filter::CreateFilter("Vector Phase");
	g_refPhaseFilter->SetInput(0, StreamDescriptor(g_refIfIFilter, 0));
	g_refPhaseFilter->SetInput(1, StreamDescriptor(g_refIfQFilter, 0));

	g_dutPhaseFilter = Filter::CreateFilter("Vector Phase");
	g_dutPhaseFilter->SetInput(0, StreamDescriptor(g_dutIfIFilter, 0));
	g_dutPhaseFilter->SetInput(1, StreamDescriptor(g_dutIfQFilter, 0));

	g_phaseDiffFilter = Filter::CreateFilter("Subtract");
	g_phaseDiffFilter->SetInput(0, StreamDescriptor(g_dutPhaseFilter, 0));
	g_phaseDiffFilter->SetInput(1, StreamDescriptor(g_refPhaseFilter, 0));
}

void OnWaveform(Oscilloscope* /*scope*/)
{
	//LogDebug("Got a waveform\n");
	//LogIndenter li;
	Filter::ClearAnalysisCache();
	Filter::SetAllFiltersDirty();

	//Get the frequency of the reference waveform
	g_thresholdFilter->Refresh();
	vector<int64_t> edges;
	Filter::FindRisingEdges(dynamic_cast<DigitalWaveform*>(g_thresholdFilter->GetData(0)), edges);
	if(edges.size() < 2)
		return;
	int64_t tfirst = edges[0];
	int64_t tlast = edges[edges.size()-1];
	int64_t refPeriodFS = (tlast - tfirst) / (edges.size()-1);
	int64_t refFreqHz = FS_PER_SECOND / refPeriodFS;
	Unit hz(Unit::UNIT_HZ);
	//LogDebug("Calculated reference frequency is %s\n", hz.PrettyPrint(refFreqHz).c_str());

	//Record a shift if our center frequency is at least 1 kHz greater than the last waveform
	bool g_lastWasShift = g_shifting;
	g_shifting = ( (refFreqHz - g_lastFreq) > 1000);
	if(g_shifting)
	{
		//LogDebug("Input frequency shift detected\n");
		g_lastFreq = refFreqHz;
		return;
	}

	//If last cycle was a shift, we should be stable now - process stuff.
	//if *not* a shift, we're a duplicate so ignore it
	if(!g_lastWasShift)
		return;

	//We want a 50-100 MHz IF to get a reasonable number of cycles in the test waveform.
	//Configure the LO to up- or downconvert based on the input frequency.
	//Use a bit of hysteresis to prevent ridiculously low LO frequencies.
	int64_t downconvertTarget = 60 * 1000 * 1000;
	int64_t downconvertThreshold = 70 * 1000 * 1000;
	int64_t upconvertTarget = 80 * 1000 * 1000;
	int64_t loFreq = 0;
	int64_t ifFreq = 0;
	if(refFreqHz > downconvertThreshold)
	{
		loFreq = refFreqHz - downconvertTarget;
		ifFreq = refFreqHz - loFreq;
		/*
		LogDebug("Downconverting with %s LO to get %s IF\n",
			hz.PrettyPrint(loFreq).c_str(),
			hz.PrettyPrint(ifFreq).c_str());
		*/
	}
	else
	{
		loFreq = upconvertTarget - refFreqHz;
		ifFreq = refFreqHz + loFreq;
		/*
		LogDebug("Upconverting with %s LO to get %s IF\n",
			hz.PrettyPrint(loFreq).c_str(),
			hz.PrettyPrint(ifFreq).c_str());
		*/
	}

	//Configure the mixers
	g_refMixerFilter->GetParameter("LO Frequency").SetFloatVal(loFreq);
	g_dutMixerFilter->GetParameter("LO Frequency").SetFloatVal(loFreq);

	//Configure the IF bandpass filters with 10 MHz bandwidth
	int64_t ifBandLow = ifFreq - (5 * 1000 * 1000);
	int64_t ifBandHigh = ifFreq + (5 * 1000 * 1000);
	g_refIfIFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_refIfIFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_refIfQFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_refIfQFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_dutIfIFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_dutIfIFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_dutIfQFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_dutIfQFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);

	//Run the final filters in the graph
	g_refMagnitudeFilter->RefreshIfDirty();
	g_dutMagnitudeFilter->RefreshIfDirty();
	g_phaseDiffFilter->RefreshIfDirty();

	//Calculate average amplitude
	float avgRef = 0;
	auto refdata = dynamic_cast<AnalogWaveform*>(g_refMagnitudeFilter->GetData(0));
	for(auto f : refdata->m_samples)
		avgRef += f;
	avgRef /= refdata->m_samples.size();

	float avgDut = 0;
	auto dutdata = dynamic_cast<AnalogWaveform*>(g_dutMagnitudeFilter->GetData(0));
	for(auto f : dutdata->m_samples)
		avgDut += f;
	avgDut /= dutdata->m_samples.size();

	//S21 magnitude is just the ratio of measured amplitudes
	float s21_mag = avgDut / avgRef;
	float s21_mag_db = 20 * log10(s21_mag);

	//Calculate average phase delta
	float s21_deg = 0;
	auto phasedata = dynamic_cast<AnalogWaveform*>(g_phaseDiffFilter->GetData(0));
	for(auto f : phasedata->m_samples)
		s21_deg += f;
	s21_deg /= phasedata->m_samples.size();
	s21_deg *= -1;

	//Print peak info
	Unit db(Unit::UNIT_DB);
	Unit deg(Unit::UNIT_DEGREES);

	LogDebug("%s: mag = %s, ang = %s\n",
		hz.PrettyPrint(refFreqHz).c_str(),
		db.PrettyPrint(s21_mag_db).c_str(),
		deg.PrettyPrint(s21_deg).c_str());

	//If the last waveform was a shift, we should be stable now.
	//Update the .s2p file with our new data.
	fprintf(g_fpOut, "%ld 0 0 %f %f 0 0 0 0\n", refFreqHz, s21_mag, s21_deg);
}
