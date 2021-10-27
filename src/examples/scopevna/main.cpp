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
#include <ffts.h>

using namespace std;

#define FFT_LEN 16777216

void OnWaveform(Oscilloscope* scope, ffts_plan_t* plan);
void OnDone(int signal);

FILE* g_fpOut = NULL;
bool g_quitting = false;

float g_lastFreq = 0;
bool g_shifting = false;

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

	//Initial scope configuration
	scope->EnableChannel(2);
	scope->EnableChannel(3);
	scope->SetSampleRate(40000000000UL);
	scope->SetSampleDepth(20000000UL);
	scope->Start();

	//Set up the FFT
	//The scope wants nice round number sample depths (plus a few extra): we get 20000003 points.
	//FFT needs power of two so only use the first 16777216 samples.
	ffts_plan_t* plan = ffts_init_1d_real(FFT_LEN, FFTS_FORWARD);

	//Signal handler for shutdown
	signal(SIGINT, OnDone);

	//Open the output S-parameter file
	g_fpOut = fopen("/tmp/test.s2p", "w");
	fprintf(g_fpOut, "# HZ S MA R 50.0\n");

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
		OnWaveform(scope, plan);
	}

	//Clean up
	ffts_free(plan);
}

void OnDone(int /*ignored*/)
{
	LogNotice("Cleaning up\n");
	fclose(g_fpOut);
	g_quitting = true;
}

void OnWaveform(Oscilloscope* scope, ffts_plan_t* plan)
{
	auto ref = dynamic_cast<AnalogWaveform*>(scope->GetChannel(2)->GetData(0));
	auto dut = dynamic_cast<AnalogWaveform*>(scope->GetChannel(3)->GetData(0));

	AlignedAllocator< float, 64 > allocator;

	//Window the data
	float* inref = allocator.allocate(FFT_LEN);
	float* indut = allocator.allocate(FFT_LEN);
	FFTFilter::ApplyWindow(
		reinterpret_cast<float*>(&ref->m_samples[0]), FFT_LEN, inref, FFTFilter::WINDOW_BLACKMAN_HARRIS);
	FFTFilter::ApplyWindow(
		reinterpret_cast<float*>(&dut->m_samples[0]), FFT_LEN, indut, FFTFilter::WINDOW_BLACKMAN_HARRIS);

	//Do the forward FFT
	float sampleGHZ = 40;
	size_t nouts = FFT_LEN/2 + 1;
	float binHz = round((0.5f * sampleGHZ * 1e9f) / nouts);
	float* fref = allocator.allocate(nouts*2);
	float* fdut = allocator.allocate(nouts*2);
	ffts_execute(plan, inref, fref);
	ffts_execute(plan, indut, fdut);

	//Find the highest point in the reference waveform
	size_t highestBin = 0;
	float highestMag = 0;
	for(size_t i=0; i<nouts; i++)
	{
		float real = fref[i*2];
		float imag = fref[i*2 + 1];
		float mag = sqrt(real*real + imag*imag);

		if(mag > highestMag)
		{
			highestBin = i;
			highestMag = mag;
		}
	}

	//Get mag/angle for this bin in both waveforms
	float real = fref[highestBin*2];
	float imag = fref[highestBin*2 + 1];
	float refMag = sqrt(real*real + imag*imag);
	float refAngle = atan2(imag, real);

	real = fdut[highestBin*2];
	imag = fdut[highestBin*2 + 1];
	float dutMag = sqrt(real*real + imag*imag);
	float dutAngle = atan2(imag, real);

	//Calculate relative S21 magnitude and angle
	float s21_mag = dutMag / refMag;
	float s21_db = 20*log10(s21_mag);
	float s21_ang = dutAngle - refAngle;
	if(s21_ang > M_PI)
		s21_ang -= 2*M_PI;
	if(s21_ang < -M_PI)
		s21_ang += 2*M_PI;

	//TODO: calibration for cable/splitter mismatch

	//Print peak info
	Unit hz(Unit::UNIT_HZ);
	Unit db(Unit::UNIT_DB);
	Unit deg(Unit::UNIT_DEGREES);
	float binFreq = binHz * (highestBin);
	float s21_deg = s21_ang * 180 / M_PI;

	LogDebug("%s: mag = %s, ang = %s\n",
		hz.PrettyPrint(binFreq).c_str(),
		db.PrettyPrint(s21_db).c_str(),
		deg.PrettyPrint(s21_deg).c_str());

	//If the last waveform was a shift, we should be stable now.
	//Update the .s2p file with our new data.
	if(g_shifting)
	{
		fprintf(g_fpOut, "%f 0 0 %f %f 0 0 0 0\n", binFreq, s21_mag, s21_deg);
		g_lastFreq = binFreq;
	}

	//Record a shift if our center frequency is at least 1 kHz greater than the last waveform
	g_shifting = ( (binFreq - g_lastFreq) > 1000);
	if(g_shifting)
		LogDebug("Input frequency shift detected\n");

	//Clean up
	allocator.deallocate(fref);
	allocator.deallocate(fdut);
	allocator.deallocate(inref);
	allocator.deallocate(indut);
}
