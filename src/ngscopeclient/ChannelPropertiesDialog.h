/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of ChannelPropertiesDialog
 */
#ifndef ChannelPropertiesDialog_h
#define ChannelPropertiesDialog_h

#include "EmbeddableDialog.h"

class ChannelPropertiesDialog : public EmbeddableDialog
{
public:
	ChannelPropertiesDialog(OscilloscopeChannel* chan, bool graphEditorMode = false);
	virtual ~ChannelPropertiesDialog();

	virtual bool DoRender();

	OscilloscopeChannel* GetChannel()
	{ return m_channel; }

protected:
	OscilloscopeChannel* m_channel;

	void RefreshInputSettings(Oscilloscope* scope, size_t nchan);

	std::string m_displayName;
	std::string m_committedDisplayName;

	std::vector<std::string> m_offset;
	std::vector<float> m_committedOffset;

	std::vector<std::string> m_range;
	std::vector<float> m_committedRange;

	std::string m_threshold;
	float m_committedThreshold;

	std::string m_hysteresis;
	float m_committedHysteresis;

	std::string m_attenuation;
	float m_committedAttenuation;

	std::vector<std::string> m_couplingNames;
	std::vector<OscilloscopeChannel::CouplingType> m_couplings;
	int m_coupling;

	std::vector<std::string> m_bwlNames;
	std::vector<unsigned int> m_bwlValues;
	int m_bwl;

	std::vector<std::string> m_imuxNames;
	int m_imux;

	std::vector<std::string> m_modeNames;
	int m_mode;

	int m_navg;

	float m_color[3];

	bool m_inverted;

	std::string m_probe;
	bool m_canAutoZero;
	bool m_canDegauss;
	bool m_shouldDegauss;
	bool m_canAverage;
};

#endif
