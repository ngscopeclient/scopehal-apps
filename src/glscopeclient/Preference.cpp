/***********************************************************************************************************************
*                                                                                                                      *
* ANTIKERNEL v0.1                                                                                                      *
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

    PreferenceBuilder&& PreferenceBuilder::IsVisible(bool isVisible) &&
    {
        this->m_pref.m_isVisible = isVisible;
        return move(*this);
    }

    PreferenceBuilder&& PreferenceBuilder::WithUnit(Unit::UnitType type) &&
    {
        this->m_pref.m_unit = Unit{ type };
        return move(*this);
    }

    Preference&& PreferenceBuilder::Build() &&
    {
        return move(this->m_pref);
    }
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
    if(m_type == PreferenceType::String)
        (reinterpret_cast<string*>(&m_value))->~basic_string();
}

string Preference::ToString() const
{
    switch(m_type)
    {
        case PreferenceType::String:
            return GetString();
        case PreferenceType::Boolean:
            return GetBool() ? "true" : "false";
        case PreferenceType::Real:
            return to_string(GetReal());
        case PreferenceType::Color:
            return "Color";
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
    
    switch(other.m_type)
    {
        case PreferenceType::Boolean:
            Construct<bool>(other.GetBool());
            break;
            
        case PreferenceType::Real:
            Construct<double>(other.GetReal());
            break;
            
        case PreferenceType::String:
            Construct<string>(move(other.GetValueRaw<string>()));
            break;

        case PreferenceType::Color:
            Construct<impl::Color>(move(other.GetValueRaw<impl::Color>()));
            break;
            
        default:
            break;
    }
    
    other.m_type = PreferenceType::None;
}


const std::string& Preference::GetLabel() const
{
    return m_label;
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

impl::PreferenceBuilder Preference::New(std::string identifier, std::string label, std::string description, bool defaultValue)
{
    return impl::PreferenceBuilder(
        Preference(std::move(identifier), std::move(label), std::move(description), defaultValue)
    );
}

impl::PreferenceBuilder Preference::New(std::string identifier, std::string label, std::string description, double defaultValue)
{
    return impl::PreferenceBuilder(
        Preference(std::move(identifier), std::move(label), std::move(description), defaultValue)
    );
}

impl::PreferenceBuilder Preference::New(std::string identifier, std::string label, std::string description, const char* defaultValue)
{
    return impl::PreferenceBuilder(
        Preference(std::move(identifier), std::move(label), std::move(description), defaultValue)
    );
}

impl::PreferenceBuilder Preference::New(std::string identifier, std::string label, std::string description, std::string defaultValue)
{
    return impl::PreferenceBuilder(
        Preference(std::move(identifier), std::move(label), std::move(description), std::move(defaultValue))
    );
}
