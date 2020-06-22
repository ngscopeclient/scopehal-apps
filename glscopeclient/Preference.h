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
	@brief  Basic preference class and auxilliary types
 */

#ifndef Preference_H
#define Preference_H

#include <string>
#include <type_traits>
#include <utility>

constexpr std::size_t max(std::size_t a, std::size_t b)
{
    return (a > b) ? a : b;
}

enum class PreferenceType
{
    Boolean,
    String,
    Real
};

class Preference
{
private:
    using PreferenceValue = typename std::aligned_union<
        max(max(sizeof(bool), sizeof(double)), sizeof(std::string)),
        bool, std::string, double
    >::type;

public:
    // Taking string as value and then moving is intended
    Preference(std::string identifier, std::string description, bool defaultValue)
        :   m_identifier{std::move(identifier)}, m_description{std::move(description)},
            m_type{PreferenceType::Boolean}
    {
        new (&m_value) bool(defaultValue);
    }
    
    Preference(std::string identifier, std::string description, std::string defaultValue)
        :   m_identifier{std::move(identifier)}, m_description{std::move(description)},
            m_type{PreferenceType::String}
    {
        new (&m_value) std::string(std::move(defaultValue));
    }
    
    Preference(std::string identifier, std::string description, double defaultValue)
        :   m_identifier{std::move(identifier)}, m_description{std::move(description)},
            m_type{PreferenceType::Real}
    {
        new (&m_value) double(defaultValue);
    }
    
public:
    Preference(const Preference&) = delete;
    Preference(Preference&&) = default;
    
    Preference& operator=(const Preference&) = delete;
    Preference& operator=(Preference&&) = default;
    
public:
    const std::string& get_identifier() const;
    const std::string& get_description() const;
    PreferenceType get_type() const;
    bool get_bool() const;
    double get_real() const;
    const std::string& get_string() const;
    
private:
    template<typename T>
    const T& get_value_raw() const
    {
        return *reinterpret_cast<const T*>(&m_value);
    }
    
private:
    std::string m_identifier;
    std::string m_description;
    PreferenceType m_type;
    PreferenceValue m_value;
};

#endif // Preference_H
