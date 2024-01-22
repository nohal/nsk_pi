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

#include "nskguiimpl.h"

PLUGIN_BEGIN_NAMESPACE

NSKPreferencesDialogImpl::NSKPreferencesDialogImpl(NSK* nsk, wxWindow* parent,
    wxWindowID id, const wxString& title, const wxPoint& pos,
    const wxSize& size, long style)
    : NSKPreferencesDialog(parent, id, title, pos, size, style)
    , m_nsk(nsk)
{
    m_tUnimplemented->SetValue(m_nsk->Unimplemented());
    m_stTotalUnimplemented->SetLabelText(
        wxString::Format("%lu", m_nsk->TotalUnimplemented()));
    m_tUnknown->SetValue(m_nsk->Unknown());
    m_stTotalUnknown->SetLabelText(
        wxString::Format("%lu", m_nsk->TotalUnknown()));
    for (auto known : m_nsk->Known()) {
        m_clKnown->Append(known.talker_tag);
        m_clKnown->Check(m_clKnown->GetCount() - 1, known.enabled);
    }
    auto format
        = "Input data rate: %lu sentences/s, output data rate: %lu deltas/s";
    auto sz
        = std::snprintf(nullptr, 0, format, m_nsk->NMEARate(), m_nsk->SKRate());
    std::string output(sz + 1, '\0');
    std::sprintf(&output[0], format, m_nsk->NMEARate(), m_nsk->SKRate());
    m_stDataRate->SetLabelText(output);
    format = "Input total: %lu, Deltas total: %lu";
    sz = std::snprintf(
        nullptr, 0, format, m_nsk->NMEATotal(), m_nsk->SKTotal());
    output.resize(sz + 1, '\0');
    std::sprintf(&output[0], format, m_nsk->NMEATotal(), m_nsk->SKTotal());
    m_stTotals->SetLabelText(output);
}

void NSKPreferencesDialogImpl::m_sdbSizerButtonsOnOKButtonClick(
    wxCommandEvent& event)
{
    size_t i = 0;
    for (auto item : m_clKnown->GetStrings()) {
        m_nsk->UpdateKnown(
            known_sentence(item.ToStdString(), m_clKnown->IsChecked(i)));
        ++i;
    }
    event.Skip();
}

PLUGIN_END_NAMESPACE
