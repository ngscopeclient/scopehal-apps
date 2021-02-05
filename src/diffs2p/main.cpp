/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
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
	@file
	@author Andrew D. Zonenberg
	@brief Program entry point
 */

#include "../scopehal/scopehal.h"
using namespace std;

int main(int argc, char* argv[])
{
	//Global settings
	Severity console_verbosity = Severity::NOTICE;

	//Parse command-line arguments
	vector<string> files;
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
		else if(s == "--version")
		{
			//not implemented
			//ShowVersion();
			return 0;
		}
		else if(s[0] == '-')
		{
			fprintf(stderr, "Unrecognized command-line argument \"%s\", use --help\n", s.c_str());
			return 1;
		}
		else
			files.push_back(s);
	}

	//Set up logging
	g_log_sinks.emplace(g_log_sinks.begin(), new ColoredSTDLogSink(console_verbosity));

	//Expect three arguments: inP-to-out, inN-to-out, out
	if(files.size() != 3)
	{
		LogNotice("Usage: diffs2p inP-to-out.s2p inN-to-out.s2p diff-to-out.s2p\n");
		return 0;
	}

	//Load the input files
	SParameters inP;
	SParameters inN;
	TouchstoneParser parser;
	parser.Load(files[0], inP);
	parser.Load(files[1], inN);

	//TODO: S-parameter generator class
	FILE* fout = fopen(files[2].c_str(), "w");
	fprintf(fout, "# MHZ S MA R 50\n");

	auto& s21_p = inP[SPair(2, 1)];
	auto& s21_n = inN[SPair(2, 1)];
	auto& s22 = inP[SPair(2, 2)];		//inN S22 should be identical as they're the same port
	for(size_t i=0; i<s21_p.size(); i++)
	{
		//LogDebug("Point: %f GHz\n", s21_p[i].m_frequency * 1e-9);
		//LogIndenter li;

		//Convert mag/angle to real/imag
		auto& p = s21_p[i];
		auto& n = s21_n[i];
		float p_real = p.m_amplitude * cos(p.m_phase);
		float p_imag = p.m_amplitude * sin(p.m_phase);
		float n_real = n.m_amplitude * cos(n.m_phase);
		float n_imag = n.m_amplitude * sin(n.m_phase);

		//Compute the differential in-to-out transfer function.
		//We do this by applying a simulus of +0.5 to P and -0.5 to N, giving a differential amplitude of unity.
		float sum_real = (p_real - n_real) / 2;
		float sum_imag = (p_imag - n_imag) / 2;

		//Convert back to polar form
		float sum_ang = atan2(sum_imag, sum_real);
		float sum_mag = sqrt(sum_imag*sum_imag + sum_real*sum_real);

		//Convert angle from radians to degrees
		sum_ang *= (180 / M_PI);

		/*
		LogDebug("P: %f + %f*i\n", p_real, p_imag);
		LogDebug("N: %f + %f*i\n", n_real, n_imag);
		LogDebug("S: %f + %f*i\n", sum_real, sum_imag);
		LogDebug(" = mag %f ang %f\n", sum_mag, sum_ang);
		*/

		//For now, only output S21 and S22
		//TODO: reverse path and input S11
		auto& reverse = s22[i];

		fprintf(fout,
			"%11f %11f %11f %11f %11f %11f %11f %11f %11f\n",
			p.m_frequency * 1e-6,
			0.0, 0.0,			//S11
			sum_mag, sum_ang,	//S21
			0.0, 0.0,			//S12,
			reverse.m_amplitude, reverse.m_phase * (180 / M_PI) );
	}

	fclose(fout);
	return 0;
}
