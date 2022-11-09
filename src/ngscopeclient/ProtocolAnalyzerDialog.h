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
	@brief Declaration of ProtocolAnalyzerDialog
 */
#ifndef ProtocolAnalyzerDialog_h
#define ProtocolAnalyzerDialog_h

#include "Dialog.h"
#include "Session.h"

#include "../scopehal/PacketDecoder.h"

class MainWindow;

/**
	@brief UI for the history system
 */
class ProtocolAnalyzerDialog : public Dialog
{
public:
	ProtocolAnalyzerDialog(PacketDecoder* filter, std::shared_ptr<PacketManager> mgr, Session& session, MainWindow& wnd);
	virtual ~ProtocolAnalyzerDialog();

	virtual bool DoRender();

	bool PollForSelectionChanges()
	{
		bool changed = m_selectionChanged;
		m_selectionChanged = false;
		return changed;
	}

	PacketDecoder* GetFilter()
	{ return m_filter; }

protected:
	PacketDecoder* m_filter;
	std::shared_ptr<PacketManager> m_mgr;
	Session& m_session;
	MainWindow& m_parent;

	///@brief Height of a row in the dialog
	float m_rowHeight;

	///@brief True if a new row in the dialog was selected this frame
	bool m_selectionChanged;
};

#endif
