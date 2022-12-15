/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  NSK Plugin
 * Author:   Pavel Kalian
 *
 ******************************************************************************
 * This file is part of the NSK plugin
 * (https://github.com/nohal/nsk_pi).
 *   Copyright (C) 2022 by Pavel Kalian
 *   https://github.com/nohal
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3, or (at your option) any later
 * version of the license.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

#include "nsk.h"
#include "nskgui.h"
#include "pi_common.h"

PLUGIN_BEGIN_NAMESPACE

class NSKPreferencesDialogImpl : public NSKPreferencesDialog {
private:
    NSK* m_nsk;

protected:
    void m_sdbSizerButtonsOnOKButtonClick(wxCommandEvent& event) override;

public:
    NSKPreferencesDialogImpl(NSK* nsk, wxWindow* parent,
        wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxSize(700, 450),
        long style = wxDEFAULT_DIALOG_STYLE);
};

PLUGIN_END_NAMESPACE
