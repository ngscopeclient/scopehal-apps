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
	@brief Declaration of BERTInputChannelDialog
 */
#ifndef BERTInputChannelDialog_h
#define BERTInputChannelDialog_h

#include "EmbeddableDialog.h"

class MainWindow;

class BERTInputChannelDialog : public EmbeddableDialog
{
public:
	BERTInputChannelDialog(BERTInputChannel* chan, MainWindow* parent, bool graphEditorMode = false);
	virtual ~BERTInputChannelDialog();

	virtual bool Render() override;
	virtual bool DoRender() override;

	BERTInputChannel* GetChannel()
	{ return m_channel; }

	void RunFileDialog();

protected:

	BERTInputChannel* m_channel;
	MainWindow* m_parent;

	bool m_invert;

	int m_patternIndex;
	std::vector<std::string> m_patternNames;
	std::vector<BERT::Pattern> m_patternValues;

	int m_scanIndex;
	std::vector<std::string> m_scanNames;
	std::vector<int64_t> m_scanValues;

	int m_ctleIndex;
	std::vector<std::string> m_ctleNames;

	std::string m_displayName;
	std::string m_committedDisplayName;

	std::string m_tempMaskFile;
	std::string m_committedMaskFile;

	float m_sampleX;
	float m_committedSampleX;
	float m_sampleY;
	float m_committedSampleY;

	///@brief Data rate selector
	int m_dataRateIndex;
	std::vector<int64_t> m_dataRates;
	std::vector<std::string> m_dataRateNames;

	float m_color[3];

	std::shared_ptr<FileBrowser> m_fileDialog;
};

#endif
