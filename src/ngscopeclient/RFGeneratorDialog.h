/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2022 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of RFGeneratorDialog
 */
#ifndef RFGeneratorDialog_h
#define RFGeneratorDialog_h

#include "Dialog.h"
#include "RollingBuffer.h"
#include "Session.h"

class RFGeneratorChannelUIState
{
public:
	bool m_outputEnabled;

	std::string m_amplitude;
	float m_committedAmplitude;

	std::string m_offset;
	float m_committedOffset;

	std::string m_frequency;
	float m_committedFrequency;
};

class RFGeneratorDialog : public Dialog
{
public:
	RFGeneratorDialog(SCPIRFSignalGenerator* meter, Session* session);
	virtual ~RFGeneratorDialog();

	virtual bool DoRender();

	SCPIRFSignalGenerator* GetGenerator()
	{ return m_generator; }

protected:
	void DoChannel(int i);

	///@brief Session handle so we can remove the PSU when closed
	Session* m_session;

	///@brief The generator we're controlling
	SCPIRFSignalGenerator* m_generator;

	///@brief UI state for each channel
	std::vector<RFGeneratorChannelUIState> m_uiState;

};



#endif
