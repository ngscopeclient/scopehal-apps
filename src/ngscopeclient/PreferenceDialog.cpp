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

/**
	@file
	@author Andrew D. Zonenberg
	@brief Implementation of PreferenceDialog
 */

#include "ngscopeclient.h"
#include "PreferenceDialog.h"
#include "PreferenceManager.h"
#include "../../lib/scopehal/FileSystem.h"

#include <regex>

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

PreferenceDialog::PreferenceDialog(PreferenceManager& prefs)
	: Dialog("Preferences", "Preferences", ImVec2(600, 400))
	, m_prefs(prefs)
{
	m_fontPaths.push_back(FindDataFile("fonts/DejaVuSans.ttf"));
	m_fontPaths.push_back(FindDataFile("fonts/DejaVuSansMono.ttf"));
	m_fontPaths.push_back(FindDataFile("fonts/DejaVuSans-Bold.ttf"));

#ifdef _WIN32
	FindFontFiles("C:\\Windows\\Fonts");
#elif __APPLE__
	FindFontFiles("/System/Library/Fonts");
	FindFontFiles("/Library/Fonts");
	FindFontFiles("~/Library/Fonts");
#else
	FindFontFiles("/usr/share/fonts");
	FindFontFiles("/usr/local/share/fonts");
	FindFontFiles("~/.local/share/fonts");
#endif

	sort(begin(m_fontPaths), end(m_fontPaths), [](string &a, string &b) {
		auto f1 = BaseName(a);
		auto f2 = BaseName(b);
		return lexicographical_compare(f1.begin(),f1.end(),f2.begin(),f2.end());
	});

	//Get short names for each file
	for(size_t i=0; i<m_fontPaths.size(); i++)
	{
		auto path = m_fontPaths[i];
		auto shortname = BaseName(path);
		shortname = shortname.substr(0, shortname.length() - 4);	//trim off extension

		m_fontShortNames.push_back(shortname);
		m_fontReverseMap[path] = i;
	}
}

PreferenceDialog::~PreferenceDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Font search

void PreferenceDialog::FindFontFiles(const string& path)
{
	auto files = Glob(path + "/*", false);
	regex fontfile_regex(R"(.[oOtT][tT][cCfF])");
	for(string f : files)
	{
		if(regex_search(f, fontfile_regex))
		{
			m_fontPaths.push_back(f);
		}

		else if(f.find(".") == string::npos)
			FindFontFiles(f);
	}
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

				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
				if(Combo(label.c_str(), names, selection))
					pref.SetEnumRaw(map.GetValue(names[selection]));
			}
			break;

		//Colors: show color chooser widget
		case PreferenceType::Color:
			{
				auto color = pref.GetColorRaw();
				float fcolor[4] =
				{
					color.m_r / 255.0f,
					color.m_g / 255.0f,
					color.m_b / 255.0f,
					color.m_a / 255.0f
				};

				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
				if(ImGui::ColorEdit4(label.c_str(), fcolor))
				{
					pref.SetColorRaw(impl::Color(
						static_cast<uint8_t>(fcolor[0] * 255),
						static_cast<uint8_t>(fcolor[1] * 255),
						static_cast<uint8_t>(fcolor[2] * 255),
						static_cast<uint8_t>(fcolor[3] * 255)
						));
				}
			}
			break;

		//Real: show a text box
		case PreferenceType::Real:
			{
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);

				//Units get special handling
				if(pref.HasUnit())
				{
					//No value yet, format the value
					auto id = pref.GetIdentifier();
					auto unit = pref.GetUnit();
					if(m_preferenceTemporaries.find(id) == m_preferenceTemporaries.end())
						m_preferenceTemporaries[id] = unit.PrettyPrint(pref.GetReal());

					//Input box
					if(ImGui::InputText(label.c_str(), &m_preferenceTemporaries[id]))
					{
						pref.SetReal(unit.ParseString(m_preferenceTemporaries[id]));
						m_preferenceTemporaries[id] = unit.PrettyPrint(pref.GetReal());
					}
				}

				//Raw numeric input
				else
				{
					float f = pref.GetReal();
					if(ImGui::InputFloat(label.c_str(), &f))
						pref.SetReal(f);
				}
			}
			break;

		//Int: show a text box
		case PreferenceType::Int:
			{
				int i = pref.GetInt();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
				if(ImGui::InputInt(label.c_str(), &i))
					pref.SetInt(i);
			}
			break;

		//Font: show a dropdown for the set of available fonts
		//and a selector for sizes
		case PreferenceType::Font:
			{
				auto font = pref.GetFont();

				string path = font.first;
				float size = font.second;
				int sel = m_fontReverseMap[path];
				label = string("###") + pref.GetIdentifier() + "face";
				bool changed = false;

				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 15);
				if(Combo(label, m_fontShortNames, sel))
				{
					path = m_fontPaths[sel];
					changed = true;
				}

				//Font size
				label = pref.GetLabel() + "###" + pref.GetIdentifier() + "size";
				ImGui::SameLine();
				ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
				if(ImGui::InputFloat(label.c_str(), &size, 1, 5))
					changed = true;

				if(changed)
					pref.SetFont(FontDescription(path, size));
			}
			break;

		default:
			ImGui::TextDisabled(
				"Unimplemented: %s = %s\n",
				pref.GetIdentifier().c_str(),
				pref.ToString().c_str());
			break;
	}

	HelpMarker(pref.GetDescription());
}
