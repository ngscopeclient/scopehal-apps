/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2025 Andrew D. Zonenberg and contributors                                                         *
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
	@brief Declaration of PreferenceDialog
 */
#ifndef PreferenceDialog_h
#define PreferenceDialog_h

#include "Dialog.h"

class PreferenceManager;
class PreferenceCategory;
class Preference;

class PreferenceDialog : public Dialog
{
public:
	PreferenceDialog(PreferenceManager& prefs);
	virtual ~PreferenceDialog();

	virtual bool DoRender();

protected:
	void ProcessCategory(PreferenceCategory& cat);
	void ProcessPreference(Preference& pref);
	bool DefaultButton(const std::string& label, const std::string& id, bool centered = false);
	void ResetCategoryToDefault(PreferenceCategory& cat);
	void OpenConfirmDialog(const std::string& title, const std::string& message, const std::string& identifier);
	bool RenderConfirmDialog(const std::string& identifier);

	PreferenceManager& m_prefs;

	std::vector<std::string> m_fontPaths;
	std::vector<std::string> m_fontShortNames;
	std::map<std::string, size_t> m_fontReverseMap;

	std::string m_confirmDialogTitle;
	std::string m_confirmDialogMessage;

	void FindFontFiles(const std::string& path);

	//Temporary values for preferences that we're still configuring
	std::map<std::string, std::string> m_preferenceTemporaries;
};

#endif
