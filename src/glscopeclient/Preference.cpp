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

/**
	@file
	@author Katharina B.
	@brief  Implementation of Preference
 */

#include <stdexcept>

#include "Preference.h"

using namespace std;

namespace impl
{
    PreferenceBuilder::PreferenceBuilder(Preference&& pref)
        : m_pref{move(pref)}
    {

    }

    PreferenceBuilder PreferenceBuilder::Invisible() &&
    {
        this->m_pref.m_isVisible = false;
        return move(*this);
    }

    PreferenceBuilder PreferenceBuilder::Label(std::string label) &&
    {
        this->m_pref.m_label = std::move(label);
        return move(*this);
    }

    PreferenceBuilder PreferenceBuilder::Description(std::string description) &&
    {
        this->m_pref.m_description = std::move(description);
        return move(*this);
    }

    PreferenceBuilder PreferenceBuilder::Unit(Unit::UnitType type) &&
    {
        this->m_pref.m_unit = ::Unit{ type };
        return move(*this);
    }

    Preference PreferenceBuilder::Build() &&
    {
        return move(this->m_pref);
    }
}

void EnumMapping::AddEnumMember(const std::string& name, base_type value)
{
    if(this->m_forwardMap.count(name) != 0)
        throw std::runtime_error("Enum mapping already contains given enum value");

    this->m_forwardMap.insert(std::make_pair(name, value));
    this->m_backwardMap.insert(std::make_pair(value, name));
    this->m_names.push_back(name);
}

const std::string& EnumMapping::GetName(base_type value) const
{
    const auto it = this->m_backwardMap.find(value);

    if(it == this->m_backwardMap.end())
        throw std::runtime_error("Enum mapping doesnt contain requested entry");

    return it->second;
}

bool EnumMapping::HasNameFor(base_type value) const
{
    return this->m_backwardMap.count(value) > 0;
}

bool EnumMapping::HasValueFor(const std::string& name) const
{
    return this->m_forwardMap.count(name) > 0;
}

EnumMapping::base_type EnumMapping::GetValue(const std::string& name) const
{
    const auto it = this->m_forwardMap.find(name);

     if(it == this->m_forwardMap.end())
        throw std::runtime_error("Enum mapping doesnt contain requested entry");

    return it->second;
}

const std::vector<std::string>& EnumMapping::GetNames() const
{
    return this->m_names;
}

void Preference::SetLabel(std::string label)
{
    this->m_label = std::move(label);
}

void Preference::SetDescription(std::string description)
{
    this->m_description = std::move(description);
}

const string& Preference::GetIdentifier() const
{
    return m_identifier;
}

const string& Preference::GetDescription() const
{
    return m_description;
}

PreferenceType Preference::GetType() const
{
    return m_type;
}

bool Preference::GetIsVisible() const
{
    return m_isVisible;
}

bool Preference::GetBool() const
{
    if(m_type != PreferenceType::Boolean)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<bool>();
}

std::int64_t Preference::GetEnumRaw() const
{
    if(m_type != PreferenceType::Enum)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<std::int64_t>();
}

Gdk::Color Preference::GetColor() const
{
    if(m_type != PreferenceType::Color)
        throw runtime_error("Preference type mismatch");

    const auto& value = GetValueRaw<impl::Color>();
    Gdk::Color color{};
    color.set_red(value.m_r);
    color.set_green(value.m_g);
    color.set_blue(value.m_b);
    return color;
}

const impl::Color& Preference::GetColorRaw() const
{
    if(m_type != PreferenceType::Color)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<impl::Color>();
}

double Preference::GetReal() const
{
    if(m_type != PreferenceType::Real)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<double>();
}

bool Preference::HasUnit()
{
    return this->m_unit.GetType() != Unit::UNIT_COUNTS;
}

Unit& Preference::GetUnit()
{
    return this->m_unit;
}

const std::string& Preference::GetString() const
{
    if(m_type != PreferenceType::String)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<std::string>();
}

void Preference::CleanUp()
{
    if(m_hasValue && (m_type == PreferenceType::String || m_type == PreferenceType::Font))
        (reinterpret_cast<string*>(&m_value))->~basic_string();
}

string Preference::ToString() const
{
    switch(m_type)
    {
        case PreferenceType::String:
            return GetString();
        case PreferenceType::Font:
            return GetFontRaw();
        case PreferenceType::Boolean:
            return GetBool() ? "true" : "false";
        case PreferenceType::Real:
            return to_string(GetReal());
        case PreferenceType::Color:
            return "Color";
        case PreferenceType::Enum:
        {
            const auto& mapper = this->GetMapping();
            const auto value = this->GetEnumRaw();
            return mapper.GetName(value);
        }
        default:
            throw runtime_error("tried to retrieve value from preference in moved-from state");
    }
}

void Preference::MoveFrom(Preference& other)
{
    m_type = other.m_type;
    m_identifier = move(other.m_identifier);
    m_description = move(other.m_description);
    m_label = move(other.m_label);
    m_isVisible = move(other.m_isVisible);
    m_unit = move(other.m_unit);
    m_hasValue = move(other.m_hasValue);
    m_mapping = move(other.m_mapping);

    if(m_hasValue)
    {
        switch(other.m_type)
        {
            case PreferenceType::Boolean:
                Construct<bool>(other.GetBool());
                break;

            case PreferenceType::Real:
                Construct<double>(other.GetReal());
                break;

            case PreferenceType::String:
            case PreferenceType::Font:
                Construct<string>(move(other.GetValueRaw<string>()));
                break;

            case PreferenceType::Color:
                Construct<impl::Color>(move(other.GetValueRaw<impl::Color>()));
                break;

            case PreferenceType::Enum:
                Construct<std::int64_t>(move(other.GetValueRaw<std::int64_t>()));
                break;

            default:
                break;
        }
    }

    other.m_type = PreferenceType::None;
}


const std::string& Preference::GetLabel() const
{
    return m_label;
}

const ::std::string& Preference::GetFontRaw() const
{
    if(m_type != PreferenceType::Font)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<std::string>();
}

Pango::FontDescription Preference::GetFont() const
{
    const auto str = this->GetFontRaw();

    return Pango::FontDescription(str.c_str());
}

void Preference::SetFontRaw(const std::string& fontRaw)
{
    CleanUp();
    Construct<string>(fontRaw);
}

void Preference::SetFont(const Pango::FontDescription& font)
{
    string str = font.to_string();
    this->SetFontRaw(str);
}

void Preference::SetBool(bool value)
{
    CleanUp();
    Construct<bool>(value);
}

void Preference::SetReal(double value)
{
    CleanUp();
    Construct<double>(value);
}

void Preference::SetEnumRaw(std::int64_t value)
{
    CleanUp();
    Construct<std::int64_t>(value);
}

void Preference::SetString(string value)
{
    CleanUp();
    Construct<string>(move(value));
}

void Preference::SetColor(const Gdk::Color& color)
{
    CleanUp();
    impl::Color clr{ color.get_red(), color.get_green(), color.get_blue() };
    Construct<impl::Color>(move(clr));
}

void Preference::SetColorRaw(const impl::Color& color)
{
    CleanUp();
    Construct<impl::Color>(color);
}

const EnumMapping& Preference::GetMapping() const
{
    return this->m_mapping;
}

void Preference::SetMapping(EnumMapping mapping)
{
    this->m_mapping = std::move(mapping);
}

impl::PreferenceBuilder Preference::Real(std::string identifier, double defaultValue)
{
    Preference pref(PreferenceType::Real, std::move(identifier));
    pref.Construct<double>(defaultValue);

    return impl::PreferenceBuilder{ std::move(pref) };
}

impl::PreferenceBuilder Preference::Bool(std::string identifier, bool defaultValue)
{
    Preference pref(PreferenceType::Boolean, std::move(identifier));
    pref.Construct<bool>(defaultValue);

    return impl::PreferenceBuilder{ std::move(pref) };
}

impl::PreferenceBuilder Preference::String(std::string identifier, std::string defaultValue)
{
    Preference pref(PreferenceType::String, std::move(identifier));
    pref.Construct<std::string>(defaultValue);

    return impl::PreferenceBuilder{ std::move(pref) };
}

impl::PreferenceBuilder Preference::Color(std::string identifier, const Gdk::Color& defaultValue)
{
    Preference pref(PreferenceType::Color, std::move(identifier));
    pref.Construct<impl::Color>(impl::Color{
        defaultValue.get_red(), defaultValue.get_green(), defaultValue.get_blue()
    });

    return impl::PreferenceBuilder{ std::move(pref) };
}

impl::PreferenceBuilder Preference::EnumRaw(std::string identifier, std::int64_t defaultValue)
{
    Preference pref(PreferenceType::Enum, std::move(identifier));
    pref.Construct<std::int64_t>(defaultValue);

    return impl::PreferenceBuilder{ std::move(pref) };
}

impl::PreferenceBuilder Preference::Font(std::string identifier, std::string defaultValue)
{
    Preference pref(PreferenceType::Font, std::move(identifier));
    pref.Construct<string>(std::move(defaultValue));

    return impl::PreferenceBuilder{ std::move(pref) };
}

