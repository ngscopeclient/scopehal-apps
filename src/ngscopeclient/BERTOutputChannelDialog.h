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
	@brief Declaration of BERTOutputChannelDialog
 */
#ifndef BERTOutputChannelDialog_h
#define BERTOutputChannelDialog_h

#include "EmbeddableDialog.h"

class BERTOutputChannelDialog : public EmbeddableDialog
{
public:
	BERTOutputChannelDialog(BERTOutputChannel* chan, bool graphEditorMode = false);
	virtual ~BERTOutputChannelDialog();

	virtual bool DoRender();

	BERTOutputChannel* GetChannel()
	{ return m_channel; }

protected:
	BERTOutputChannel* m_channel;

	bool m_invert;
	bool m_enable;

	float m_precursor;
	float m_postcursor;

	int m_patternIndex;
	std::vector<std::string> m_patternNames;
	std::vector<BERT::Pattern> m_patternValues;

	int m_driveIndex;
	std::vector<std::string> m_driveNames;
	std::vector<float> m_driveValues;

	std::string m_displayName;
	std::string m_committedDisplayName;

	///@brief Data rate selector
	int m_dataRateIndex;
	std::vector<int64_t> m_dataRates;
	std::vector<std::string> m_dataRateNames;

	float m_color[3];
};

#endif
