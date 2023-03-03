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
	@brief Implementation of FunctionGeneratorDialog
 */
#include "glscopeclient.h"
#include "FunctionGeneratorDialog.h"

using namespace std;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FunctionGeneratorChannelPage

FunctionGeneratorChannelPage::FunctionGeneratorChannelPage(FunctionGenerator* gen, size_t channel)
	: m_gen(gen)
	, m_channel(channel)
{
	Unit volts(Unit::UNIT_VOLTS);
	Unit percent(Unit::UNIT_PERCENT);
	Unit hz(Unit::UNIT_HZ);

	m_frame.set_label(gen->GetFunctionChannelName(channel));
	m_frame.add(m_grid);

		auto shapes = gen->GetAvailableWaveformShapes(channel);

		int row = 0;
		m_grid.attach(m_impedanceLabel,		0, row, 1, 1);
			m_impedanceLabel.set_text("Output Impedance");
		m_grid.attach(m_impedanceBox,		1, row, 2, 1);
			m_impedanceBox.append("50Ω");
			m_impedanceBox.append("High-Z");
			if(m_gen->GetFunctionChannelOutputImpedance(channel) == FunctionGenerator::IMPEDANCE_HIGH_Z)
				m_impedanceBox.set_active_text("High-Z");
			else
				m_impedanceBox.set_active_text("50Ω");
			m_impedanceBox.signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnOutputImpedanceChanged));

		//Waveform type
		row++;
		m_grid.attach(m_functionTypeLabel,		0, row, 1, 1);
			m_functionTypeLabel.set_text("Waveform");
		m_grid.attach(m_functionTypeBox,		1, row, 2, 1);

		//Populate list of legal functions
		auto curShape = gen->GetFunctionChannelShape(m_channel);
		for(auto s : shapes)
		{
			string str;
			switch(s)
			{
				case FunctionGenerator::SHAPE_SINE:
					str = "Sine";
					break;

				case FunctionGenerator::SHAPE_SQUARE:
					str = "Square";
					break;

				case FunctionGenerator::SHAPE_TRIANGLE:
					str = "Triangle";
					break;

				case FunctionGenerator::SHAPE_PULSE:
					str = "Pulse";
					break;

				case FunctionGenerator::SHAPE_DC:
					str = "DC";
					break;

				case FunctionGenerator::SHAPE_NOISE:
					str = "Noise";
					break;

				case FunctionGenerator::SHAPE_SAWTOOTH_UP:
					str = "Sawtooth up";
					break;

				case FunctionGenerator::SHAPE_SAWTOOTH_DOWN:
					str = "Sawtooth down";
					break;

				case FunctionGenerator::SHAPE_SINC:
					str = "Sinc";
					break;

				case FunctionGenerator::SHAPE_GAUSSIAN:
					str = "Gaussian";
					break;

				case FunctionGenerator::SHAPE_LORENTZ:
					str = "Lorentz";
					break;

				case FunctionGenerator::SHAPE_HALF_SINE:
					str = "Half sine";
					break;

				case FunctionGenerator::SHAPE_PRBS_NONSTANDARD:
					str = "PRBS (nonstandard polynomial)";
					break;

				case FunctionGenerator::SHAPE_EXPONENTIAL_RISE:
					str = "Exponential Rise";
					break;

				case FunctionGenerator::SHAPE_EXPONENTIAL_DECAY:
					str = "Exponential Decay";
					break;

				case FunctionGenerator::SHAPE_HAVERSINE:
					str = "Haversine";
					break;

				case FunctionGenerator::SHAPE_CARDIAC:
					str = "Cardiac";
					break;

				case FunctionGenerator::SHAPE_STAIRCASE_UP:
					str = "Staircase up";
					break;

				case FunctionGenerator::SHAPE_STAIRCASE_DOWN:
					str = "Staircase down";
					break;

				case FunctionGenerator::SHAPE_STAIRCASE_UP_DOWN:
					str = "Staircase triangular";
					break;

				case FunctionGenerator::SHAPE_NEGATIVE_PULSE:
					str = "Negative pulse";
					break;

				case FunctionGenerator::SHAPE_LOG_RISE:
					str = "Logarithmic rise";
					break;

				case FunctionGenerator::SHAPE_LOG_DECAY:
					str = "Logarithmic decay";
					break;

				case FunctionGenerator::SHAPE_SQUARE_ROOT:
					str = "Square root";
					break;

				case FunctionGenerator::SHAPE_CUBE_ROOT:
					str = "Cube root";
					break;

				case FunctionGenerator::SHAPE_QUADRATIC:
					str = "Quadratic";
					break;

				case FunctionGenerator::SHAPE_CUBIC:
					str = "Cubic";
					break;

				case FunctionGenerator::SHAPE_DLORENTZ:
					str = "DLorentz";
					break;

				case FunctionGenerator::SHAPE_GAUSSIAN_PULSE:
					str = "Gaussian pulse";
					break;

				case FunctionGenerator::SHAPE_HAMMING:
					str = "Hamming";
					break;

				case FunctionGenerator::SHAPE_HANNING:
					str = "Hanning";
					break;

				case FunctionGenerator::SHAPE_KAISER:
					str = "Kaiser";
					break;

				case FunctionGenerator::SHAPE_BLACKMAN:
					str = "Blackman";
					break;

				case FunctionGenerator::SHAPE_GAUSSIAN_WINDOW:
					str = "Gaussian window";
					break;

				case FunctionGenerator::SHAPE_HARRIS:
					str = "Harris";
					break;

				case FunctionGenerator::SHAPE_BARTLETT:
					str = "Bartlett";
					break;

				case FunctionGenerator::SHAPE_TAN:
					str = "Tan";
					break;

				case FunctionGenerator::SHAPE_COT:
					str = "Cot";
					break;

				case FunctionGenerator::SHAPE_SEC:
					str = "Sec";
					break;

				case FunctionGenerator::SHAPE_CSC:
					str = "Csc";
					break;

				case FunctionGenerator::SHAPE_ASIN:
					str = "Asin";
					break;

				case FunctionGenerator::SHAPE_ACOS:
					str = "Acos";
					break;

				case FunctionGenerator::SHAPE_ATAN:
					str = "Atan";
					break;

				case FunctionGenerator::SHAPE_ACOT:
					str = "Acot";
					break;

				//Arbitrary is not supported yet so don't show it in the list
				//case FunctionGenerator::SHAPE_ARBITRARY:
				//	continue;

				default:
					str = "Unknown";
					break;
			}

			m_functionTypeBox.append(str);
			if(s == curShape)
				m_functionTypeBox.set_active_text(str);

			m_waveShapes.push_back(s);
		}

		m_functionTypeBox.signal_changed().connect(
			sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnWaveformChanged));

		//Amplitude
		row++;
		m_grid.attach(m_amplitudeLabel,			0, row, 1, 1);
			m_amplitudeLabel.set_text("Amplitude");
		m_grid.attach(m_amplitudeBox,			1, row, 1, 1);
			m_amplitudeBox.set_text(volts.PrettyPrint(gen->GetFunctionChannelAmplitude(channel)));
			m_amplitudeBox.signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnAmplitudeChanged));
		m_grid.attach(m_amplitudeApplyButton,	2, row, 1, 1);
			m_amplitudeApplyButton.set_label("Apply");
			m_amplitudeApplyButton.set_sensitive(false);
			m_amplitudeApplyButton.signal_clicked().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnAmplitudeApply));

		//Offset
		row++;
		m_grid.attach(m_offsetLabel,			0, row, 1, 1);
			m_offsetLabel.set_text("Offset");
		m_grid.attach(m_offsetBox,			1, row, 1, 1);
			m_offsetBox.set_text(volts.PrettyPrint(gen->GetFunctionChannelOffset(channel)));
			m_offsetBox.signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnOffsetChanged));
		m_grid.attach(m_offsetApplyButton,	2, row, 1, 1);
			m_offsetApplyButton.set_label("Apply");
			m_offsetApplyButton.set_sensitive(false);
			m_offsetApplyButton.signal_clicked().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnOffsetApply));

		//Duty cycle
		row++;
		m_grid.attach(m_dutyLabel,			0, row, 1, 1);
			m_dutyLabel.set_text("Duty Cycle");
		m_grid.attach(m_dutyBox,			1, row, 2, 1);
			m_dutyBox.set_text(percent.PrettyPrint(gen->GetFunctionChannelDutyCycle(channel)));
			m_dutyBox.signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnDutyCycleChanged));

		//Frequency
		row++;
		m_grid.attach(m_freqLabel,			0, row, 1, 1);
			m_freqLabel.set_text("Frequency");
		m_grid.attach(m_freqBox,			1, row, 2, 1);
			m_freqBox.set_text(hz.PrettyPrint(gen->GetFunctionChannelFrequency(channel)));
			m_freqBox.signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnFrequencyChanged));

		//On/off switch
		row++;
		m_grid.attach(m_oeLabel,			0, row, 1, 1);
			m_oeLabel.set_text("Output Enable");
		m_grid.attach(m_oeSwitch,			1, row, 1, 1);
			m_oeSwitch.set_state(gen->GetFunctionChannelActive(channel));
			m_oeSwitch.property_active().signal_changed().connect(
				sigc::mem_fun(*this, &FunctionGeneratorChannelPage::OnOutputEnableChanged));

		//TODO: rise/fall time

	m_frame.show_all();
}

FunctionGeneratorChannelPage::~FunctionGeneratorChannelPage()
{

}

void FunctionGeneratorChannelPage::OnAmplitudeApply()
{
	m_amplitudeApplyButton.set_sensitive(false);

	Unit volts(Unit::UNIT_VOLTS);
	m_gen->SetFunctionChannelAmplitude(m_channel, volts.ParseString(m_amplitudeBox.get_text()));
}

void FunctionGeneratorChannelPage::OnAmplitudeChanged()
{
	m_amplitudeApplyButton.set_sensitive();
}

void FunctionGeneratorChannelPage::OnOffsetApply()
{
	m_offsetApplyButton.set_sensitive(false);

	Unit volts(Unit::UNIT_VOLTS);
	m_gen->SetFunctionChannelOffset(m_channel, volts.ParseString(m_offsetBox.get_text()));
}

void FunctionGeneratorChannelPage::OnOffsetChanged()
{
	m_offsetApplyButton.set_sensitive();
}

void FunctionGeneratorChannelPage::OnDutyCycleChanged()
{
	Unit pct(Unit::UNIT_PERCENT);
	m_gen->SetFunctionChannelDutyCycle(m_channel, pct.ParseString(m_dutyBox.get_text()));
}

void FunctionGeneratorChannelPage::OnOutputEnableChanged()
{
	m_gen->SetFunctionChannelActive(m_channel, m_oeSwitch.get_state());
}

void FunctionGeneratorChannelPage::OnWaveformChanged()
{
	auto wfm = m_waveShapes[m_functionTypeBox.get_active_row_number()];
	m_gen->SetFunctionChannelShape(m_channel, wfm);

	//Enable or disable duty cycle
	switch(wfm)
	{
		case FunctionGenerator::SHAPE_PULSE:
		case FunctionGenerator::SHAPE_SQUARE:
		case FunctionGenerator::SHAPE_PRBS_NONSTANDARD:
			m_dutyBox.set_sensitive();
			break;

		default:
			m_dutyBox.set_sensitive(false);
	}
}

void FunctionGeneratorChannelPage::OnOutputImpedanceChanged()
{
	if(m_impedanceBox.get_active_row_number() == 0)
		m_gen->SetFunctionChannelOutputImpedance(m_channel, FunctionGenerator::IMPEDANCE_50_OHM);
	else
		m_gen->SetFunctionChannelOutputImpedance(m_channel, FunctionGenerator::IMPEDANCE_HIGH_Z);
}

void FunctionGeneratorChannelPage::OnFrequencyChanged()
{
	Unit hz(Unit::UNIT_HZ);
	m_gen->SetFunctionChannelFrequency(m_channel, hz.ParseString(m_freqBox.get_text()));
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction / destruction

FunctionGeneratorDialog::FunctionGeneratorDialog(FunctionGenerator* gen)
	: Gtk::Dialog( string("Function Generator: ") + gen->m_nickname )
	, m_gen(gen)
{
	get_vbox()->add(m_grid);

	//Add each channel page
	size_t n = gen->GetFunctionChannelCount();
	for(size_t i=0; i<n; i++)
	{
		auto page = new FunctionGeneratorChannelPage(gen, i);
		m_grid.attach(page->m_frame, 0, i, 1, 1);
		m_pages.push_back(page);
	}

	show_all();
}

FunctionGeneratorDialog::~FunctionGeneratorDialog()
{
	for(auto p : m_pages)
	{
		m_grid.remove(p->m_frame);
		delete p;
	}
}

/*
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Event handlers

void FunctionGeneratorDialog::AddMode(FunctionGenerator::MeasurementTypes type, const std::string& label)
{
	if(m_meter->GetMeasurementTypes() & type)
	{
		m_typeBox.append(label);
		m_modemap[label] = type;
	}
}
*/
void FunctionGeneratorDialog::on_show()
{
	Gtk::Dialog::on_show();
	show_all();

	//m_meter->StartMeter();
}

void FunctionGeneratorDialog::on_hide()
{
	Gtk::Dialog::on_hide();

	//m_meter->StopMeter();
}
