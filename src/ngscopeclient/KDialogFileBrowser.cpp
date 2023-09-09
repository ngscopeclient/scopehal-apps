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

#ifdef __linux__

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of KDialogFileBrowser
 */
#include "ngscopeclient.h"
#include "KDialogFileBrowser.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

KDialogFileBrowser::KDialogFileBrowser(
	const string& initialPath,
	const string& title,
	const string& filterName,
	const string& filterMask,
	bool saveDialog
	)
	: m_initialPath(initialPath)
	, m_title(title)
	, m_filterName(filterName)
	, m_filterMask(filterMask)
	, m_saveDialog(saveDialog)
	, m_cachedResultValid(false)
{
	//Trim off filter name
	size_t iparen = m_filterName.find('(');
	if(iparen != string::npos)
		m_filterName = m_filterName.substr(0, iparen);

	m_filterMask = filterMask.substr(2);

	m_future = async(launch::async, [this]{return ThreadProc(); } );
}

KDialogFileBrowser::~KDialogFileBrowser()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI handlers

optional<string> KDialogFileBrowser::GetCachedResult()
{
	if(!m_cachedResultValid)
	{
		m_cachedResult = m_future.get();
		m_cachedResultValid = true;
	}

	return m_cachedResult;
}

void KDialogFileBrowser::Render()
{
	//no action needed
}

bool KDialogFileBrowser::IsClosed()
{
	if(m_cachedResultValid)
		return true;

	return (m_future.wait_for(0s) == future_status::ready);
}

bool KDialogFileBrowser::IsClosedOK()
{
	if(IsClosed())
		return GetCachedResult().has_value();
	else
		return false;
}

string KDialogFileBrowser::GetFileName()
{
	if(IsClosedOK())
		return GetCachedResult().value();
	else
		return "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread functions

optional<string> KDialogFileBrowser::ThreadProc()
{
	string cmd = "XDG_CURRENT_DESKTOP=kde kdialog ";
	if(m_saveDialog)
		cmd += "--getsavefilename ";
	else
		cmd += "--getopenfilename ";
	cmd += string(" --title \"") + m_title + "\" ";
	cmd += string("\"") + m_initialPath + "\" ";
	cmd += string("\"") + m_filterName + "(*." + m_filterMask + ")\" ";
	LogDebug("Final command: %s\n", cmd.c_str());
	FILE* fp = popen(cmd.c_str(), "r");

	char tmp[1024] = {0};
	if(!fgets(tmp, sizeof(tmp), fp))
	{
		pclose(fp);
		return {};
	}

	pclose(fp);
	return Trim(tmp);
}

#endif
