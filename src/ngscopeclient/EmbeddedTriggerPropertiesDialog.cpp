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

#include "ngscopeclient.h"
#include "EmbeddedTriggerPropertiesDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

EmbeddedTriggerPropertiesDialog::EmbeddedTriggerPropertiesDialog(shared_ptr<Oscilloscope> scope)
	: EmbeddableDialog("Trigger", string("Trigger properties: ") + scope->m_nickname, ImVec2(300, 400), true)
	, m_scope(scope)
{
	m_page = make_unique<TriggerPropertiesPage>(scope);

	//Figure out combo index for active trigger
	m_triggerTypeIndex = 0;
	vector<string> types = scope->GetTriggerTypes();
	auto trig = scope->GetTrigger();
	string ttype;
	if(trig)
		ttype = trig->GetTriggerDisplayName();
	for(size_t i=0; i<types.size(); i++)
	{
		if(types[i] == ttype)
		{
			m_triggerTypeIndex = i;
			break;
		}
	}
}

EmbeddedTriggerPropertiesDialog::~EmbeddedTriggerPropertiesDialog()
{
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Rendering

bool EmbeddedTriggerPropertiesDialog::DoRender()
{
	//Dropdown with list of trigger types is outside the main trigger panel
	//TODO: cache some of this?
	vector<string> types = m_scope->GetTriggerTypes();
	if(Combo("Type", types, m_triggerTypeIndex))
	{
		//Save the level and inputs of the old trigger so we can reuse it
		auto oldTrig = m_scope->GetTrigger();
		float level = 0;
		if(oldTrig)
			level = oldTrig->GetLevel();
		vector<StreamDescriptor> inputs;
		for(size_t j=0; j<oldTrig->GetInputCount(); j++)
			inputs.push_back(oldTrig->GetInput(j));

		//Create the new trigger
		auto newTrig = Trigger::CreateTrigger(types[m_triggerTypeIndex], m_scope.get());
		if(newTrig)
		{
			//Copy settings over from old trigger to new
			//TODO: copy both levels if both are two level triggers
			newTrig->SetLevel(level);
			for(size_t j=0; (j<newTrig->GetInputCount()) && (j < inputs.size()); j++)
				newTrig->SetInput(j, inputs[j]);

			//Push changes to the scope all at once after the new trigger is set up
			m_scope->SetTrigger(newTrig);
			m_scope->PushTrigger();

			//Replace the properties page with whatever the new trigger eeds
			m_page = make_unique<TriggerPropertiesPage>(m_scope);
		}
	}
	HelpMarker("Select the type of trigger for this instrument\n");

	//Render the main trigger page
	if(m_page)
		m_page->Render(true);

	return true;
}
