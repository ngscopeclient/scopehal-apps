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
	@brief Implementation of ProtocolAnalyzerDialog
 */

#include "ngscopeclient.h"
#include "ProtocolAnalyzerDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

ProtocolAnalyzerDialog::ProtocolAnalyzerDialog(
	PacketDecoder* filter, shared_ptr<PacketManager> mgr, Session& session, MainWindow& wnd)
	: Dialog(string("Protocol: ") + filter->GetDisplayName(), ImVec2(425, 350))
	, m_filter(filter)
	, m_mgr(mgr)
	, m_session(session)
	, m_parent(wnd)
	, m_rowHeight(0)
	, m_waveformChanged(false)
	, m_lastSelectedWaveform(0, 0)
	, m_selectedPacket(nullptr)
	, m_dataFormat(FORMAT_HEX)
	, m_needToScrollToSelectedPacket(false)
{
	//Hold a reference open to the filter so it doesn't disappear on us
	m_filter->AddRef();
}

ProtocolAnalyzerDialog::~ProtocolAnalyzerDialog()
{
	m_filter->Release();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool ProtocolAnalyzerDialog::DoRender()
{
	static ImGuiTableFlags flags =
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_BordersOuter |
		ImGuiTableFlags_BordersV |
		ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_SizingFixedFit;

	float width = ImGui::GetFontSize();

	auto cols = m_filter->GetHeaders();
	//TODO: hide certain headers like length and ascii?

	//Figure out channel setup
	//Default is timestamp plus all headers, add optional other channels as needed
	int ncols = 1 + cols.size();
	int datacol = 0;
	if(m_filter->GetShowDataColumn())
		datacol = (ncols ++);
	if(m_filter->GetShowImageColumn())
		ncols ++;
	//TODO: integrate length natively vs having to make the filter calculate it??

	auto dataFont = m_parent.GetFontPref("Appearance.Protocol Analyzer.data_font");

	//Output format for data column
	if(m_filter->GetShowDataColumn())
	{
		ImGui::SetNextItemWidth(10 * width);
		ImGui::Combo("Data Format", (int*)&m_dataFormat, "Hex\0ASCII\0Hexdump\0");
	}

	if(ImGui::BeginTable("table", ncols, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 12*width);
		for(auto c : cols)
			ImGui::TableSetupColumn(c.c_str(), ImGuiTableColumnFlags_WidthFixed, 0.0f);
		if(m_filter->GetShowDataColumn())
			ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		if(m_filter->GetShowImageColumn())
			ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		ImGui::TableHeadersRow();

		//Do an update cycle to make sure any recently acquired packets are captured
		m_mgr->Update();

		lock_guard lock(m_mgr->GetMutex());
		auto packets = m_mgr->GetPackets();

		//Make a list of waveform timestamps and make sure we display them in order
		vector<TimePoint> times;
		for(auto& it : packets)
			times.push_back(it.first);
		std::sort(times.begin(), times.end());

		//Process packets from each waveform
		for(auto wavetime : times)
		{
			//TODO: add some kind of marker to indicate gaps between waveforms (if we have >1)?
			ImGui::PushID(wavetime.first);
			ImGui::PushID(wavetime.second);

			auto& wpackets = packets[wavetime];
			for(auto pack : wpackets)
			{
				//Instead of using packet pointer as identifier (can change if filter graph re-runs for
				//unrelated reasons), use timestamp instead.
				ImGui::PushID(pack->m_offset);

				ImGui::TableNextRow(ImGuiTableRowFlags_None, m_rowHeight);

				//Set up colors for the packet
				ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ColorFromString(pack->m_displayBackgroundColor));
				ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(pack->m_displayForegroundColor));

				//See if we have child packets
				auto children = m_mgr->GetChildPackets(pack);
				bool hasChildren = !children.empty();

				//Timestamp (and row selection logic)
				ImGui::TableSetColumnIndex(0);
				bool open = false;
				if(hasChildren)
				{
					open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_OpenOnArrow);
					if(open)
						ImGui::TreePop();
				}
				ImGui::SameLine();
				bool rowIsSelected = (m_selectedPacket == pack);
				TimePoint packtime(wavetime.GetSec(), wavetime.GetFs() + pack->m_offset);
				if(ImGui::Selectable(
					packtime.PrettyPrint().c_str(),
					rowIsSelected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
					ImVec2(0, m_rowHeight)))
				{
					m_selectedPacket = pack;
					rowIsSelected = true;

					//See if a new waveform was selected
					if( (m_lastSelectedWaveform != TimePoint(0, 0)) && (m_lastSelectedWaveform != wavetime) )
						m_waveformChanged = true;
					m_lastSelectedWaveform = wavetime;

					m_parent.NavigateToTimestamp(pack->m_offset, pack->m_len, StreamDescriptor(m_filter, 0));
				}

				//Update scroll position if requested
				if(rowIsSelected && m_needToScrollToSelectedPacket)
				{
					m_needToScrollToSelectedPacket = false;
					ImGui::SetScrollHereY();
				}

				//Headers
				for(size_t i=0; i<cols.size(); i++)
				{
					if(ImGui::TableSetColumnIndex(i+1))
						ImGui::TextUnformatted(pack->m_headers[cols[i]].c_str());
				}

				//Data column
				if(m_filter->GetShowDataColumn())
					DoDataColumn(datacol, pack, dataFont);

				//Child nodes for merged packets
				if(open)
				{
					ImGui::TreePush("##tree");

					for(auto child : children)
					{
						//Instead of using packet pointer as identifier (can change if filter graph re-runs for
						//unrelated reasons), use timestamp instead.
						ImGui::PushID(child->m_offset);

						ImGui::TableNextRow(ImGuiTableRowFlags_None, m_rowHeight);

						//Set up colors for the packet
						ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ColorFromString(child->m_displayBackgroundColor));
						ImGui::PushStyleColor(ImGuiCol_Text, ColorFromString(child->m_displayForegroundColor));

						bool childIsSelected = (m_selectedPacket == child);

						ImGui::TableSetColumnIndex(0);
						TimePoint ctime(wavetime.GetSec(), wavetime.GetFs() + child->m_offset);
						if(ImGui::Selectable(
							ctime.PrettyPrint().c_str(),
							childIsSelected,
							ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
							ImVec2(0, m_rowHeight)))
						{
							m_selectedPacket = child;
							childIsSelected = true;

							//See if a new waveform was selected
							if( (m_lastSelectedWaveform != TimePoint(0, 0)) && (m_lastSelectedWaveform != wavetime) )
								m_waveformChanged = true;
							m_lastSelectedWaveform = wavetime;

							m_parent.NavigateToTimestamp(child->m_offset, child->m_len, StreamDescriptor(m_filter, 0));
						}

						//Update scroll position if requested
						if(rowIsSelected && m_needToScrollToSelectedPacket)
						{
							m_needToScrollToSelectedPacket = false;
							ImGui::SetScrollHereY();
						}

						//Headers
						for(size_t i=0; i<cols.size(); i++)
						{
							if(ImGui::TableSetColumnIndex(i+1))
								ImGui::TextUnformatted(child->m_headers[cols[i]].c_str());
						}

						//Data column
						if(m_filter->GetShowDataColumn())
							DoDataColumn(datacol, child, dataFont);

						ImGui::PopStyleColor();
						ImGui::PopID();
					}

					ImGui::TreePop();
				}

				ImGui::PopStyleColor();
				ImGui::PopID();
			}

			ImGui::PopID();
			ImGui::PopID();
		}

		ImGui::EndTable();
	}
	return true;
}

/**
	@brief Handles the "data" column for packets
 */
void ProtocolAnalyzerDialog::DoDataColumn(int datacol, Packet* pack, ImFont* dataFont)
{
	size_t bytesPerLine = 1;
	switch(m_dataFormat)
	{
		case FORMAT_HEX:
			bytesPerLine = 16;
			break;

		case FORMAT_ASCII:
			bytesPerLine = 32;
			break;

		case FORMAT_HEXDUMP:
			bytesPerLine = 8;
			break;
	}

	if(ImGui::TableSetColumnIndex(datacol))
	{
		string firstLine;

		auto& bytes = pack->m_data;

		string lineHex;
		string lineAscii;

		//Format the data
		string data;
		char tmp[32];
		for(size_t i=0; i<bytes.size(); i++)
		{
			if( (i % bytesPerLine) == 0)
			{
				snprintf(tmp, sizeof(tmp), "%04zx ", i);
				data += tmp;
			}

			switch(m_dataFormat)
			{
				case FORMAT_HEX:
					snprintf(tmp, sizeof(tmp), "%02x ", bytes[i]);
					data += tmp;
					break;

				case FORMAT_ASCII:
					if(isprint(bytes[i]) || (bytes[i] == ' '))
						data += bytes[i];
					else
						data += '.';
					break;

				case FORMAT_HEXDUMP:

					//hex dump
					snprintf(tmp, sizeof(tmp), "%02x ", bytes[i]);
					lineHex += tmp;

					//ascii
					if(isprint(bytes[i]) || (bytes[i] == ' '))
						lineAscii += bytes[i];
					else
						lineAscii += '.';
					break;
			}

			if( (i % bytesPerLine) == bytesPerLine-1)
			{
				//Special processing for hex dump
				if(m_dataFormat == FORMAT_HEXDUMP)
				{
					data += lineHex + "   " + lineAscii;
					lineHex = "";
					lineAscii = "";
				}

				if(firstLine.empty())
				{
					firstLine = data;
					data = "";
				}
				else
					data += "\n";
			}
		}

		//Handle data less than one line in size
		if(firstLine.empty() && !data.empty())
		{
			firstLine = data;
			data = "";
		}

		if(m_dataFormat == FORMAT_HEXDUMP)
		{
			//process last partial line at end
			if(!lineHex.empty())
			{
				while(lineHex.length() < 3*bytesPerLine)
					lineHex += ' ';

				data += lineHex + "   " + lineAscii;
			}
		}

		if(!firstLine.empty())
		{
			ImGui::PushFont(dataFont);

			//If we have multiple data lines, show tree
			if(!data.empty())
			{
				firstLine += "##data";
				if(ImGui::TreeNodeEx(firstLine.c_str(), ImGuiTreeNodeFlags_OpenOnArrow))
				{
					ImGui::TextUnformatted(data.c_str());
					ImGui::TreePop();
				}
			}

			//If just one line, print it
			else
				ImGui::TextUnformatted(firstLine.c_str());

			ImGui::PopFont();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI event handlers

/**
	@brief Notifies the dialog that a cursor has been moved
 */
void ProtocolAnalyzerDialog::OnCursorMoved(int64_t offset)
{
	//If nothing is selected, use our current waveform timestamp as a reference
	if(m_lastSelectedWaveform == TimePoint(0, 0))
	{
		auto data = m_filter->GetData(0);
		m_lastSelectedWaveform = TimePoint(data->m_startTimestamp, data->m_startFemtoseconds);
	}

	auto& allpackets = m_mgr->GetPackets();
	auto it = allpackets.find(m_lastSelectedWaveform);
	if(it == allpackets.end())
		return;
	auto packets = it->second;

	//TODO: binary search vs linear
	for(auto p : packets)
	{
		//Check child packets first
		auto& children = m_mgr->GetChildPackets(p);
		for(auto c : children)
		{
			if(offset > (c->m_offset + c->m_len) )
				continue;
			if(c->m_offset > offset)
				return;

			m_selectedPacket = c;
			m_needToScrollToSelectedPacket = true;
			return;
		}

		//If we get here no child hit, try to match parent
		if(offset > (p->m_offset + p->m_len) )
			continue;
		if(p->m_offset > offset)
			return;

		m_selectedPacket = p;
		m_needToScrollToSelectedPacket = true;
		return;
	}
}
