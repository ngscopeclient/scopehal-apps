/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2023 Andrew D. Zonenberg                                                                          *
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
	@brief Declaration of BERTDialog
 */
#ifndef BERTDialog_h
#define BERTDialog_h

#include "Dialog.h"
#include "Session.h"

#include <future>

class BERTDialog : public Dialog
{
public:
	BERTDialog(SCPIBERT* bert, std::shared_ptr<BERTState> state, Session* session);
	virtual ~BERTDialog();

	virtual bool DoRender();

	SCPIBERT* GetBERT()
	{ return m_bert; }

protected:

	///@brief Session handle so we can remove the load when closed
	Session* m_session;

	///@brief Timestamp of when we opened the dialog
	double m_tstart;

	///@brief The BERT we're controlling
	SCPIBERT* m_bert;

	///@brief Current channel stats, live updated
	std::shared_ptr<BERTState> m_state;

	///@brief Set of channel names
	std::vector<std::string> m_channelNames;
};



#endif
