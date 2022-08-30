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
#include "../../../lib/scopehal/SiglentVectorSignalGenerator.h"

using namespace std;

FILE* g_fpOut = NULL;

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

OscilloscopeChannel* g_dutChannel = NULL;
OscilloscopeChannel* g_refChannel = NULL;

void BuildFilterGraph(Oscilloscope* scope);
void OnWaveform(float freq, int iteration);

float last_phase = 0;

float g_phases[3];
float g_mags[3];

int CompareFloat(const void* a, const void* b);

int CompareFloat(const void* a, const void* b)
{
	float* pa = (float*)a;
	float* pb = (float*)b;

	return *pa < *pb;
}

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
	//g_disableOpenCL = true;
	g_searchPaths.push_back("/ceph/fast/home/azonenberg/code/scopehal-apps/lib/scopeprotocols");

	//Initialize object creation tables for predefined libraries
	VulkanInit();
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

	//Initial scope configuration: not interleaved
	//Probe on 3, ref on 4
	scope->EnableChannel(3);
	scope->EnableChannel(4);

	//Connect to signal generator, configure for 0 dBm
	//TODO: dynamic creation etc
	auto genTransport = SCPITransport::CreateTransport("lan", "ssg.scada.poulsbo.antikernel.net");
	auto gen = new SiglentVectorSignalGenerator(genTransport);
	gen->SetChannelOutputPower(0, 0);
	gen->SetChannelOutputEnable(0, true);

	//Open the output S-parameter file
	g_fpOut = fopen("/tmp/test.s2p", "w");
	fprintf(g_fpOut, "# HZ S MA R 50.0\n");

	//Create the filter graph where all our fun happens
	BuildFilterGraph(scope);

	//Load the calibration file, if it exists
	TouchstoneParser parser;
	SParameters params;
	bool has_cal = parser.Load("/tmp/scopevna-cal.s2p", params);

	//Main processing loop
	//for(float freq = 0; freq < 6e9; freq += 1e7)
	for(float freq = 0; freq < 6e8; freq += 1e6)
	{
		//Clamp lowest frequency to 9 kHz
		float realfreq = freq;
		if(freq < 9e3)
			realfreq = 9e3;

		//For higher freqs: 5 Gsps, 1M points
		//Below 100 MHz: 1 Gsps
		if(freq < 1e8)
			scope->SetSampleRate(1000000000UL);
		else
			scope->SetSampleRate(5000000000UL);
		scope->SetSampleDepth(1000000UL);

		//Set the frequency
		gen->SetChannelCenterFrequency(0, realfreq);

		//Wait a while to make sure synthesizer has updated
		std::this_thread::sleep_for(std::chrono::milliseconds(20));

		//Grab a couple of waveforms
		for(int i=0; i<3; i++)
		{
			//Grab a waveform
			scope->StartSingleTrigger();
			while(scope->PollTrigger() != Oscilloscope::TRIGGER_MODE_TRIGGERED)
				std::this_thread::sleep_for(std::chrono::milliseconds(50));
			scope->AcquireData();
			scope->PopPendingWaveform();

			//Crunch it
			OnWaveform(realfreq, i);
		}

		//Caculate median value
		qsort(g_phases, 3, sizeof(float), CompareFloat);
		qsort(g_mags, 3, sizeof(float), CompareFloat);
		float mag = g_mags[1];
		float ang = g_phases[1];

		//Apply calibration
		if(has_cal)
		{
			//LogDebug("base mag/ang = %f, %f\n", mag, ang);

			auto& cal_s21 = params[SPair(2, 1)];
			auto cal_mag = cal_s21.InterpolateMagnitude(realfreq);
			auto cal_ang = cal_s21.InterpolateAngle(realfreq) * 180 / M_PI;
			//LogDebug("cal mag/ang = %f, %f\n", cal_mag, cal_ang);

			mag /= cal_mag;
			ang -= cal_ang;

			if(ang <= -180)
				ang += 360;
			if(ang >= 180)
				ang -= 360;
		}

		//Write to touchstone file
		fprintf(g_fpOut, "%.0f 0 0 %f %f 0 0 0 0\n", realfreq, mag, ang);
		fflush(g_fpOut);
	}

	//Done, clean up
	gen->SetChannelOutputEnable(0, false);
	fclose(g_fpOut);
}

void BuildFilterGraph(Oscilloscope* scope)
{
	g_refChannel = scope->GetChannel(3);
	g_dutChannel = scope->GetChannel(2);

	//Mix the reference and DUT waveform with coherent LOs
	g_refMixerFilter = Filter::CreateFilter("Downconvert");
	g_refMixerFilter->SetInput(0, StreamDescriptor(g_refChannel, 0));

	g_dutMixerFilter = Filter::CreateFilter("Downconvert");
	g_dutMixerFilter->SetInput(0, StreamDescriptor(g_dutChannel, 0));

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
	g_phaseDiffFilter->SetInput(0, StreamDescriptor(g_refPhaseFilter, 0));
	g_phaseDiffFilter->SetInput(1, StreamDescriptor(g_dutPhaseFilter, 0));
}

void OnWaveform(float refFreqHz, int iteration)
{
	//LogDebug("Got a waveform\n");
	//LogIndenter li;
	Filter::ClearAnalysisCache();

	//We want a 50-100 MHz IF to get a reasonable number of cycles in the test waveform.
	//Configure the LO to up- or downconvert based on the input frequency.
	//Use a bit of hysteresis to prevent ridiculously low LO frequencies.
	int64_t downconvertTarget = 60 * 1000 * 1000;
	int64_t downconvertThreshold = 70 * 1000 * 1000;
	int64_t upconvertTarget = 80 * 1000 * 1000;
	int64_t loFreq = 0;
	int64_t ifFreq = 0;
	bool invertPhase;
	if(refFreqHz > downconvertThreshold)
	{
		loFreq = refFreqHz - downconvertTarget;
		ifFreq = refFreqHz - loFreq;
		invertPhase = false;
		/*
		LogDebug("Downconverting with %s LO to get %s IF\n",
			hz.PrettyPrint(loFreq).c_str(),
			hz.PrettyPrint(ifFreq).c_str());
		*/
	}

	//When upconverting we're using the other sideband
	//Invert the phase to compensate
	else
	{
		loFreq = upconvertTarget - refFreqHz;
		ifFreq = refFreqHz + loFreq;
		invertPhase = true;
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
	if(ifFreq < 5e6)
		ifBandLow = 0;
	int64_t ifBandHigh = ifFreq + (5 * 1000 * 1000);
	g_refIfIFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_refIfIFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_refIfQFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_refIfQFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_dutIfIFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_dutIfIFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);
	g_dutIfQFilter->GetParameter("Frequency Low").SetFloatVal(ifBandLow);
	g_dutIfQFilter->GetParameter("Frequency High").SetFloatVal(ifBandHigh);

	//Run the filter graph
	FilterGraphExecutor ex;
	ex.RunBlocking(Filter::GetAllInstances());

	//Calculate average amplitude
	float avgRef = 0;
	auto refdata = dynamic_cast<UniformAnalogWaveform*>(g_refMagnitudeFilter->GetData(0));
	if(refdata == nullptr)
	{
		LogWarning("null waveform\n");
		return;
	}
	for(auto f : refdata->m_samples)
		avgRef += f;
	avgRef /= refdata->m_samples.size();

	float avgDut = 0;
	auto dutdata = dynamic_cast<UniformAnalogWaveform*>(g_dutMagnitudeFilter->GetData(0));
	if(dutdata == nullptr)
	{
		LogWarning("null waveform\n");
		return;
	}
	for(auto f : dutdata->m_samples)
		avgDut += f;
	avgDut /= dutdata->m_samples.size();

	//S21 magnitude is just the ratio of measured amplitudes
	float s21_mag = avgDut / avgRef;
	float s21_mag_db = 20 * log10(s21_mag);

	//Calculate average phase delta
	auto phasedata = dynamic_cast<UniformAnalogWaveform*>(g_phaseDiffFilter->GetData(0));
	float s21_ang_i = 0;
	float s21_ang_q = 0;
	for(auto f : phasedata->m_samples)
	{
		float rad = f * M_PI / 180;
		s21_ang_i += sin(rad);
		s21_ang_q += cos(rad);
	}
	float s21_deg = atan2(s21_ang_i, s21_ang_q) * 180 / M_PI;
	if(invertPhase)
		s21_deg = -s21_deg;

	g_phases[iteration] = s21_deg;
	g_mags[iteration] = s21_mag;

	//Print peak info
	Unit db(Unit::UNIT_DB);
	Unit hz(Unit::UNIT_HZ);
	Unit deg(Unit::UNIT_DEGREES);

	LogDebug("%s: mag = %s, ang = %s\n",
		hz.PrettyPrint(refFreqHz).c_str(),
		db.PrettyPrint(s21_mag_db).c_str(),
		deg.PrettyPrint(s21_deg).c_str());
}
