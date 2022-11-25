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
	@brief Implementation of IGFDFileBrowser
 */
#include "ngscopeclient.h"
#include "IGFDFileBrowser.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

IGFDFileBrowser::IGFDFileBrowser(
	const string& initialPath,
	const string& title,
	const string& id,
	const string& filterName,
	const string& filterMask,
	bool saveDialog
	)
	: m_closed(false)
	, m_closedOK(false)
	, m_id(id)
{
	//If linux read ~/.config/gtk-3.0/bookmarks
	//TODO: read bookmarks on other OSes
	#ifdef __linux__
		string home = getenv("HOME");
		string path = home + "/.config/gtk-3.0/bookmarks";
		FILE* fp = fopen(path.c_str(), "r");
		if(fp)
		{
			char line[1024];
			char fname[512] = "";
			char bname[512] = "";
			while(fgets(line, sizeof(line), fp) != nullptr)
			{
				auto sline = Trim(line);
				auto nfields = sscanf(sline.c_str(), "file://%511[^ ] %511s", fname, bname);
				if(nfields == 2)
					m_bookmarks[fname] = bname;
				else if(nfields == 1)
					m_bookmarks[fname] = BaseName(fname);
			}
			fclose(fp);
		}
	#endif

	//Tweak the mask for imgui filedialog
	//(needs to be in parentheses to be recognized as a regex)
	//Special case for touchstone since internal parentheses aren't well supported by IGFD
	string mask;
	if(filterMask == "*.s*p")
		mask = "Touchstone files (*.s*p){.s2p,.s3p,.s4p,.s5p,.s6p,.s7p,.s8p,.s9p,.snp}";
	else
		mask = filterName + "{" + filterMask.substr(1) + "}";

	for(auto jt : m_bookmarks)
		m_dialog.AddBookmark(jt.second, jt.first);
	if(saveDialog)
	{
		m_dialog.OpenDialog(
			m_id,
			title,
			mask.c_str(),
			".",
			initialPath,
			ImGuiFileDialogFlags_ConfirmOverwrite);
	}
	else
	{
		m_dialog.OpenDialog(
			m_id,
			title,
			mask.c_str(),
			".",
			initialPath);
	}
}

IGFDFileBrowser::~IGFDFileBrowser()
{
	//TODO: save bookmarks at exit
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI handlers

void IGFDFileBrowser::Render()
{
	if(m_closed)
		return;

	float fontsize = ImGui::GetFontSize();
	if(m_dialog.Display(m_id, ImGuiWindowFlags_NoCollapse, ImVec2(60*fontsize, 30*fontsize)))
	{
		if(m_dialog.IsOk())
			m_closedOK = true;
		m_closed = true;
	}
}

bool IGFDFileBrowser::IsClosed()
{
	return m_closed;
}

bool IGFDFileBrowser::IsClosedOK()
{
	return m_closedOK;
}

string IGFDFileBrowser::GetFileName()
{
	return m_dialog.GetFilePathName();
}
