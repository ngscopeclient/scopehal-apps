/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Implementation of NFDFileBrowser
 */
#include "ngscopeclient.h"
#include "NFDFileBrowser.h"
#include "MainWindow.h"
#include <nfd_glfw3.h>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

NFDFileBrowser::NFDFileBrowser(
	const string& initialPath,
	const string& title,
	const string& filterName,
	const string& filterMask,
	bool saveDialog,
	MainWindow* parent
	)
	: m_parent(parent)
	, m_initialPath(initialPath)
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

NFDFileBrowser::~NFDFileBrowser()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// UI handlers

optional<string> NFDFileBrowser::GetCachedResult()
{
	if(!m_cachedResultValid)
	{
		m_cachedResult = m_future.get();
		m_cachedResultValid = true;
	}

	return m_cachedResult;
}

void NFDFileBrowser::Render()
{
	//no action needed
}

bool NFDFileBrowser::IsClosed()
{
	if(m_cachedResultValid)
		return true;

	return (m_future.wait_for(0s) == future_status::ready);
}

bool NFDFileBrowser::IsClosedOK()
{
	if(IsClosed())
		return GetCachedResult().has_value();
	else
		return false;
}

string NFDFileBrowser::GetFileName()
{
	if(IsClosedOK())
		return GetCachedResult().value();
	else
		return "";
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Thread functions

optional<string> NFDFileBrowser::ThreadProc()
{
	if(NFD_Init() != NFD_OKAY)
	{
		LogError("NFD_Init() failed\n");
		return {};
	}

	nfdchar_t* outPath = nullptr;
	nfdu8filteritem_t filterItem = { m_filterName.c_str(), m_filterMask.c_str() };
	nfdresult_t result;
	if(m_saveDialog)
	{
		//Fill out arguments
		nfdsavedialogu8args_t args;
		memset(&args, 0, sizeof(args));

		args.filterList = &filterItem;
		args.filterCount = 1;
		args.defaultPath = nullptr;
		if(!NFD_GetNativeWindowFromGLFWWindow(m_parent->GetWindow(), &args.parentWindow))
			LogError("failed to get window handle\n");

		result = NFD_SaveDialogU8_With(&outPath, &args);
	}
	else
	{
		//Fill out arguments
		nfdopendialogu8args_t args;
		memset(&args, 0, sizeof(args));

		args.filterList = &filterItem;
		args.filterCount = 1;
		args.defaultPath = nullptr;
		if(!NFD_GetNativeWindowFromGLFWWindow(m_parent->GetWindow(), &args.parentWindow))
			LogError("failed to get window handle\n");

		//And run the dialog
		result = NFD_OpenDialogU8_With(&outPath, &args);
	}
	if(result == NFD_OKAY)
	{
		string ret = outPath;
		NFD_FreePath(outPath);
		NFD_Quit();
		return ret;
	}
	else
	{
		NFD_Quit();
		return {};
	}
}
