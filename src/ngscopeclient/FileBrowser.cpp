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
	@brief Implementation of FileBrowser
 */
#include "ngscopeclient.h"
#include "FileBrowser.h"
#include "MainWindow.h"
#include "IGFDFileBrowser.h"
#include "NFDFileBrowser.h"
#include "PreferenceTypes.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FileBrowser::FileBrowser()
{
}

FileBrowser::~FileBrowser()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Factory methods

/**
	@brief Helper function to create the correct FileBrowser based on user preferences
 */
shared_ptr<FileBrowser> MakeFileBrowser(
	MainWindow* wnd,
	const string& initialPath,
	const string& title,
	const string& filterName,
	const string& filterMask,
	bool saveDialog)
{
	auto pref = wnd->GetSession().GetPreferences().GetEnumRaw(
		"Appearance.File Browser.dialogmode");
	
#ifdef _APPLE_SILICON     // only the native dialog works. NFDFileBrowser crashes on MacOS
	pref = BROWSER_IMGUI;  
#endif

	//Fullscreen mode overrides preferences and forces use of imgui browser
	if( (pref == BROWSER_IMGUI) || wnd->IsFullscreen() )
	{
		return make_shared<IGFDFileBrowser>(
			initialPath,
			title,
			"FileChooser",
			filterName,
			filterMask,
			saveDialog);
	}
	else
	{
		return make_shared<NFDFileBrowser>(
			initialPath,
			title,
			filterName,
			filterMask,
			saveDialog);
	}
}
