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
	: Dialog(
		string("Protocol: ") + filter->GetDisplayName(),
		string("Protocol: ") + filter->GetHwname(),
		ImVec2(425, 350))
	, m_filter(filter)
	, m_mgr(mgr)
	, m_session(session)
	, m_parent(wnd)
	, m_waveformChanged(false)
	, m_lastSelectedWaveform(0, 0)
	, m_selectedPacket(nullptr)
	, m_dataFormat(FORMAT_HEX)
	, m_needToScrollToSelectedPacket(false)
	, m_firstDataBlockOfFrame(true)
	, m_bytesPerLine(1)
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

void ProtocolAnalyzerDialog::SetFilterExpression(const string& f)
{
	m_filterExpression = f;
	m_committedFilterExpression = f;

	auto cols = m_filter->GetHeaders();
	size_t ifilter = 0;
	auto pfilter = make_shared<ProtocolDisplayFilter>(f, ifilter);
	if(pfilter->Validate(cols))
		m_mgr->SetDisplayFilter(pfilter);
}

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
	auto& prefs = m_parent.GetSession().GetPreferences();

	//Figure out color for filter expression
	ImU32 bgcolor;
	size_t ifilter = 0;
	ProtocolDisplayFilter filter(m_filterExpression, ifilter);
	if(m_filterExpression == "")
		bgcolor = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);
	else if(filter.Validate(cols))
		bgcolor = ColorFromString("#008000");
	else
		bgcolor = ColorFromString("#800000");
	//TODO: yellow for possibly wrong stuff?
	//TODO: allow configuration under preferences

	//Filter expression
	float boxwidth = ImGui::GetContentRegionAvail().x;
	ImGui::SetNextItemWidth(boxwidth - ImGui::CalcTextSize("Filter").x - ImGui::GetStyle().ItemSpacing.x);
	ImGui::PushStyleColor(ImGuiCol_FrameBg, bgcolor);
	ImGui::InputText("Filter", &m_filterExpression);
	bool updated = !ImGui::IsItemActive();
	bool filterDirty = (m_committedFilterExpression != m_filterExpression);
	ImGui::PopStyleColor();

	//Display tooltip for filter state
	if(ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
	{
		size_t itotal = 0;
		size_t idisplayed = 0;
		{
			lock_guard<recursive_mutex> lock(m_mgr->GetMutex());

			auto& packets = m_mgr->GetPackets();
			for(auto it : packets)
				itotal += it.second.size();

			auto& filt = m_mgr->GetFilteredPackets();
			for(auto it : filt)
				idisplayed += it.second.size();
		}
		char stmp[128];
		snprintf(stmp, sizeof(stmp), "%zu / %zu packets displayed (%.2f %%)\n",
			idisplayed, itotal, idisplayed * 100.0 / itotal);

		ImGui::BeginTooltip();
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 50);
		ImGui::TextUnformatted(stmp);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}

	//Output format for data column
	//If this is changed force a refresh
	bool forceRefresh = false;
	if(m_filter->GetShowDataColumn())
	{
		ImGui::SetNextItemWidth(10 * width);
		if(ImGui::Combo("Data Format", (int*)&m_dataFormat, "Hex\0ASCII\0Hexdump\0"))
			forceRefresh = true;
	}

	//Do an update cycle to make sure any recently acquired packets are captured
	m_mgr->Update();

	lock_guard<recursive_mutex> lock(m_mgr->GetMutex());
	auto& rows = m_mgr->GetRows();

	m_firstDataBlockOfFrame = true;
	if(!rows.empty() && ImGui::BeginTable("table", ncols, flags))
	{
		ImGui::TableSetupScrollFreeze(0, 1); //Header row does not scroll
		ImGui::TableSetupColumn("Timestamp", ImGuiTableColumnFlags_WidthFixed, 12*width);
		for(auto c : cols)
			ImGui::TableSetupColumn(c.c_str(), ImGuiTableColumnFlags_WidthFixed, 0.0f);
		if(m_filter->GetShowDataColumn())
			ImGui::TableSetupColumn("Data", ImGuiTableColumnFlags_WidthStretch, 0.0f);
		if(m_filter->GetShowImageColumn())
			ImGui::TableSetupColumn("Image", ImGuiTableColumnFlags_WidthFixed, 0.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin((int)rows.back().m_totalHeight, 1.0f);

		//see https://github.com/ocornut/imgui/issues/6042
		// hacky way to disable clipper.Step() submitting a range for an offscreen row that has focus
		ImGuiContext& g = *ImGui::GetCurrentContext();
		ImGuiID navId = g.NavId;
		g.NavId = 0;

		//TODO: add some kind of marker to indicate gaps between waveforms (if we have >1)?
		//(need to make sure this works with culling etc)

		//Go through the rows and render them, culling anything offscreen
		bool visibleRowSelected = false;
		while(clipper.Step())
		{
			double minY = (double)clipper.DisplayStart;
			double maxY = (double)clipper.DisplayEnd;

			const auto sit = std::lower_bound(
				rows.begin(),
				rows.end(),
				minY,
				[](const RowData& data, double f) { return f > data.m_totalHeight; });
			size_t istart = sit - rows.begin();

			for (size_t i = istart; i < rows.size() && (!i || maxY > rows[i - 1].m_totalHeight); i++)
			{
				auto& row = rows[i];

				ImGui::PushID(row.m_stamp.first);
				ImGui::PushID(row.m_stamp.second);

				//Is it a packet?
				auto pack = row.m_packet;

				//Make sure we have the packed colors cached
				if(pack)
					pack->RefreshColors();

				//Instead of using packet pointer as identifier (can change if filter graph re-runs for
				//unrelated reasons), use timestamp instead.
				if(pack)
					ImGui::PushID(pack->m_offset);
				else
				{
					ImGui::PushID(row.m_marker.m_offset);
					ImGui::PushID("Marker");
				}

				ImGui::TableNextRow(ImGuiTableRowFlags_None);

				//Set up colors for the packet
				if(pack)
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, pack->m_displayBackgroundColorPacked);
					ImGui::PushStyleColor(ImGuiCol_Text, pack->m_displayForegroundColorPacked);
				}
				else
				{
					ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, prefs.GetColor("Appearance.Graphs.bottom_color"));
					ImGui::PushStyleColor(ImGuiCol_Text, prefs.GetColor("Appearance.Cursors.marker_color"));
				}

				//See if we have child packets
				bool hasChildren = false;
				if(pack)
				{
					auto children = m_mgr->GetFilteredChildPackets(pack);
					hasChildren = !children.empty();
				}

				float rowStart = rows[i].m_totalHeight - rows[i].m_height;
				bool firstRow = (i == istart);

				//Timestamp (and row selection logic)
				ImGui::TableSetColumnIndex(0);
				if(firstRow)
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetScrollY() - rowStart));
				bool open = false;
				if(hasChildren)
				{
					open = ImGui::TreeNodeEx("##tree", ImGuiTreeNodeFlags_OpenOnArrow);

					if(m_mgr->IsChildOpen(pack) != open)
					{
						m_mgr->SetChildOpen(pack, open);
						LogTrace("tree node opened or closed, forcing refresh\n");
						forceRefresh = true;
					}

					if(open)
						ImGui::TreePop();
					ImGui::SameLine();
				}

				//TODO allow selection of marker
				int64_t offset = 0;
				int64_t len = 0;
				if(pack)
				{
					offset = pack->m_offset;
					len = pack->m_len;
				}
				else
					offset = row.m_marker.m_offset;
				bool rowIsSelected = pack && (m_selectedPacket == pack);
				TimePoint packtime(row.m_stamp.GetSec(), row.m_stamp.GetFs() + offset);

				if(ImGui::Selectable(
					packtime.PrettyPrint().c_str(),
					rowIsSelected,
					ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowItemOverlap,
					ImVec2(0, 0)))
				{
					m_selectedPacket = pack;
					rowIsSelected = true;
					visibleRowSelected = true;

					//See if a new waveform was selected
					if( (m_lastSelectedWaveform != TimePoint(0, 0)) && (m_lastSelectedWaveform != row.m_stamp) )
						m_waveformChanged = true;
					m_lastSelectedWaveform = row.m_stamp;

					m_parent.NavigateToTimestamp(offset, len, StreamDescriptor(m_filter, 0));
				}

				if(pack)
				{
					//Headers
					for(size_t j=0; j<cols.size(); j++)
					{
						if(ImGui::TableSetColumnIndex(j+1))
						{
							if(firstRow)
								ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetScrollY() - rowStart));

							ImGui::TextUnformatted(pack->m_headers[cols[j]].c_str());
						}
					}

					//Data column
					if(m_filter->GetShowDataColumn())
					{
						if(ImGui::TableSetColumnIndex(datacol))
						{
							if(firstRow)
								ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetScrollY() - rowStart));

							DoDataColumn(pack, dataFont, rows, i);
						}
					}
				}

				//Marker name
				else
				{
					//TODO: which column to use for marker text??)
					if(m_filter->GetShowDataColumn())
					{
						if(ImGui::TableSetColumnIndex(datacol))
						{
							if(firstRow)
								ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (ImGui::GetScrollY() - rowStart));
							ImGui::TextUnformatted(row.m_marker.m_name.c_str());
						}
					}
				}

				ImGui::PopStyleColor();
				if(!pack)
					ImGui::PopID();
				ImGui::PopID();
				ImGui::PopID();
				ImGui::PopID();
			}
		}

		//Only scroll if requested packet is off screen
		if(m_needToScrollToSelectedPacket && !visibleRowSelected)
		{
			//Go through our visible rows to find the closest packet
			//(may not be the selected one we're just trying to scroll to that general area)
			const auto sit = std::lower_bound(
				rows.begin(),
				rows.end(),
				m_selectedPacket->m_offset,
				[](const RowData& data, double f)
					{ return f > (data.m_packet? data.m_packet->m_offset : data.m_marker.m_offset); });
			auto& row = *sit;
			ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y + row.m_totalHeight);

			m_needToScrollToSelectedPacket = false;
		}

		ImGui::EndTable();

		g.NavId = navId;
	}

	//Apply filter expressions
	if( (updated && filterDirty) || forceRefresh)
	{
		if(!forceRefresh)
			m_committedFilterExpression = m_filterExpression;

		//No filter expression? Nothing to do
		if(m_filterExpression == "")
			m_mgr->SetDisplayFilter(nullptr);
		else
		{
			//Parse the expression. Apply only if valid
			//If not valid, keep old filter active
			ifilter = 0;
			auto pfilter = make_shared<ProtocolDisplayFilter>(m_filterExpression, ifilter);
			if(pfilter->Validate(cols))
				m_mgr->SetDisplayFilter(pfilter);
		}
	}

	return true;
}

/**
	@brief Handles the "data" column for packets
 */
void ProtocolAnalyzerDialog::DoDataColumn(Packet* pack, ImFont* dataFont, vector<RowData>& rows, size_t nrow)
{
	//When drawing the first cell, figure out dimensions for subsequent stuff
	if(m_firstDataBlockOfFrame)
	{
		//Available space (after subtracting tree button)
		auto xsize = ImGui::GetContentRegionAvail().x - ImGui::GetStyle().IndentSpacing;

		//Figure out how many characters of text we can fit in the data region
		//This assumes data font is fixed width, may break if user chooses variable width.
		//But hex dumps with variable width will look horrible anyway so that's probably not a problem?
		auto fontwidth = dataFont->CalcTextSizeA(dataFont->FontSize * ImGui::GetIO().FontGlobalScale, FLT_MAX, -1, "W").x;
		size_t charsPerLine = floor(xsize / fontwidth);

		//TODO: use 2-nibble address if packet has <256 bytes of data

		//Number of characters available for displaying data (address column doesn't count)
		size_t dataCharsPerLine = charsPerLine - 5;

		switch(m_dataFormat)
		{
			//Ascii is trivial: data bytes map 1:1 to characters
			case FORMAT_ASCII:
				m_bytesPerLine = dataCharsPerLine;
				break;

			//Hex needs three chars (2 hex + space)
			//TODO: last char doesn't need the space
			case FORMAT_HEX:
				m_bytesPerLine = dataCharsPerLine / 3;
				break;

			//Hexdump needs a fixed 3 spaces between the hex and the ascii parts.
			//Then we need 3 for each hex and one for each ascii.
			case FORMAT_HEXDUMP:
				m_bytesPerLine = (dataCharsPerLine - 3) / 4;
				break;
		}

		if(m_bytesPerLine <= 0)
			return;
	}

	string firstLine;

	auto& bytes = pack->m_data;

	string lineHex;
	string lineAscii;

	//Create the tree node early - before we've even rendered any data - so we know the open / closed state
	ImGui::PushFont(dataFont);
	bool open = false;
	if(!bytes.empty())
	{
		//If we have more than one line worth of data, show the tree
		if(bytes.size() > m_bytesPerLine)
		{
			open = ImGui::TreeNodeEx("##data", ImGuiTreeNodeFlags_OpenOnArrow);
			ImGui::SameLine();
		}
	}

	//Format the data
	string data;
	char tmp[32];
	for(size_t i=0; i<bytes.size(); i++)
	{
		//Address block
		if( (i % m_bytesPerLine) == 0)
		{
			//Is this the first block of an open tree view? Show address
			if(open)
			{
				snprintf(tmp, sizeof(tmp), "%04zx ", i);
				data += tmp;
			}

			//Tree closed or single line: don't show the 0000 which can be confused with data
			else
				data += "     ";
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

		if( (i % m_bytesPerLine) == m_bytesPerLine-1)
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
			while(lineHex.length() < 3*m_bytesPerLine)
				lineHex += ' ';

			data += lineHex + "   " + lineAscii;
		}
	}

	ImGui::TextUnformatted(firstLine.c_str());

	//Multiple lines? Only show if open
	if(open)
	{
		ImGui::TextUnformatted(data.c_str());
		ImGui::TreePop();
	}

	ImGui::PopFont();
	m_firstDataBlockOfFrame = false;

	//Recompute height of THIS cell and apply changes if we've expanded
	double padding = ImGui::GetStyle().CellPadding.y;
	double height = padding*2 + ImGui::CalcTextSize(firstLine.c_str()).y;
	if(open)
		height += ImGui::CalcTextSize(data.c_str()).y;
	double oldheight = rows[nrow].m_height;
	double delta = height - oldheight;
	if(abs(delta) > 0.001)
	{
		//Apply the changed height
		rows[nrow].m_height = height;

		//Move every impacted row up or down as appropriate
		for(size_t i=nrow; i<rows.size(); i++)
			rows[i].m_totalHeight += delta;
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

	auto& allpackets = m_mgr->GetFilteredPackets();
	auto it = allpackets.find(m_lastSelectedWaveform);
	if(it == allpackets.end())
		return;
	auto packets = it->second;

	//TODO: binary search vs linear
	for(auto p : packets)
	{
		//Check child packets first
		auto& children = m_mgr->GetFilteredChildPackets(p);
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
