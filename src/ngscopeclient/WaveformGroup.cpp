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
	@brief Implementation of WaveformGroup
 */
#include "ngscopeclient.h"
#include "WaveformGroup.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

WaveformGroup::WaveformGroup(const string& title)
	: m_title(title)
{
}

WaveformGroup::~WaveformGroup()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Area management

void WaveformGroup::AddArea(shared_ptr<WaveformArea>& area)
{
	m_areas.push_back(area);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool WaveformGroup::Render()
{
	bool open = true;
	ImGui::SetNextWindowSize(ImVec2(320, 240), ImGuiCond_Appearing);
	if(!ImGui::Begin(m_title.c_str(), &open))
	{
		ImGui::End();
		return false;
	}

	ImVec2 clientArea = ImGui::GetContentRegionAvail();

	//Render our waveform areas
	vector<size_t> areasToClose;
	for(size_t i=0; i<m_areas.size(); i++)
	{
		if(!m_areas[i]->Render(i, m_areas.size(), clientArea))
			areasToClose.push_back(i);
	}

	//Close any areas that are now empty
	for(ssize_t i=static_cast<ssize_t>(areasToClose.size()) - 1; i >= 0; i--)
		m_areas.erase(m_areas.begin() + areasToClose[i]);

	//If we no longer have any areas in the group, close the group
	if(m_areas.empty())
		open = false;

	ImGui::End();
	return open;
}
