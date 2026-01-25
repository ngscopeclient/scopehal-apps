/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2026 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of FilterGraphWorkspace
 */
#ifndef FilterGraphWorkspace_h
#define FilterGraphWorkspace_h

#include "Workspace.h"
class FilterGraphEditor;
class CreateFilterBrowser;
class MainWindow;

/**
	@brief Helper class for building the default filter graph editor workspace
 */
class FilterGraphWorkspace : public Workspace
{
public:
	FilterGraphWorkspace(
		Session& session,
		MainWindow* parent,
		std::shared_ptr<FilterGraphEditor> graphEditor,
		std::shared_ptr<CreateFilterBrowser> palette
		);
	virtual ~FilterGraphWorkspace()
	{}

protected:
	virtual void DoRender(ImGuiID id) override;

	bool m_firstRun;

	//Only valid on initial launch of the workspace, set to null after
	//TODO: maybe use weak pointers here?
	std::shared_ptr<FilterGraphEditor> m_graphEditor;
	std::shared_ptr<CreateFilterBrowser> m_palette;
};

#endif

