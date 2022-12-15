///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-254-gc2ef7767)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

#include <wx/artprov.h>
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/colour.h>
#include <wx/dialog.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/intl.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/stattext.h>
#include <wx/string.h>
#include <wx/textctrl.h>
#include <wx/xrc/xmlres.h>

///////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
/// Class NSKPreferencesDialog
///////////////////////////////////////////////////////////////////////////////
class NSKPreferencesDialog : public wxDialog {
private:
protected:
    wxStaticText* m_stDataRate;
    wxStaticText* m_stTotals;
    wxCheckListBox* m_clKnown;
    wxTextCtrl* m_tUnimplemented;
    wxStaticText* m_stTotalUnimplemented;
    wxTextCtrl* m_tUnknown;
    wxStaticText* m_stTotalUnknown;
    wxStdDialogButtonSizer* m_sdbSizerButtons;
    wxButton* m_sdbSizerButtonsOK;
    wxButton* m_sdbSizerButtonsCancel;

    // Virtual event handlers, override them in your derived class
    virtual void m_sdbSizerButtonsOnCancelButtonClick(wxCommandEvent& event)
    {
        event.Skip();
    }
    virtual void m_sdbSizerButtonsOnOKButtonClick(wxCommandEvent& event)
    {
        event.Skip();
    }

public:
    NSKPreferencesDialog(wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxString& title = wxEmptyString,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxSize(700, 450),
        long style = wxDEFAULT_DIALOG_STYLE);

    ~NSKPreferencesDialog();
};
