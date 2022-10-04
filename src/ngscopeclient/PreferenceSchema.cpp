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

#include "PreferenceManager.h"
#include "PreferenceTypes.h"
#include "ngscopeclient.h"

void PreferenceManager::InitializeDefaults()
{
	auto& appearance = this->m_treeRoot.AddCategory("Appearance");
		auto& cursors = appearance.AddCategory("Cursors");
			cursors.AddPreference(
				Preference::Color("cursor_1_color", ColorFromString("#ffff00"))
				.Label("Cursor #1 color")
				.Description("Color for the left or top cursor"));
			cursors.AddPreference(
				Preference::Color("cursor_2_color", ColorFromString("#ff8000"))
				.Label("Cursor #2 color")
				.Description("Color for the right or bottom cursor"));
			cursors.AddPreference(
				Preference::Color("cursor_fill_color", ColorFromString("#ffff0040"))
				.Label("Cursor fill color")
				.Description("Color for the filled area between cursors"));
			cursors.AddPreference(
				Preference::Color("cursor_fill_text_color", ColorFromString("#ffff00"))
				.Label("Cursor fill text color")
				.Description("Color for in-band power and other text drawn between cursors"));
			cursors.AddPreference(
				Preference::Font("label_font", "sans normal 10")
				.Label("Cursor label font")
				.Description("Font used for voltage measurements displayed next to cursors"));
			cursors.AddPreference(
				Preference::Color("marker_color", ColorFromString("#ff00a0"))
				.Label("Marker color")
				.Description("Color for markers"));

		auto& decodes = appearance.AddCategory("Decodes");
			decodes.AddPreference(
				Preference::Font("protocol_font", "sans normal 10")
				.Label("Protocol font")
				.Description("Font used for protocol decode overlay text"));
			decodes.AddPreference(
				Preference::Color("address_color", ColorFromString("#ffff00"))
				.Label("Address color")
				.Description("Color for register/memory addresses"));
			decodes.AddPreference(
				Preference::Color("checksum_bad_color", ColorFromString("#ff0000"))
				.Label("Checksum/CRC color (Bad)")
				.Description("Color for incorrect checksums/CRCs"));
			decodes.AddPreference(
				Preference::Color("checksum_ok_color", ColorFromString("#00ff00"))
				.Label("Checksum/CRC color (OK)")
				.Description("Color for correct checksums/CRCs"));
			decodes.AddPreference(
				Preference::Color("control_color", ColorFromString("#c000a0"))
				.Label("Control color")
				.Description("Color for control events"));
			decodes.AddPreference(
				Preference::Color("data_color", ColorFromString("#336699"))
				.Label("Data color")
				.Description("Color for generic protocol data bytes"));
			decodes.AddPreference(
				Preference::Color("error_color", ColorFromString("#ff0000"))
				.Label("Error color")
				.Description("Color for malformed data or error conditions"));
			decodes.AddPreference(
				Preference::Color("idle_color", ColorFromString("#404040"))
				.Label("Idle color")
				.Description("Color for idle sequences between meaningful data"));
			decodes.AddPreference(
				Preference::Color("preamble_color", ColorFromString("#808080"))
				.Label("Preamble color")
				.Description("Color for preambles, sync bytes, and other fixed header data"));

		auto& graph = appearance.AddCategory("Filter Graph");
			graph.AddPreference(
				Preference::Color("background_color", ColorFromString("#101010"))
				.Label("Background color")
				.Description("Color for the background of the filter graph editor"));
			graph.AddPreference(
				Preference::Color("node_color", ColorFromString("#404040"))
				.Label("Node color")
				.Description("Color for node background"));
			graph.AddPreference(
				Preference::Color("node_title_text_color", ColorFromString("#000000"))
				.Label("Node title text color")
				.Description("Color for node title"));
			graph.AddPreference(
				Preference::Color("node_text_color", ColorFromString("#ffffff"))
				.Label("Node text color")
				.Description("Color for node text"));
			graph.AddPreference(
				Preference::Color("outline_color", ColorFromString("#009900"))
				.Label("Outline color")
				.Description("Color for node outlines"));
			graph.AddPreference(
				Preference::Font("node_name_font", "sans bold 12")
				.Label("Node name font")
				.Description("Font used for graph node title"));
			graph.AddPreference(
				Preference::Font("port_font", "sans 10")
				.Label("Port font")
				.Description("Font used for port names"));
			graph.AddPreference(
				Preference::Font("param_font", "sans 10")
				.Label("Parameter font")
				.Description("Font used for parameters"));
			graph.AddPreference(
				Preference::Color("analog_port_color", ColorFromString("#000080"))
				.Label("Analog port color")
				.Description("Color for analog node ports"));
			graph.AddPreference(
				Preference::Color("digital_port_color", ColorFromString("#800080"))
				.Label("Digital port color")
				.Description("Color for digital node ports"));
			graph.AddPreference(
				Preference::Color("complex_port_color", ColorFromString("#808000"))
				.Label("Complex port color")
				.Description("Color for complex node ports"));
			graph.AddPreference(
				Preference::Color("line_color", ColorFromString("#c0c0c0"))
				.Label("Line color")
				.Description("Color for lines between nodes"));
			graph.AddPreference(
				Preference::Color("line_highlight_color", ColorFromString("#ff8000"))
				.Label("Highlighted line color")
				.Description("Color for highlighted lines between nodes"));
			graph.AddPreference(
				Preference::Color("disabled_port_color", ColorFromString("#404040"))
				.Label("Disabled port color")
				.Description("Color for ports which cannot be selected in the current mode"));

		auto& general = appearance.AddCategory("General");
			general.AddPreference(
				Preference::Enum("theme", THEME_DARK)
					.Label("GUI Theme")
					.Description("Color scheme for GUI widgets")
					.EnumValue("Light", THEME_LIGHT)
					.EnumValue("Dark", THEME_DARK)
					.EnumValue("Classic", THEME_CLASSIC)
				);

		auto& peaks = appearance.AddCategory("Peaks");
			peaks.AddPreference(
				Preference::Color("peak_outline_color", ColorFromString("#009900"))
				.Label("Outline color")
				.Description("Color for the outline of peak labels"));
			peaks.AddPreference(
				Preference::Color("peak_text_color", ColorFromString("#ffffff"))
				.Label("Text color")
				.Description("Color for the text on peak labels"));
			peaks.AddPreference(
				Preference::Color("peak_background_color", ColorFromString("#000000"))
				.Label("Background color")
				.Description("Color for the background of peak labels"));

		auto& proto = appearance.AddCategory("Protocol Analyzer");
			proto.AddPreference(
				Preference::Color("command_color", ColorFromString("#600050"))
				.Label("Command color")
				.Description("Color for packets that execute commands"));
			proto.AddPreference(
				Preference::Color("control_color", ColorFromString("#808000"))
				.Label("Control color")
				.Description("Color for packets that have control functionality"));
			proto.AddPreference(
				Preference::Color("data_read_color", ColorFromString("#336699"))
				.Label("Data read color")
				.Description("Color for packets that read information from a peripheral"));
			proto.AddPreference(
				Preference::Color("data_write_color", ColorFromString("#339966"))
				.Label("Data write color")
				.Description("Color for packets that write information from a peripheral"));
			proto.AddPreference(
				Preference::Color("error_color", ColorFromString("#800000"))
				.Label("Error color")
				.Description("Color for packets that are malformed or indicate an error condition"));
			proto.AddPreference(
				Preference::Color("status_color", ColorFromString("#000080"))
				.Label("Status color")
				.Description("Color for packets that convey status information"));
			proto.AddPreference(
				Preference::Color("default_color", ColorFromString("#101010"))
				.Label("Default color")
				.Description("Color for packets that don't fit any other category"));

		auto& timeline = appearance.AddCategory("Timeline");
			timeline.AddPreference(
				Preference::Font("tick_label_font", "sans normal 10")
				.Label("Tick font")
				.Description("Font used for tickmark labels on the timeline"));

		auto& toolbar = appearance.AddCategory("Toolbar");
			toolbar.AddPreference(
				Preference::Enum("icon_size", 24)
					.Label("Icon Size")
					.Description("Toolbar icon size, in pixels")
					.EnumValue("24x24", 24)
					.EnumValue("48x48", 48)
				);

		auto& waveforms = appearance.AddCategory("Waveforms");
			waveforms.AddPreference(
				Preference::Font("y_axis_font", "monospace normal 10")
				.Label("Y axis font")
				.Description("Font used for text on the vertical axis of waveforms"));
			waveforms.AddPreference(
				Preference::Real("persist_decay_rate", 0.9)
				.Label("Persistence decay rate (0 = none, 1 = infinite)")
				.Description("Decay rate for persistence waveforms. ")
				.Unit(Unit::UNIT_COUNTS));

		auto& windows = appearance.AddCategory("Windows");
			windows.AddPreference(
				Preference::Color("trigger_bar_color", ColorFromString("#ffffff"))
				.Label("Trigger bar color")
				.Description("Color for the dotted line shown when dragging a trigger"));

	auto& drivers = this->m_treeRoot.AddCategory("Drivers");
		auto& lecroy = drivers.AddCategory("Teledyne LeCroy");
			lecroy.AddPreference(
				Preference::Bool("force_16bit", true)
				.Label("Force 16 bit mode")
				.Description(
					"Force use of 16-bit integer format when downloading sample data from the instrument.\n\n"
					"Even if the instrument only has an 8-bit ADC, due to internal flatness correction and calibration "
					"steps, the internal data representation on the scope has additional significant bits.\n\n"
					"When this setting is disabled, instruments with 8-bit ADCs will use 8-bit integer format for downloading "
					"samples. This slightly improves waveforms-per-second performance but increases quantization noise and "
					"can lead to horizontal \"streak\" artifacts in eye patterns.\n\n"
					"This setting has no effect on instruments with >8 bit ADCs (HDO, WaveSurfer HD, WaveRunner HD, "
					"WavePro HD) which use 16-bit transfer format at all times.\n\n"
					"Changes to this setting take effect the next time a connection to the instrument is opened; "
					"the transfer format for active sessions is not updated."
				));

	auto& files = this->m_treeRoot.AddCategory("Files");
		files.AddPreference(
			Preference::Int("max_recent_files", 10)
			.Label("Max recent files")
			.Description("Maximum number of recent .scopesession file paths to save in history")
			.Unit(Unit::UNIT_COUNTS));

	auto& privacy = this->m_treeRoot.AddCategory("Privacy");
		 privacy.AddPreference(
			Preference::Bool("redact_serial_in_title", false)
			.Label("Redact serial number in title bar")
			.Description(
				"Partially hide instrument serial numbers in the window title bar.\n\n"
				"This allows you to share screenshots without revealing your serial numbers."
			));
}
