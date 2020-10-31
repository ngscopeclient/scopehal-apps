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
	@brief  A window that allows manipulation of preference values
 */
 
#ifndef PreferenceDialog_h
#define PreferenceDialog_h

#include <memory>
#include <vector>
#include "glscopeclient.h"
#include "PreferenceManager.h"

class OscilloscopeWindow;

namespace impl
{
    class PreferenceRowBase
    {
        public:
            PreferenceRowBase(Preference& preference);
            virtual ~PreferenceRowBase();

        public:
            virtual Gtk::Widget& GetValueWidget() = 0;

        public:
            Gtk::Label& GetLabelWidget();
            const std::string& GetIdentifier();

        protected:
            std::string m_identifier;
            Gtk::Label m_label;
    };

    class BooleanRow
        : public PreferenceRowBase
    {
        public:
            BooleanRow(Preference& preference)
                : PreferenceRowBase(preference)
            {
                this->m_check.set_active(preference.GetBool());
            }

        public:
            virtual Gtk::Widget& GetValueWidget();
            Gtk::CheckButton& GetCheckBox();

        protected:
            Gtk::CheckButton m_check;
    };

    class StringRealRow
        : public PreferenceRowBase
    {
        public:
            StringRealRow(Preference& preference);

        public:
            virtual Gtk::Widget& GetValueWidget();
            Gtk::Entry& GetEntry();

        protected:
            Gtk::Entry m_value;
    };

    class ColorRow
        : public PreferenceRowBase
    {
        public:
            ColorRow(Preference& preference)
                : PreferenceRowBase(preference)
            {
                this->m_colorbutton.set_color(preference.GetColor());
            }

        public:
            virtual Gtk::Widget& GetValueWidget();
            Gtk::ColorButton& GetColorButton();

        protected:
            Gtk::ColorButton m_colorbutton;
    };

    class EnumRow
        : public PreferenceRowBase
    {
        protected:
            struct ModelColumns
                : public Gtk::TreeModel::ColumnRecord
            {
                ModelColumns()
                {
                    add(m_col_name);
                }

                Gtk::TreeModelColumn<Glib::ustring> m_col_name;
            };

        public:
            EnumRow(Preference& pref);

        public:
            std::string GetActiveName();
            virtual Gtk::Widget& GetValueWidget();

        protected:
            ModelColumns m_columns;
            Glib::RefPtr<Gtk::ListStore> m_refTreeModel;
            Gtk::ComboBox m_value;
    };

    class FontRow
        : public PreferenceRowBase
    {
        public:
            FontRow(Preference& pref);

        public:
            virtual Gtk::Widget& GetValueWidget();
            Gtk::FontButton& GetFontButton();

        protected:
            Gtk::FontButton m_button;
    };


    class PreferencePage
        : public Gtk::Grid
    {
        public:
            PreferencePage(PreferenceCategory& category);

        public:
            void SaveChanges();

        protected:
            void CreateWidgets();

        protected:
            PreferenceCategory& m_category;
            std::vector<std::unique_ptr<PreferenceRowBase>> m_rows;
    };
}

class PreferenceDialog
    : public Gtk::Dialog
{
protected:
    class ModelColumns : public Gtk::TreeModel::ColumnRecord
    {
    public:
        ModelColumns()
        {
            add(m_col_category); add(m_col_page);
        }

        Gtk::TreeModelColumn<Glib::ustring> m_col_category;
        Gtk::TreeModelColumn<void*> m_col_page;
    };

public:
    PreferenceDialog(OscilloscopeWindow* parent, PreferenceManager& preferences);
 
    void SaveChanges();
 
protected:
    void CreateWidgets();
    void SetupTree();
    void ProcessCategory(PreferenceCategory& category, Gtk::TreeModel::Row& parent);
    void ProcessRootCategories(PreferenceCategory& root);
    void OnSelectionChanged();
    void ActivatePage(impl::PreferencePage* page);

protected:
    PreferenceManager& m_preferences;
    std::vector<std::unique_ptr<impl::PreferencePage>> m_pages;
    ModelColumns m_columns;
    Glib::RefPtr<Gtk::TreeStore> m_treeModel;

    Gtk::Paned m_root;
        Gtk::ScrolledWindow m_wnd;
                Gtk::TreeView m_tree;
        Gtk::Grid m_grid;
};

#endif // PreferenceDialog_h
