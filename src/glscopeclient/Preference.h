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

#ifndef Preference_h
#define Preference_h

#include <string>
#include <type_traits>
#include <utility>
#include <type_traits>
#include <cstdint>

#include <giomm.h>
#include <gtkmm.h>

#include "Unit.h"

constexpr std::size_t max(std::size_t a, std::size_t b)
{
    return (a > b) ? a : b;
}

enum class PreferenceType
{
    Boolean,
    String,
    Real,
    Color,
    None // Only for moved-from values
};

namespace impl
{
    class PreferenceBuilder;

    struct Color
    {
        Color(std::uint16_t r, std::uint16_t g, std::uint16_t b)
            : m_r{r}, m_g{g}, m_b{b}
        {

        }

        std::uint16_t m_r, m_g, m_b;
    };
}

class EnumMapping
{
    using base_type = signed long long;

    public:
        EnumMapping()
        {

        }

    public:
        void AddEnumMember(const std::string& name, base_type value);
        const std::string& GetName(base_type value);
        base_type GetValue(const std::string& name);
        const std::vector<std::string>& GetNames();

    public:
        template<
            typename E,
            typename = typename std::enable_if<
                std::is_convertible<E, base_type>::value &&
                not std::is_same<E, base_type>::value
            >::type
        >
        void AddEnumMember(const std::string& name, E value)
        {
            const auto val = static_cast<base_type>(value);
            this->AddEnumMember(name, val);
        }

    protected:
        std::map<std::string, base_type> m_forwardMap;
        std::map<base_type, std::string> m_backwardMap;
        std::vector<std::string> m_names;
};

class Preference
{
    friend class impl::PreferenceBuilder;

private:
    using PreferenceValue = typename std::aligned_union<
        max(sizeof(impl::Color), max(max(sizeof(bool), sizeof(double)), sizeof(std::string))),
        bool, std::string, double, impl::Color
    >::type;

public:
    Preference(PreferenceType type, std::string identifier)
        : m_identifier{std::move(identifier)}, m_type{type}
    {

    }
    
    ~Preference()
    {
        CleanUp();
    }

public:
    Preference(const Preference&) = delete;
    Preference(Preference&& other)
    {
        MoveFrom(other);
    }
    
    Preference& operator=(const Preference&) = delete;
    Preference& operator=(Preference&& other)
    {
        CleanUp();
        MoveFrom(other);
        return *this;
    }
    
public:
    const std::string& GetIdentifier() const;
    const std::string& GetLabel() const;
    const std::string& GetDescription() const;
    PreferenceType GetType() const;
    bool GetBool() const;
    double GetReal() const;
    const std::string& GetString() const;
    std::string ToString() const;
    bool GetIsVisible() const;
    Gdk::Color GetColor() const;
    const impl::Color& GetColorRaw() const;
    void SetBool(bool value);
    void SetReal(double value);
    void SetString(std::string value);
    void SetColor(const Gdk::Color& value);
    void SetColorRaw(const impl::Color& value);
    void SetLabel(std::string label);
    void SetDescription(std::string description);
    bool HasUnit();
    Unit& GetUnit();

public:
    static impl::PreferenceBuilder Real(std::string identifier, double defaultValue);
    static impl::PreferenceBuilder Bool(std::string identifier, bool defaultValue);
    static impl::PreferenceBuilder String(std::string identifier, std::string defaultValue);
    static impl::PreferenceBuilder Color(std::string identifier, const Gdk::Color& defaultValue);
    
private:
    template<typename T>
    const T& GetValueRaw() const
    {
        return *reinterpret_cast<const T*>(&m_value);
    }
    
    template<typename T>
    T& GetValueRaw()
    {
        return *reinterpret_cast<T*>(&m_value);
    }
    
    void CleanUp();
    
    template<typename T>
    void Construct(T value)
    {
        new (&m_value) T(std::move(value));
        m_hasValue = true;
    }
    
    void MoveFrom(Preference& other);
    
private:
    std::string m_identifier;
    std::string m_label;
    std::string m_description;
    PreferenceType m_type;
    PreferenceValue m_value;
    bool m_isVisible{true};
    Unit m_unit{Unit::UNIT_COUNTS};
    bool m_hasValue{false};
};

namespace impl
{
    class PreferenceBuilder
    {
        public:
            PreferenceBuilder(Preference&& pref);

        public:
            PreferenceBuilder Invisible() &&;
            PreferenceBuilder Label(std::string label) &&;
            PreferenceBuilder Description(std::string description) &&;
            PreferenceBuilder Unit(Unit::UnitType type) &&;
            Preference Build() &&;

        protected:
            Preference m_pref;
    };
}

#endif // Preference_h
