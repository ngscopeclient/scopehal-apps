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
	@brief Implementation of PreferenceDialog
 */

#include "ngscopeclient.h"
#include "PreferenceDialog.h"
#include "PreferenceManager.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PreferenceDialog::PreferenceDialog(PreferenceManager& prefs)
	: Dialog("Preferences", ImVec2(600, 400))
	, m_prefs(prefs)
{

}

PreferenceDialog::~PreferenceDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

/**
	@brief Renders the dialog and handles UI events

	@return		True if we should continue showing the dialog
				False if it's been closed
 */
bool PreferenceDialog::DoRender()
{
	auto& root = m_prefs.AllPreferences();
	auto& children = root.GetChildren();

	//Top level uses collapsing headers
	for(const auto& identifier: root.GetOrdering())
	{
		auto& node = children[identifier];

		if(node->IsCategory())
		{
			auto& subCategory = node->AsCategory();
			if(subCategory.IsVisible())
			{
				if(ImGui::CollapsingHeader(identifier.c_str()))
					ProcessCategory(subCategory);
			}
		}
	}

	return true;
}

/**
	@brief Run the UI for a category, including any subcategories or preferences
 */
void PreferenceDialog::ProcessCategory(PreferenceCategory& cat)
{
	auto& children = cat.GetChildren();
	for(const auto& identifier: cat.GetOrdering())
	{
		auto& node = children[identifier];

		//Add child categories
		if(node->IsCategory())
		{
			auto& subCategory = node->AsCategory();

			if(subCategory.IsVisible())
			{
				if(ImGui::TreeNode(identifier.c_str()))
				{
					ProcessCategory(subCategory);
					ImGui::TreePop();
				}
			}
		}

		//Add preference widgets
		if(node->IsPreference())
			ProcessPreference(node->AsPreference());
	}
}

/**
	@brief Run the UI for a single preference
 */
void PreferenceDialog::ProcessPreference(Preference& pref)
{
	string label = pref.GetLabel() + "###" + pref.GetIdentifier();

	switch(pref.GetType())
	{
		//Bool: show a checkbox
		case PreferenceType::Boolean:
			{
				bool b = pref.GetBool();
				if(ImGui::Checkbox(label.c_str(), &b))
					pref.SetBool(b);
				HelpMarker(pref.GetDescription());
			}
			break;

		//Enums: show a combo box
		case PreferenceType::Enum:
			{
				auto map = pref.GetMapping();
				auto names = map.GetNames();
				auto curValue = pref.ToString();

				//This is a bit ugly and slow, but works...
				int selection = 0;
				for(size_t i=0; i<names.size(); i++)
				{
					if(curValue == names[i])
					{
						selection = i;
						break;
					}
				}

				if(Combo(label.c_str(), names, selection))
					pref.SetEnumRaw(map.GetValue(names[selection]));

				HelpMarker(pref.GetDescription());
			}
			break;

		//Colors: show color chooser widget
		case PreferenceType::Color:
			{

			}
			break;

		default:
			ImGui::TextDisabled(
				"Unimplemented: %s = %s\n",
				pref.GetIdentifier().c_str(),
				pref.ToString().c_str());
			break;
	}
}
