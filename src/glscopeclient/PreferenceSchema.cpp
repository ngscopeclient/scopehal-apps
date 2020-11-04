/***********************************************************************************************************************
*                                                                                                                      *
* glscopeclient                                                                                                        *
*                                                                                                                      *
* Copyright (c) 2012-2020 Andrew D. Zonenberg                                                                          *
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

enum class TestEnum
{
    Choice1 = 0,
    Choice2,
    Choice3
};

void PreferenceManager::InitializeDefaults()
{
    auto& appearance = this->m_treeRoot.AddCategory("Appearance");
        auto& cursors = appearance.AddCategory("Cursors");
            cursors.AddPreference(
                Preference::Color("cursor_1_color", Gdk::Color("yellow"))
                .Label("Cursor #1 color")
                .Description("Color for the left or top cursor"));
            cursors.AddPreference(
                Preference::Color("cursor_2_color", Gdk::Color("orange"))
                .Label("Cursor #2 color")
                .Description("Color for the right or bottom cursor"));
            cursors.AddPreference(
                Preference::Color("cursor_fill_color", Gdk::Color("yellow"))
                .Label("Cursor fill color")
                .Description("Color for the filled area between cursors"));
            cursors.AddPreference(
                Preference::Color("cursor_fill_text_color", Gdk::Color("yellow"))
                .Label("Cursor fill text color")
                .Description("Color for in-band power and other text drawn between cursors"));
        auto& decode_colors = appearance.AddCategory("Decodes");
            decode_colors.AddPreference(
                Preference::Color("address_color", Gdk::Color("#ffff00"))
                .Label("Address color")
                .Description("Color for register/memory addresses"));
             decode_colors.AddPreference(
                Preference::Color("checksum_bad_color", Gdk::Color("#ff0000"))
                .Label("Checksum/CRC color (Bad)")
                .Description("Color for incorrect checksums/CRCs"));
            decode_colors.AddPreference(
                Preference::Color("checksum_ok_color", Gdk::Color("#00ff00"))
                .Label("Checksum/CRC color (OK)")
                .Description("Color for correct checksums/CRCs"));
            decode_colors.AddPreference(
                Preference::Color("control_color", Gdk::Color("#c000a0"))
                .Label("Control color")
                .Description("Color for control events"));
            decode_colors.AddPreference(
                Preference::Color("data_color", Gdk::Color("#336699"))
                .Label("Data color")
                .Description("Color for generic protocol data bytes"));
            decode_colors.AddPreference(
                Preference::Color("error_color", Gdk::Color("#ff0000"))
                .Label("Error color")
                .Description("Color for malformed data or error conditions"));
            decode_colors.AddPreference(
                Preference::Color("idle_color", Gdk::Color("#404040"))
                .Label("Idle color")
                .Description("Color for idle sequences between meaningful data"));
            decode_colors.AddPreference(
                Preference::Color("preamble_color", Gdk::Color("#808080"))
                .Label("Preamble color")
                .Description("Color for preambles, sync bytes, and other fixed header data"));
        auto& peaks = appearance.AddCategory("Peaks");
            peaks.AddPreference(
                Preference::Color("peak_outline_color", Gdk::Color("#009900"))
                .Label("Outline color")
                .Description("Color for the outline of peak labels"));
            peaks.AddPreference(
                Preference::Color("peak_text_color", Gdk::Color("#ffffff"))
                .Label("Text color")
                .Description("Color for the text on peak labels"));
            peaks.AddPreference(
                Preference::Color("peak_background_color", Gdk::Color("#000000"))
                .Label("Background color")
                .Description("Color for the background of peak labels"));

        auto& proto = appearance.AddCategory("Protocol Analyzer");
            proto.AddPreference(
                Preference::Color("command_color", Gdk::Color("#600050"))
                .Label("Command color")
                .Description("Color for packets that execute commands"));
            proto.AddPreference(
                Preference::Color("control_color", Gdk::Color("#808000"))
                .Label("Control color")
                .Description("Color for packets that have control functionality"));
            proto.AddPreference(
                Preference::Color("data_read_color", Gdk::Color("#336699"))
                .Label("Data read color")
                .Description("Color for packets that read information from a peripheral"));
            proto.AddPreference(
                Preference::Color("data_write_color", Gdk::Color("#339966"))
                .Label("Data write color")
                .Description("Color for packets that write information from a peripheral"));
            proto.AddPreference(
                Preference::Color("error_color", Gdk::Color("#800000"))
                .Label("Error color")
                .Description("Color for packets that are malformed or indicate an error condition"));
            proto.AddPreference(
                Preference::Color("status_color", Gdk::Color("#000080"))
                .Label("Status color")
                .Description("Color for packets that convey status information"));
            proto.AddPreference(
                Preference::Color("default_color", Gdk::Color("#101010"))
                .Label("Default color")
                .Description("Color for packets that don't fit any other category"));


        auto& windows = appearance.AddCategory("Windows");
            windows.AddPreference(
                Preference::Color("insertion_bar_color", Gdk::Color("yellow"))
                .Label("Insertion bar color (insert)")
                .Description("Color for the insertion bar when dragging a waveform within a group"));
            windows.AddPreference(
                Preference::Color("insertion_bar_split_color", Gdk::Color("orange"))
                .Label("Insertion bar color (split)")
                .Description("Color for the insertion bar when splitting a waveform group"));

    auto& instrument = this->m_treeRoot.AddCategory("Instrument");
        auto& trans = instrument.AddCategory("Transports");
            trans.AddPreference(
                Preference::String("test_string", "string")
                .Label("Test string")
                .Description("First test value"));
            trans.AddPreference(
                Preference::Real("test_real", 42.09)
                .Label("Test real")
                .Description("Second test value")
                .Unit(Unit::UNIT_VOLTS));
            trans.AddPreference(
                Preference::Bool("test_bool", true)
                .Label("Test boolean")
                .Description("Third test value"));
            trans.AddPreference(
                Preference::Enum("test_enum", TestEnum::Choice1)
                .Label("Test enum")
                .Description("Fourth test value")
                .EnumValue("Choice 1", TestEnum::Choice1)
                .EnumValue("Choice 2", TestEnum::Choice2)
                .EnumValue("Choice 3", TestEnum::Choice3)
            );
            trans.AddPreference(
                Preference::Font("test_font", "sans normal 10")
                .Label("Test font")
                .Description("Fifth test value")
            );
        auto& decode = instrument.AddCategory("Decoders");
            decode.AddPreference(
                Preference::Real("hidden_setting", 13.37)
                .Invisible()
                .Unit(Unit::UNIT_CELSIUS));
}
