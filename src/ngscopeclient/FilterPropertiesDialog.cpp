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
	@brief Implementation of FilterPropertiesDialog
 */
#include "ngscopeclient.h"
#include "ChannelPropertiesDialog.h"
#include "FilterPropertiesDialog.h"
#include "MainWindow.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constuction / destruction

FilterPropertiesDialog::FilterPropertiesDialog(Filter* f, MainWindow* parent, bool graphEditorMode)
	: ChannelPropertiesDialog(f, graphEditorMode)
	, m_parent(parent)
{

}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main GUI

bool FilterPropertiesDialog::Render()
{
	//Run file dialog and handle it being closed
	if(m_fileDialog)
	{
		float fontsize = ImGui::GetFontSize();
		if(m_fileDialog->Display("FileChooser", ImGuiWindowFlags_NoCollapse, ImVec2(60*fontsize, 30*fontsize)))
		{
			if(m_fileDialog->IsOk())
			{
				auto f = dynamic_cast<Filter*>(m_channel);
				f->GetParameter(m_fileParamName).SetFileName(m_fileDialog->GetFilePathName());
				m_paramTempValues.erase(m_fileParamName);
			}

			m_fileDialog->Close();
		}
	}

	return Dialog::Render();
}

//TODO: some of this code needs to be shared by the trigger dialog

bool FilterPropertiesDialog::DoRender()
{
	//Flags for a header that should be open by default EXCEPT in the graph editor
	ImGuiTreeNodeFlags defaultOpenFlags = m_graphEditorMode ? 0 : ImGuiTreeNodeFlags_DefaultOpen;

	//Update name as we go
	m_title = m_channel->GetHwname();

	if(!ChannelPropertiesDialog::DoRender())
		return false;

	auto f = dynamic_cast<Filter*>(m_channel);

	bool reconfigured = false;

	//Show inputs (if we have any)
	if( (f->GetInputCount() != 0) && !m_graphEditorMode)
	{
		if(ImGui::CollapsingHeader("Inputs", ImGuiTreeNodeFlags_DefaultOpen))
		{
			//TODO: cache some of this?
			vector<StreamDescriptor> streams;
			FindAllStreams(streams);

			for(size_t i=0; i<f->GetInputCount(); i++)
			{
				//Find the set of legal streams for this input
				vector<StreamDescriptor> matchingInputs;
				vector<string> names;
				int sel = -1;
				for(auto stream : streams)
				{
					if(!f->ValidateChannel(i, stream))
						continue;

					if(f->GetInput(i) == stream)
						sel = matchingInputs.size();

					matchingInputs.push_back(stream);
					names.push_back(stream.GetName());
				}

				//The actual combo box
				if(Combo(f->GetInputName(i).c_str(), names, sel))
				{
					f->SetInput(i, matchingInputs[sel]);
					reconfigured = true;
				}
			}
		}
	}

	//Show parameters (if we have any)
	if(f->GetParamCount() != 0)
	{
		if(ImGui::CollapsingHeader("Parameters", defaultOpenFlags))
		{
			for(auto it = f->GetParamBegin(); it != f->GetParamEnd(); it++)
			{
				auto& param = it->second;
				auto name = it->first;

				//See what kind of parameter it is
				switch(param.GetType())
				{
					case FilterParameter::TYPE_FLOAT:
						{
							//If we don't have a temporary value, make one
							auto nval = param.GetFloatVal();
							if(m_paramTempValues.find(name) == m_paramTempValues.end())
								m_paramTempValues[name] = param.GetUnit().PrettyPrint(nval);

							//Input path
							ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
							if(UnitInputWithImplicitApply(name.c_str(), m_paramTempValues[name], nval, param.GetUnit()))
							{
								param.SetFloatVal(nval);
								reconfigured = true;
							}
						}
						break;

					case FilterParameter::TYPE_INT:
						{
							//If we don't have a temporary value, make one
							//TODO: can we figure out how to preserve full int64 precision end to end here?
							//For now, use a double to get as close as we can
							double nval = param.GetIntVal();
							if(m_paramTempValues.find(name) == m_paramTempValues.end())
								m_paramTempValues[name] = param.GetUnit().PrettyPrint(nval);

							//Input path
							ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
							if(UnitInputWithImplicitApply(name.c_str(), m_paramTempValues[name], nval, param.GetUnit()))
							{
								param.SetIntVal(nval);
								reconfigured = true;
							}
						}
						break;

					case FilterParameter::TYPE_BOOL:
						{
							bool b = param.GetBoolVal();
							if(ImGui::Checkbox(name.c_str(), &b))
							{
								param.SetBoolVal(b);
								reconfigured = true;
							}
						}
						break;

					case FilterParameter::TYPE_STRING:
						{
							//If we don't have a temporary value, make one
							string s = param.ToString();
							if(m_paramTempValues.find(name) == m_paramTempValues.end())
								m_paramTempValues[name] = s;

							//Input path
							ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
							if(TextInputWithImplicitApply(name.c_str(), m_paramTempValues[name], s))
							{
								param.SetStringVal(s);
								reconfigured = true;
							}
						}
						break;

					case FilterParameter::TYPE_ENUM:
						{
							vector<string> enumValues;
							param.GetEnumValues(enumValues);

							int nsel = -1;
							string s = param.ToString();
							for(size_t i=0; i<enumValues.size(); i++)
							{
								if(enumValues[i] == s)
								{
									nsel = i;
									break;
								}
							}

							if(Combo(name.c_str(), enumValues, nsel))
							{
								param.ParseString(enumValues[nsel]);
								reconfigured = true;
							}
						}
						break;

					case FilterParameter::TYPE_FILENAME:
						{
							string s = param.GetFileName();
							if(m_paramTempValues.find(name) == m_paramTempValues.end())
								m_paramTempValues[name] = s;

							//Input path
							ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10);
							string tname = string("###path") + name;
							if(TextInputWithImplicitApply(tname.c_str(), m_paramTempValues[name], s))
							{
								param.SetStringVal(s);
								reconfigured = true;
							}

							//Tweak the mask for imgui filedialog
							//(needs to be in parentheses to be recognized as a regex)
							//Special case for touchstone since internal parentheses aren't well supported by IGFD
							string mask;
							if(param.m_fileFilterMask == "*.s*p")
								mask = "Touchstone files (*.s*p){.s2p,.s3p,.s4p,.s5p,.s6p,.s7p,.s8p,.s9p,.snp}";
							else
								mask = param.m_fileFilterName + "{" + param.m_fileFilterMask.substr(1) + "}";

							//Browser button
							ImGui::SameLine();
							string bname = string("...###browse") + name;
							if(ImGui::Button(bname.c_str()))
							{
								m_fileDialog = make_unique<ImGuiFileDialog>();
								m_fileDialog->OpenDialog(
									"FileChooser",
									"Select File",
									mask.c_str(),
									".",
									s);
								m_fileParamName = name;
							}
							ImGui::SameLine();
							ImGui::TextUnformatted(name.c_str());
						}
						break;

					//TODO: TYPE_8B10B_PATTERN

					default:
						ImGui::Text("Parameter %s is unimplemented type", name.c_str());
						break;
				}
			}
		}
	}

	if(reconfigured)
	{
		//Update auto generated name
		if(f->IsUsingDefaultName())
		{
			f->SetDefaultName();
			m_committedDisplayName = f->GetDisplayName();
			m_displayName = m_committedDisplayName;
		}

		m_parent->OnFilterReconfigured(f);
	}

	return true;
}

/**
	@brief Get every stream that might be usable as an input to a filter
 */
void FilterPropertiesDialog::FindAllStreams(vector<StreamDescriptor>& streams)
{
	//Null stream always has to be considered
	streams.push_back(StreamDescriptor(nullptr, 0));

	//Then find every channel of every scope
	auto& scopes = m_parent->GetSession().GetScopes();
	for(auto scope : scopes)
	{
		for(size_t i=0; i<scope->GetChannelCount(); i++)
		{
			auto chan = scope->GetChannel(i);
			for(size_t j=0; j<chan->GetStreamCount(); j++)
				streams.push_back(StreamDescriptor(chan, j));
		}
	}

	//Then add every stream of every filter
	auto filters = Filter::GetAllInstances();
	for(auto f : filters)
	{
		for(size_t j=0; j<f->GetStreamCount(); j++)
			streams.push_back(StreamDescriptor(f, j));
	}
}
