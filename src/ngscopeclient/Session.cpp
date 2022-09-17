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
	@brief Implementation of Session
 */
#include "ngscopeclient.h"
#include "Session.h"
#include "MainWindow.h"
#include "FunctionGeneratorDialog.h"
#include "MultimeterDialog.h"
#include "PowerSupplyDialog.h"
#include "RFGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

Session::Session(MainWindow* wnd)
	: m_mainWindow(wnd)
	, m_shuttingDown(false)
	, m_modifiedSinceLastSave(false)
{
}

Session::~Session()
{
	Clear();
}

void Session::Clear()
{
	//Signal our threads to exit
	m_shuttingDown = true;

	//Block until our processing threads exit
	for(auto& t : m_threads)
		t->join();
	m_threads.clear();

	//Delete scopes once we've terminated the threads
	for(auto scope : m_oscilloscopes)
		delete scope;
	m_oscilloscopes.clear();
	m_psus.clear();
	m_rfgenerators.clear();
	m_meters.clear();

	//Clear shutdown flag in case we're reusing the session object
	m_shuttingDown = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Instrument management

void Session::AddOscilloscope(Oscilloscope* scope)
{
	m_modifiedSinceLastSave = true;
	m_oscilloscopes.push_back(scope);

	m_threads.push_back(make_unique<thread>(ScopeThread, scope, &m_shuttingDown));

	m_mainWindow->AddToRecentInstrumentList(dynamic_cast<SCPIOscilloscope*>(scope));
	m_mainWindow->OnScopeAdded(scope);
}

/**
	@brief Adds a power supply to the session
 */
void Session::AddPowerSupply(SCPIPowerSupply* psu)
{
	m_modifiedSinceLastSave = true;

	//Create shared PSU state
	auto state = make_shared<PowerSupplyState>(psu->GetPowerChannelCount());
	m_psus[psu] = make_unique<PowerSupplyConnectionState>(psu, state);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<PowerSupplyDialog>(psu, state, this));

	m_mainWindow->AddToRecentInstrumentList(psu);
}

/**
	@brief Removes a power supply from the session
 */
void Session::RemovePowerSupply(SCPIPowerSupply* psu)
{
	m_modifiedSinceLastSave = true;
	m_psus.erase(psu);
}

/**
	@brief Adds a multimeter to the session
 */
void Session::AddMultimeter(SCPIMultimeter* meter)
{
	m_modifiedSinceLastSave = true;

	//Create shared meter state
	auto state = make_shared<MultimeterState>();
	m_meters[meter] = make_unique<MultimeterConnectionState>(meter, state);

	//Add the dialog to view/control it
	m_mainWindow->AddDialog(make_shared<MultimeterDialog>(meter, state, this));

	m_mainWindow->AddToRecentInstrumentList(meter);
}

/**
	@brief Removes a multimeter from the session
 */
void Session::RemoveMultimeter(SCPIMultimeter* meter)
{
	m_modifiedSinceLastSave = true;
	m_meters.erase(meter);
}

/**
	@brief Adds a function generator to the session
 */
void Session::AddFunctionGenerator(SCPIFunctionGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	m_generators.push_back(generator);
	m_mainWindow->AddDialog(make_shared<FunctionGeneratorDialog>(generator, this));

	m_mainWindow->AddToRecentInstrumentList(generator);
}

/**
	@brief Removes a function generator from the session
 */
void Session::RemoveFunctionGenerator(SCPIFunctionGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	for(size_t i=0; i<m_generators.size(); i++)
	{
		if(m_generators[i] == generator)
		{
			m_generators.erase(m_generators.begin() + i);
			break;
		}
	}

	//Free it iff it's not part of an oscilloscope or RF signal generator
	if( (dynamic_cast<Oscilloscope*>(generator) == nullptr) && (dynamic_cast<RFSignalGenerator*>(generator) == nullptr) )
		delete generator;
}

/**
	@brief Adds an RF signal generator to the session
 */
void Session::AddRFGenerator(SCPIRFSignalGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	//Create shared meter state
	auto state = make_shared<RFSignalGeneratorState>(generator->GetChannelCount());
	m_rfgenerators[generator] = make_unique<RFSignalGeneratorConnectionState>(generator, state);

	m_mainWindow->AddDialog(make_shared<RFGeneratorDialog>(generator, state, this));

	m_mainWindow->AddToRecentInstrumentList(generator);
}

/**
	@brief Removes an RF signal from the session
 */
void Session::RemoveRFGenerator(SCPIRFSignalGenerator* generator)
{
	m_modifiedSinceLastSave = true;

	//If the generator is also a function generator, delete that too
	//FIXME: This is not the best UX. Would be best to ref count and delete when both are closed
	auto func = dynamic_cast<SCPIFunctionGenerator*>(generator);
	if(func != nullptr)
	{
		RemoveFunctionGenerator(func);
		m_mainWindow->RemoveFunctionGenerator(func);
	}

	m_rfgenerators.erase(generator);
}

/**
	@brief Returns a list of all connected instruments, of any type

	Multi-type instruments are only counted once.
 */
set<SCPIInstrument*> Session::GetSCPIInstruments()
{
	set<SCPIInstrument*> insts;
	for(auto& scope : m_oscilloscopes)
	{
		auto s = dynamic_cast<SCPIInstrument*>(scope);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_psus)
	{
		auto s = dynamic_cast<SCPIInstrument*>(it.first);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_meters)
	{
		auto s = dynamic_cast<SCPIInstrument*>(it.first);
		if(s != nullptr)
			insts.emplace(s);
	}
	for(auto& it : m_rfgenerators)
		insts.emplace(it.first);
	for(auto gen : m_generators)
		insts.emplace(gen);

	return insts;
}
