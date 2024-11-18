/***********************************************************************************************************************
*                                                                                                                      *
* ngscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2024 Andrew D. Zonenberg                                                                          *
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

		auto& consts = appearance.AddCategory("Constellations");
			consts.AddPreference(
				Preference::Color("point_color", ColorFromString("#ff0000ff"))
				.Label("Point color")
				.Description("Color for nominal constellation points"));

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
			/*cursors.AddPreference(
				Preference::Color("cursor_fill_text_color", ColorFromString("#ffff00"))
				.Label("Cursor fill text color")
				.Description("Color for in-band power and other text drawn between cursors"));*/
			cursors.AddPreference(
				Preference::Font("label_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
				.Label("Label font")
				.Description("Font used for cursor labels"));
			cursors.AddPreference(
				Preference::Color("marker_color", ColorFromString("#ff00a0"))
				.Label("Marker color")
				.Description("Color for markers"));
			cursors.AddPreference(
				Preference::Color("hover_color", ColorFromString("#ffffff80"))
				.Label("Hover color")
				.Description("Color for the hovered-packet indicator"));

		auto& decodes = appearance.AddCategory("Decodes");
			decodes.AddPreference(
				Preference::Font("protocol_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
				.Label("Protocol font")
				.Description("Font used for protocol decode overlay text"));
			/*decodes.AddPreference(
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
				.Description("Color for preambles, sync bytes, and other fixed header data"));*/

		auto& eye = appearance.AddCategory("Eye Patterns");
			eye.AddPreference(
				Preference::Color("border_color_pass", ColorFromString("#00ff00ff"))
				.Label("Border color (pass)")
				.Description("Color for drawing mask polygon border if no or acceptable violations"));
			eye.AddPreference(
				Preference::Color("border_color_fail", ColorFromString("#ff0000ff"))
				.Label("Border color (fail)")
				.Description("Color for drawing mask polygon border if unacceptable violations"));
			eye.AddPreference(
				Preference::Color("mask_color", ColorFromString("#0000ff80"))
				.Label("Mask color")
				.Description("Color for drawing mask overlays"));

		auto& file = appearance.AddCategory("File Browser");
			file.AddPreference(
				Preference::Enum("dialogmode", BROWSER_NATIVE)
					.Label("Non-fullscreened dialog style")
					.Description(
						"Select the file browser to use for loading and saving files when not in fullscreen mode.\n\n"

						"The native file browser cannot be used when ngscopeclient is in fullscreen mode,\n"
						"so the ImGui browser is always used when fullscreened."
						)
					.EnumValue("ImGui", BROWSER_IMGUI)
					.EnumValue("Native", BROWSER_NATIVE)
					.EnumValue("KDialog", BROWSER_KDIALOG)
				);

		auto& graph = appearance.AddCategory("Filter Graph");
			graph.AddPreference(
				Preference::Font("header_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 15))
				.Label("Header font")
				.Description("Font for filter/channel names"));
			graph.AddPreference(
				Preference::Color("header_text_color", ColorFromString("#000000"))
				.Label("Header text color")
				.Description("Color for filter/channel names"));
			graph.AddPreference(
				Preference::Color("valid_link_color", ColorFromString("#00ff00"))
				.Label("Valid link color")
				.Description("Color indicating a potential connection path is valid"));
			graph.AddPreference(
				Preference::Color("invalid_link_color", ColorFromString("#ff0000"))
				.Label("Invalid link color")
				.Description("Color indicating a potential connection path is invalid"));
			graph.AddPreference(
				Preference::Font("icon_caption_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
				.Label("Icon font")
				.Description("Font for icon captions"));
			graph.AddPreference(
				Preference::Color("icon_caption_color", ColorFromString("#ffffff"))
				.Label("Icon color")
				.Description("Color for icon captions"));

		auto& stream = appearance.AddCategory("Stream Browser");
			stream.AddPreference(
				Preference::Bool("use_7_segment_display", true)
				.Label("Use 7 segment style display")
				.Description("Use 7 segment style display for DMM and PSU values"));
			stream.AddPreference(
				Preference::Real("instrument_badge_latch_duration", 0.4)
				.Label("Intrument badge latch duration (seconds)")
				.Description("Duration during which instrument badges are preserved (to prevent flashing)."));
			stream.AddPreference(
				Preference::Color("download_wait_badge_color", ColorFromString("#CC4C4C"))
				.Label("Download wait badge color")
				.Description("Color for download 'wait' badge"));
			stream.AddPreference(
				Preference::Color("download_progress_badge_color", ColorFromString("#B3B44D"))
				.Label("Download progress badge color")
				.Description("Color for download 'progress' badge"));
			stream.AddPreference(
				Preference::Color("download_finished_badge_color", ColorFromString("#4CCC4C"))
				.Label("Download finished badge color")
				.Description("Color for download 'finished' badge"));
			stream.AddPreference(
				Preference::Color("download_active_badge_color", ColorFromString("#4CCC4C"))
				.Label("Download active badge color")
				.Description("Color for download 'active' badge"));
			stream.AddPreference(
				Preference::Color("trigger_armed_badge_color", ColorFromString("#4CCC4C"))
				.Label("Trigger armed badge color")
				.Description("Color for trigger 'armed' badge"));
			stream.AddPreference(
				Preference::Color("trigger_stopped_badge_color", ColorFromString("#CC4C4C"))
				.Label("Trigger stopped badge color")
				.Description("Color for trigger 'stopped' badge"));
			stream.AddPreference(
				Preference::Color("trigger_triggered_badge_color", ColorFromString("#B3B44D"))
				.Label("Trigger triggered badge color")
				.Description("Color for trigger 'triggered' badge"));
			stream.AddPreference(
				Preference::Color("trigger_busy_badge_color", ColorFromString("#CC4C4C"))
				.Label("Trigger buwy badge color")
				.Description("Color for trigger 'busy' badge"));
			stream.AddPreference(
				Preference::Color("trigger_auto_badge_color", ColorFromString("#4CCC4C"))
				.Label("Trigger auto badge color")
				.Description("Color for trigger 'auto' badge"));
			stream.AddPreference(
				Preference::Color("instrument_disabled_badge_color", ColorFromString("#666666"))
				.Label("Instrument disabled badge color")
				.Description("Color for instrument 'disabled' badge"));
			stream.AddPreference(
				Preference::Color("instrument_offline_badge_color", ColorFromString("#CC4C4C"))
				.Label("Instrument offline badge color")
				.Description("Color for instrument 'offline' badge"));
			stream.AddPreference(
				Preference::Color("instrument_on_badge_color", ColorFromString("#4CCC4C"))
				.Label("Instrument on badge color")
				.Description("Color for instrument 'on' badge"));
			stream.AddPreference(
				Preference::Color("instrument_partial_badge_color", ColorFromString("#E2CD23FF"))
				.Label("Instrument partial on badge color")
				.Description("Color for intrument partial 'on' badge"));
			stream.AddPreference(
				Preference::Color("instrument_off_badge_color", ColorFromString("#CC4C4C"))
				.Label("Instrument off badge color")
				.Description("Color for instrument 'off' badge"));
			stream.AddPreference(
				Preference::Color("psu_cv_badge_color", ColorFromString("#4CCC4C"))
				.Label("PSU cv badge color")
				.Description("Color for PSU 'cv' badge"));
			stream.AddPreference(
				Preference::Color("psu_cc_badge_color", ColorFromString("#CC4C4C"))
				.Label("PCU CC badge color")
				.Description("Color for psu 'c' badge"));
			stream.AddPreference(
				Preference::Color("psu_set_label_color", ColorFromString("#FFFF00"))
				.Label("PSU set label color")
				.Description("Color for PSU 'set' label"));
			stream.AddPreference(
				Preference::Color("psu_meas_label_color", ColorFromString("#00C100"))
				.Label("PSU measured label color")
				.Description("Color for PSU 'meas.' label"));
			stream.AddPreference(
				Preference::Color("psu_7_segment_color", ColorFromString("#B2FFFF"))
				.Label("PSU 7 segment display color")
				.Description("Color for PSU 7 segment style display"));
			stream.AddPreference(
				Preference::Color("awg_hiz_badge_color", ColorFromString("#666600"))
				.Label("Function Generator HI-Z badge color")
				.Description("Color for Function Generator 'HI-Z' badge"));
			stream.AddPreference(
				Preference::Color("awg_50ohms_badge_color", ColorFromString("#B54C85"))
				.Label("Function Generator 50 Ohms badge color")
				.Description("Color for Function Generator '50Ohm' badge"));

		auto& general = appearance.AddCategory("General");
			general.AddPreference(
				Preference::Enum("theme", THEME_DARK)
					.Label("GUI Theme")
					.Description("Color scheme for GUI widgets")
					.EnumValue("Light", THEME_LIGHT)
					.EnumValue("Dark", THEME_DARK)
					.EnumValue("Classic", THEME_CLASSIC)
				);
			general.AddPreference(
				Preference::Font("default_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
				.Label("Default font")
				.Description("Font used for most GUI elements"));
			general.AddPreference(
				Preference::Font("title_font", FontDescription(FindDataFile("fonts/DejaVuSans-Bold.ttf"), 16))
				.Label("Title font")
				.Description("Font used for headings in reports or wizards"));
			general.AddPreference(
				Preference::Font("console_font", FontDescription(FindDataFile("fonts/DejaVuSansMono.ttf"), 13))
				.Label("Console font")
				.Description("Font used for SCPI console and log viewer"));

		auto& graphs = appearance.AddCategory("Graphs");
			graphs.AddPreference(
				Preference::Color("bottom_color", ColorFromString("#000000ff"))
				.Label("Background color bottom")
				.Description("Color for the bottom side of the background gradient in a waveform graph"));
			graphs.AddPreference(
				Preference::Color("top_color", ColorFromString("#202020ff"))
				.Label("Background color top")
				.Description("Color for the top side of the background gradient in a waveform graph"));
			graphs.AddPreference(
				Preference::Color("grid_centerline_color", ColorFromString("#c0c0c0ff"))
				.Label("Grid centerline color")
				.Description("Color for the grid line at Y=0"));
			graphs.AddPreference(
				Preference::Color("grid_color", ColorFromString("#c0c0c040"))
				.Label("Grid color")
				.Description("Color for grid lines at Y=0"));
			graphs.AddPreference(
				Preference::Real("grid_centerline_width", 1)
				.Label("Axis width")
				.Description("Width of grid line at Y=0"));
			graphs.AddPreference(
				Preference::Real("grid_width", 1)
				.Label("Grid width")
				.Description("Width of grid lines"));
			graphs.AddPreference(
				Preference::Color("y_axis_text_color", ColorFromString("#ffffffff"))
				.Label("Y axis text color")
				.Description("Color for Y axis text"));
			graphs.AddPreference(
				Preference::Font("y_axis_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
					.Label("Y axis font")
					.Description("Font used for Y axis text"));

		auto& logs = appearance.AddCategory("Log Viewer");
			logs.AddPreference(
				Preference::Color("error_color", ColorFromString("#800000"))
				.Label("Error color")
				.Description("Background color for log messages with \"error\" severity"));

			logs.AddPreference(
				Preference::Color("warning_color", ColorFromString("#404000"))
				.Label("Warning color")
				.Description("Background color for log messages with \"warning\" severity"));

		auto& markdown = appearance.AddCategory("Markdown");

			markdown.AddPreference(
				Preference::Font("heading_1_font", FontDescription(FindDataFile("fonts/DejaVuSans-Bold.ttf"), 20))
					.Label("Heading 1 font")
					.Description("Font used for level 1 headings in Markdown"));

			markdown.AddPreference(
				Preference::Font("heading_2_font", FontDescription(FindDataFile("fonts/DejaVuSans-Bold.ttf"), 16))
					.Label("Heading 2 font")
					.Description("Font used for level 2 headings in Markdown"));

			markdown.AddPreference(
				Preference::Font("heading_3_font", FontDescription(FindDataFile("fonts/DejaVuSans-Bold.ttf"), 14))
					.Label("Heading 3 font")
					.Description("Font used for level 3 headings in Markdown"));

		auto& peaks = appearance.AddCategory("Peaks");
			/*peaks.AddPreference(
				Preference::Color("peak_outline_color", ColorFromString("#009900"))
				.Label("Outline color")
				.Description("Color for the outline of peak labels"));*/
			peaks.AddPreference(
				Preference::Color("peak_text_color", ColorFromString("#ffffff"))
				.Label("Text color")
				.Description("Color for the text on peak labels"));
			/*peaks.AddPreference(
				Preference::Color("peak_background_color", ColorFromString("#000000"))
				.Label("Background color")
				.Description("Color for the background of peak labels"));
				*/
			peaks.AddPreference(
				Preference::Font("label_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 13))
					.Label("Label font")
					.Description("Font used for peak labels"));


		auto& proto = appearance.AddCategory("Protocol Analyzer");
			/*proto.AddPreference(
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
				.Description("Color for packets that don't fit any other category"));*/
			proto.AddPreference(
				Preference::Font("data_font", FontDescription(FindDataFile("fonts/DejaVuSansMono.ttf"), 13))
					.Label("Data font")
					.Description("Font used for packet data hex dumps"));

		auto& timeline = appearance.AddCategory("Timeline");
			timeline.AddPreference(
				Preference::Color("axis_color", ColorFromString("#ffffff"))
				.Label("Axis color")
				.Description("Color for the X axis line and tick marks"));
			timeline.AddPreference(
				Preference::Color("text_color", ColorFromString("#ffffff"))
				.Label("Text color")
				.Description("Color for text labels on the X axis"));
			timeline.AddPreference(
				Preference::Color("trigger_bar_color", ColorFromString("#ffffff40"))
				.Label("Trigger bar color")
				.Description("Color for the vertical position line shown when dragging a trigger"));
			timeline.AddPreference(
				Preference::Font("x_axis_font", FontDescription(FindDataFile("fonts/DejaVuSans.ttf"), 15))
				.Label("X axis font")
				.Description("Font used for X axis text"));

		auto& toolbar = appearance.AddCategory("Toolbar");
			toolbar.AddPreference(
				Preference::Enum("icon_size", 24)
					.Label("Icon Size")
					.Description("Toolbar icon size, in pixels")
					.EnumValue("24x24", 24)
					.EnumValue("48x48", 48)
				);

		/*auto& waveforms = appearance.AddCategory("Waveforms");
			waveforms.AddPreference(
				Preference::Real("persist_decay_rate", 0.9)
				.Label("Persistence decay rate (0 = none, 1 = infinite)")
				.Description("Decay rate for persistence waveforms. ")
				.Unit(Unit::UNIT_COUNTS));
		*/
		auto& windows = appearance.AddCategory("Windowing");
			windows.AddPreference(
				Preference::Enum("viewport_mode", VIEWPORT_ENABLE)
					.Label("Viewport Mode")
					.Description(
						"Specifies whether the GUI library is allowed to create multiple top level windows,\n"
						"or if all child windows (menus, dialogs, tooltips, etc) are forced to stay within the\n"
						"boundaries of the application window.\n"
						"\n"
						"The default is multi-window, but if you are having problems with a Linux tiling\n"
						"window manager, you may have a better experience using single-window mode.\n"
						"\n"
						"Changes to this setting will not take effect until ngscopeclient is restarted."
						)
					.EnumValue("Multi window", VIEWPORT_ENABLE)
					.EnumValue("Single window", VIEWPORT_DISABLE)
				);

	auto& drivers = this->m_treeRoot.AddCategory("Drivers");
		auto& dgeneral = drivers.AddCategory("General");
			dgeneral.AddPreference(
				Preference::Enum("headless_startup", HEADLESS_STARTUP_C1_ONLY)
				.Label("Headless scope default state")
				.Description(
				"Select the set of channels which are active by default on PC-attached oscilloscopes\n"
				"with no front panel display of their own. "
				)
				.EnumValue("All non-MSO channels", HEADLESS_STARTUP_ALL_NON_MSO)
				.EnumValue("Channel 1 only", HEADLESS_STARTUP_C1_ONLY) );
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
		auto& siglent = drivers.AddCategory("Siglent SDS HD");
			siglent.AddPreference(
				Preference::Enum("data_width", WIDTH_AUTO)
					.Label("Data Width")
					.Description(
					"Data width used when downloading sample data from the instrument.\n\n"
					"Even if the instrument has a 12-bit ADC, using 8 rather than 16 bit data format allows (about 10%) faster"
					" waveform update rate.\n\n"
					"Choose 16 bit mode if you want to privilege data accuracy over refresh rate.\n\n"
					"Changes to this setting take effect the next time a connection to the instrument is opened; "
					"the transfer format for active sessions is not updated."
						)
					.EnumValue("Auto", WIDTH_AUTO)
					.EnumValue("8 bits", WIDTH_8_BITS)
					.EnumValue("16 bits", WIDTH_16_BITS)
				);
		auto& rigol = drivers.AddCategory("Rigol DHO");
			rigol.AddPreference(
				Preference::Enum("data_width", WIDTH_AUTO)
					.Label("Data Width")
					.Description(
					"Data width used when downloading sample data from the instrument.\n\n"
					"Even if the instrument has a 12-bit ADC, using 8 rather than 16 bit data format allows faster"
					" waveform update rate.\n\n"
					"Choose 16 bit mode if you want to privilege data accuracy over refresh rate.\n\n"
					"Changes to this setting take effect the next time a connection to the instrument is opened; "
					"the transfer format for active sessions is not updated."
						)
					.EnumValue("Auto", WIDTH_AUTO)
					.EnumValue("8 bits", WIDTH_8_BITS)
					.EnumValue("16 bits", WIDTH_16_BITS)
				);

	auto& files = this->m_treeRoot.AddCategory("Files");
		files.AddPreference(
			Preference::Int("max_recent_files", 10)
			.Label("Max recent files")
			.Description("Maximum number of recent .scopesession file paths to save in history")
			.Unit(Unit::UNIT_COUNTS));

	auto& misc = this->m_treeRoot.AddCategory("Miscellaneous");
		auto& menus = misc.AddCategory("Menus");
			menus.AddPreference(
				Preference::Int("recent_instrument_count", 20)
				.Label("Recent instrument count")
				.Description("Number of recently used instruments to display"));

	auto& pwr = this->m_treeRoot.AddCategory("Power");
		auto& events = pwr.AddCategory("Events");
			events.AddPreference(
				Preference::Enum("event_driven_ui", 0)
					.Label("Event loop mode")
					.Description(
						"Specify how the main event loop should operate.\n"
						"\n"
						"In Performance mode, the event loop runs at a constant speed locked to the display\n"
						"refresh rate. This results in the smoothest GUI and maximum waveform update, but the\n"
						"constant redraws increase power consumption.\n"
						"\n"
						"In Power mode, the event loop blocks until a GUI event (keystroke, mouse movement, etc.)\n"
						"occurs, or a user-specified timeout elapses. This results in more jerky display updates\n"
						"but keeps the CPU idle most of the time, saving power."
						)
					.EnumValue("Performance", 0)
					.EnumValue("Power", 1)
				);
			events.AddPreference(
				Preference::Real("polling_timeout", FS_PER_SECOND / 4)
				.Label("Polling timeout")
				.Unit(Unit::UNIT_FS)
				.Description(
					"Polling timeout for event loop in power-optimized mode.\n\n"
					"Longer timeout values reduce power consumption, but also slows display updates.\n")
				);


	/*
	auto& privacy = this->m_treeRoot.AddCategory("Privacy");
		 privacy.AddPreference(
			Preference::Bool("redact_serial_in_title", false)
			.Label("Redact serial number in title bar")
			.Description(
				"Partially hide instrument serial numbers in the window title bar.\n\n"
				"This allows you to share screenshots without revealing your serial numbers."
			));*/
}
