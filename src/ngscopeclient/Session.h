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
	@brief Declaration of Session
 */
#ifndef Session_h
#define Session_h

class MainWindow;

/**
	@brief Internal state for a connection to an RF signal generator
 */
class RFSignalGeneratorConnectionState
{
public:
	RFSignalGeneratorConnectionState(SCPIRFSignalGenerator* gen, std::shared_ptr<RFSignalGeneratorState> state)
		: m_gen(gen)
		, m_shuttingDown(false)
	{
		RFSignalGeneratorThreadArgs args(gen, &m_shuttingDown, state);
		m_thread = std::make_unique<std::thread>(RFSignalGeneratorThread, args);
	}

	~RFSignalGeneratorConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Disconnect once the thread has terminated
		delete m_gen;
	}

	///@brief The signal generator
	SCPIRFSignalGenerator* m_gen;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the generator
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief Internal state for a connection to a PSU
 */
class PowerSupplyConnectionState
{
public:
	PowerSupplyConnectionState(SCPIPowerSupply* psu, std::shared_ptr<PowerSupplyState> state)
		: m_psu(psu)
		, m_shuttingDown(false)
	{
		PowerSupplyThreadArgs args(psu, &m_shuttingDown, state);
		m_thread = std::make_unique<std::thread>(PowerSupplyThread, args);
	}

	~PowerSupplyConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Disconnect once the thread has terminated
		delete m_psu;
	}

	///@brief The power supply
	SCPIPowerSupply* m_psu;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the PSU
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief Internal state for a connection to a multimeter
 */
class MultimeterConnectionState
{
public:
	MultimeterConnectionState(SCPIMultimeter* meter, std::shared_ptr<MultimeterState> state)
		: m_meter(meter)
		, m_shuttingDown(false)
	{
		MultimeterThreadArgs args(meter, &m_shuttingDown, state);
		m_thread = std::make_unique<std::thread>(MultimeterThread, args);
	}

	~MultimeterConnectionState()
	{
		//Terminate the thread
		m_shuttingDown = true;
		m_thread->join();

		//Delete the meter once the thread has terminated unless it's also an oscilloscope
		if(dynamic_cast<Oscilloscope*>(m_meter) == nullptr)
			delete m_meter;
	}

	///@brief The meter
	SCPIMultimeter* m_meter;

	///@brief Termination flag for shutting down the polling thread
	std::atomic<bool> m_shuttingDown;

	///@brief Thread for polling the meter
	std::unique_ptr<std::thread> m_thread;
};

/**
	@brief A Session stores all of the instrument configuration and other state the user has open.

	Generally only accessed from the GUI thread.
	TODO: interlocking if needed?
 */
class Session
{
public:
	Session(MainWindow* wnd);
	virtual ~Session();

	void Clear();

	void AddFunctionGenerator(SCPIFunctionGenerator* generator);
	void RemoveFunctionGenerator(SCPIFunctionGenerator* generator);
	void AddMultimeter(SCPIMultimeter* meter);
	void RemoveMultimeter(SCPIMultimeter* meter);
	void AddOscilloscope(Oscilloscope* scope);
	void AddPowerSupply(SCPIPowerSupply* psu);
	void RemovePowerSupply(SCPIPowerSupply* psu);
	void AddRFGenerator(SCPIRFSignalGenerator* generator);
	void RemoveRFGenerator(SCPIRFSignalGenerator* generator);

	/**
		@brief Get the set of scopes we're currently connected to
	 */
	const std::vector<Oscilloscope*>& GetScopes()
	{ return m_oscilloscopes; }

	/**
		@brief Gets the set of all SCPI instruments we're connect to (regardless of type)
	 */
	std::set<SCPIInstrument*> GetSCPIInstruments();

protected:

	///@brief Top level UI window
	MainWindow* m_mainWindow;

	///@brief Flag for shutting down all scope threads when we exit
	std::atomic<bool> m_shuttingDown;

	///@brief True if the session has been modified since last time it was saved
	bool m_modifiedSinceLastSave;

	///@brief Oscilloscopes we are currently connected to
	std::vector<Oscilloscope*> m_oscilloscopes;

	///@brief Power supplies we are currently connected to
	std::map<PowerSupply*, std::unique_ptr<PowerSupplyConnectionState> > m_psus;

	///@brief Multimeters we are currently connected to
	std::map<Multimeter*, std::unique_ptr<MultimeterConnectionState> > m_meters;

	///@brief RF generators we are currently connected to
	std::map<SCPIRFSignalGenerator*, std::unique_ptr<RFSignalGeneratorConnectionState> > m_rfgenerators;

	///@brief Function generators we are currently connected to
	std::vector<SCPIFunctionGenerator*> m_generators;

	///@brief Processing threads for polling and processing scope waveforms
	std::vector< std::unique_ptr<std::thread> > m_threads;
};

#endif
