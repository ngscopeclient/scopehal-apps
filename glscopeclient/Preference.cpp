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

bool Preference::GetBool() const
{
    if(m_type != PreferenceType::Boolean)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<bool>();
}

double Preference::GetReal() const
{
    if(m_type != PreferenceType::Real)
        throw runtime_error("Preference type mismatch");

    return GetValueRaw<double>();
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
