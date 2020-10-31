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
	@brief Implementation of the preference dialog class.
 */

#include "PreferenceDialog.h"

#include <utility>
#include <limits>
#include <memory>
#include <algorithm>
#include <string>
#include <iostream>

using namespace std;

namespace impl
{
    PreferenceRowBase::PreferenceRowBase(Preference& preference)
        : m_identifier{preference.GetIdentifier()}
    {
        this->m_label.set_label(preference.GetLabel());
        this->m_label.set_halign(Gtk::ALIGN_START);
        this->m_label.set_label(preference.GetLabel());
        this->m_label.set_tooltip_text(preference.GetDescription());
    }
    
    PreferenceRowBase::~PreferenceRowBase()
    {

    }

    Gtk::Label& PreferenceRowBase::GetLabelWidget()
    {
        return this->m_label;
    }

    const std::string& PreferenceRowBase::GetIdentifier()
    {
        return this->m_identifier;
    }

    Gtk::Widget& BooleanRow::GetValueWidget()
    {
        return this->m_check;
    }

    Gtk::CheckButton& BooleanRow::GetCheckBox()
    {
        return this->m_check;
    }

    Gtk::Widget& StringRealRow::GetValueWidget()
    {
        return this->m_value;
    }
    
    Gtk::Entry& StringRealRow::GetEntry()
    {
        return this->m_value;
    }

    StringRealRow::StringRealRow(Preference& preference)
        : PreferenceRowBase(preference)
    {
         std::string text;

        if(preference.GetType() == PreferenceType::Real)
        {
            if(preference.HasUnit())
            {
                auto unit = preference.GetUnit();
                text = unit.PrettyPrint(preference.GetReal());
            }
            else
            {
                text = to_string(preference.GetReal());
            }             
        }
        else
        {
            text = preference.GetString();
        }

        this->m_value.set_text(text);
    }

    Gtk::Widget& ColorRow::GetValueWidget()
    {
        return this->m_colorbutton;
    }

    Gtk::ColorButton& ColorRow::GetColorButton()
    {
        return this->m_colorbutton;
    }

    Gtk::Widget& EnumRow::GetValueWidget()
    {
        return this->m_value;
    }

    EnumRow::EnumRow(Preference& pref)
        : PreferenceRowBase(pref)
    {
        const auto& mapper = pref.GetMapping();

        this->m_refTreeModel = Gtk::ListStore::create(this->m_columns);
        this->m_value.set_model(this->m_refTreeModel);

        for(const auto& name: mapper.GetNames())
        {
            Gtk::TreeModel::Row row = *(m_refTreeModel->append());
            row[this->m_columns.m_col_name] = name.c_str();
            const auto value = mapper.GetValue(name);

            if(pref.GetEnumRaw() == value)
                this->m_value.set_active(row);
        }

        this->m_value.pack_start(this->m_columns.m_col_name);
    }

    std::string EnumRow::GetActiveName()
    {
        Gtk::TreeModel::iterator iter = this->m_value.get_active();
        Gtk::TreeModel::Row row = *iter;

        Glib::ustring str = row[this->m_columns.m_col_name];
        return (std::string)str;
    }

    PreferencePage::PreferencePage(PreferenceCategory& category)
        : m_category{category}
    {
        set_row_spacing(5);
        set_column_spacing(150);
        CreateWidgets();
    }

    void PreferencePage::CreateWidgets()
    {
        Gtk::Widget* last = nullptr;

        auto& entries = m_category.GetChildren();

        for(const auto& identifier: m_category.GetOrdering())
        {
            auto& node = entries[identifier];

            if(!node->IsPreference())
                continue;

            auto& preference = node->AsPreference();

            if(!preference.GetIsVisible())
                continue;

            std::unique_ptr<PreferenceRowBase> row;

            switch(preference.GetType())       
            {
                case PreferenceType::Enum:
                {
                    row = unique_ptr<PreferenceRowBase>{ new EnumRow(preference) };       
                    break;
                }

                case PreferenceType::Boolean:
                {
                    row = unique_ptr<PreferenceRowBase>{ new BooleanRow(preference) };  
                    break;
                }
                
                case PreferenceType::Color:
                {
                    row = unique_ptr<PreferenceRowBase>{ new ColorRow(preference) };  
                    break;
                }

                case PreferenceType::Real:
                case PreferenceType::String:
                {
                    row = unique_ptr<PreferenceRowBase>{ new StringRealRow(preference) };            
                    break;
                }   
                    
                default:
                    break;
            }
        
        
            row->GetValueWidget().set_halign(Gtk::ALIGN_CENTER);
            row->GetValueWidget().set_tooltip_text(preference.GetDescription());
                    
            if(!last)
                attach(row->GetLabelWidget(), 0, 0, 1, 1);
            else
                attach_next_to(row->GetLabelWidget(), *last, Gtk::POS_BOTTOM, 1, 1);
                
            attach_next_to(row->GetValueWidget(), row->GetLabelWidget(), Gtk::POS_RIGHT, 1, 1);
            
            last = &(row->GetLabelWidget());
            this->m_rows.push_back(std::move(row));
        }  
    }

    void PreferencePage::SaveChanges()
    {
        for(auto& entry: m_category.GetChildren())
        {
            auto* node = entry.second.get();

            if(!node->IsPreference())
                continue;

            auto& preference = node->AsPreference();

            const auto it = find_if(m_rows.begin(), m_rows.end(),
                [&preference](const unique_ptr<PreferenceRowBase>& x) -> bool 
                {
                    return x->GetIdentifier() == preference.GetIdentifier(); 
                });

            PreferenceRowBase* rowBase = it->get();

            switch(preference.GetType())
            {
                case PreferenceType::Color:
                {
                    ColorRow* row = dynamic_cast<ColorRow*>(rowBase);
                    preference.SetColor(row->GetColorButton().get_color());
                    break;
                }

                case PreferenceType::Enum:
                {
                    EnumRow* row = dynamic_cast<EnumRow*>(rowBase);
                    const auto& mapping = preference.GetMapping();
                    preference.SetEnumRaw(mapping.GetValue(row->GetActiveName()));   
                    break;
                }

                case PreferenceType::Boolean:
                {
                    BooleanRow* row = dynamic_cast<BooleanRow*>(rowBase);
                    preference.SetBool(row->GetCheckBox().get_active());    
                    break;
                }
                
                case PreferenceType::Real:
                case PreferenceType::String:
                {
                    StringRealRow* row = dynamic_cast<StringRealRow*>(rowBase);
                        
                    const auto text = row->GetEntry().get_text();
                        
                    if(preference.GetType() == PreferenceType::Real)
                    {
                        try
                        {
                            if(preference.HasUnit())
                            {
                                auto unit = preference.GetUnit();
                                preference.SetReal(unit.ParseString(text));
                            }
                            else
                            {
                                preference.SetReal(stod(text));
                            }
                        }
                        catch(...)
                        {
                            LogError("Ignoring value %s for preference %s: Wrong number format", text.c_str(), preference.GetIdentifier().c_str());
                        }
                    }
                    else
                    {
                        preference.SetString(text);
                    }
                    
                    break;
                }
                
                default:
                    break;
            }
        }
    }
}

PreferenceDialog::PreferenceDialog(OscilloscopeWindow* parent, PreferenceManager& preferences)
    : Gtk::Dialog("Preferences", *parent, Gtk::DIALOG_MODAL)
    , m_preferences(preferences)
{
    set_position(Gtk::WIN_POS_CENTER);
    CreateWidgets();
    show_all();
}

void PreferenceDialog::SetupTree()
{
    m_treeModel = Gtk::TreeStore::create(m_columns);
    m_tree.set_model(m_treeModel);
    m_tree.append_column("Category", m_columns.m_col_category);

    m_tree.set_headers_visible(false);

    auto& prefTree = m_preferences.AllPreferences();
    this->ProcessRootCategories(prefTree);

    m_tree.get_selection()->signal_changed().connect(
		sigc::mem_fun(*this, &PreferenceDialog::OnSelectionChanged));
}

void PreferenceDialog::OnSelectionChanged()
{
    auto selection = m_tree.get_selection();
	if(!selection->count_selected_rows())
		return;

	auto row = *selection->get_selected();
    void* newPageVp = row[m_columns.m_col_page];
    impl::PreferencePage* newPage = reinterpret_cast<impl::PreferencePage*>(newPageVp);

    ActivatePage(newPage);
}

void PreferenceDialog::ActivatePage(impl::PreferencePage* page)
{
    auto* child = m_root.get_child2();

    if(child)
        m_root.remove(*child);

    m_root.add2(*page);
    show_all();
}

void PreferenceDialog::ProcessCategory(PreferenceCategory& category, Gtk::TreeModel::Row& parent)
{
    auto& children = category.GetChildren();
    for(const auto& identifier: category.GetOrdering())
    {
        auto& node = children[identifier];

        if(node->IsCategory())
        {
            auto& subCategory = node->AsCategory();

            if(subCategory.IsVisible())
            {
                unique_ptr<impl::PreferencePage> pagePtr{ new impl::PreferencePage(subCategory) };

                Gtk::TreeModel::Row childrow = *(m_treeModel->append(parent.children()));
                childrow[m_columns.m_col_category] = identifier.c_str();
                childrow[m_columns.m_col_page] = pagePtr.get();

                ProcessCategory(subCategory, childrow);
                m_pages.push_back(std::move(pagePtr));
            }
        }
    }
}

void PreferenceDialog::ProcessRootCategories(PreferenceCategory& root)
{
    auto& children = root.GetChildren();
    for(const auto& identifier: root.GetOrdering())
    {
        auto& node = children[identifier];

        if(node->IsCategory())
        {
            auto& subCategory = node->AsCategory();

            if(subCategory.IsVisible())
            {
                unique_ptr<impl::PreferencePage> pagePtr{ new impl::PreferencePage(subCategory) };

                Gtk::TreeModel::Row row = *(m_treeModel->append());
                row[m_columns.m_col_category] = identifier.c_str();
                row[m_columns.m_col_page] = pagePtr.get();
                ProcessCategory(subCategory, row);
                m_pages.push_back(std::move(pagePtr));
            }
        }
    }
}

void PreferenceDialog::CreateWidgets()
{
    resize(650, 500);
    add_button("OK", Gtk::RESPONSE_OK);
	add_button("Cancel", Gtk::RESPONSE_CANCEL);
    set_deletable(false);

    SetupTree();

    get_vbox()->pack_start(m_root, Gtk::PACK_EXPAND_WIDGET);
        m_root.add1(m_wnd);
            m_wnd.add(m_tree);
            m_wnd.set_policy(Gtk::POLICY_AUTOMATIC, Gtk::POLICY_AUTOMATIC);
        m_root.set_position(200);
}

void PreferenceDialog::SaveChanges()
{
    for(auto& page: m_pages)
        page->SaveChanges();
    
    m_preferences.SavePreferences();
}
 
 
