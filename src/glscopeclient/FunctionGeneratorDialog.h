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
	@brief Declaration of FunctionGenerator
 */

#ifndef FunctionGeneratorDialog_h
#define FunctionGeneratorDialog_h

class FunctionGeneratorChannelPage
{
public:
	FunctionGeneratorChannelPage(FunctionGenerator* gen, size_t channel);
	virtual ~FunctionGeneratorChannelPage();

	Gtk::Frame m_frame;
		Gtk::Grid m_grid;
			Gtk::Label m_impedanceLabel;
				Gtk::ComboBoxText m_impedanceBox;
			Gtk::Label m_functionTypeLabel;
				Gtk::ComboBoxText m_functionTypeBox;
			Gtk::Label m_amplitudeLabel;
				Gtk::Entry m_amplitudeBox;
				Gtk::Button m_amplitudeApplyButton;
			Gtk::Label m_offsetLabel;
				Gtk::Entry m_offsetBox;
				Gtk::Button m_offsetApplyButton;
			Gtk::Label m_dutyLabel;
				Gtk::Entry m_dutyBox;
			Gtk::Label m_freqLabel;
				Gtk::Entry m_freqBox;
			Gtk::Label m_oeLabel;
				Gtk::Switch m_oeSwitch;

protected:

	void OnAmplitudeApply();
	void OnAmplitudeChanged();
	void OnOffsetApply();
	void OnOffsetChanged();
	void OnDutyCycleChanged();
	void OnOutputEnableChanged();
	void OnWaveformChanged();
	void OnOutputImpedanceChanged();
	void OnFrequencyChanged();

	FunctionGenerator* m_gen;
	size_t m_channel;

	std::vector<FunctionGenerator::WaveShape> m_waveShapes;
};

/**
	@brief Dialog for interacting with a FunctionGenerator (which may or may not be part of an Oscilloscope)
 */
class FunctionGeneratorDialog	: public Gtk::Dialog
{
public:
	FunctionGeneratorDialog(FunctionGenerator* gen);
	virtual ~FunctionGeneratorDialog();

protected:
	virtual void on_show();
	virtual void on_hide();

	/*
	void OnInputChanged();
	void OnModeChanged();

	void AddMode(FunctionGenerator::MeasurementTypes type, const std::string& label);
	std::map<std::string, FunctionGenerator::MeasurementTypes> m_modemap;
	*/
	FunctionGenerator* m_gen;

	//Top level control
	Gtk::Grid m_grid;
		std::vector<FunctionGeneratorChannelPage*> m_pages;

	/*
		Gtk::Label			m_inputLabel;
			Gtk::ComboBoxText	m_inputBox;
		Gtk::Label			m_typeLabel;
			Gtk::ComboBoxText	m_typeBox;
		Gtk::Label			m_valueLabel;
			Gtk::Label			m_valueBox;
		Graph m_graph;
			Graphable m_graphData;

	double m_minval;
	double m_maxval;
	*/
};

#endif
