///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1-254-gc2ef7767)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include "nskgui.h"

///////////////////////////////////////////////////////////////////////////

NSKPreferencesDialog::NSKPreferencesDialog(wxWindow* parent, wxWindowID id,
    const wxString& title, const wxPoint& pos, const wxSize& size, long style)
    : wxDialog(parent, id, title, pos, size, style)
{
    this->SetSizeHints(wxDefaultSize, wxDefaultSize);

    wxBoxSizer* bSizerMain;
    bSizerMain = new wxBoxSizer(wxVERTICAL);

    m_stDataRate = new wxStaticText(this, wxID_ANY,
        _("Input data rate: %d sentences/s, output data rate: %d deltas/s"),
        wxDefaultPosition, wxDefaultSize, 0);
    m_stDataRate->Wrap(-1);
    bSizerMain->Add(m_stDataRate, 0, wxALL, 5);

    m_stTotals = new wxStaticText(this, wxID_ANY,
        _("Input total: %d, Deltas total: %d"), wxDefaultPosition,
        wxDefaultSize, 0);
    m_stTotals->Wrap(-1);
    bSizerMain->Add(m_stTotals, 0, wxALL, 5);

    wxBoxSizer* bSizerSentences;
    bSizerSentences = new wxBoxSizer(wxHORIZONTAL);

    wxStaticBoxSizer* sbSizerKnown;
    sbSizerKnown = new wxStaticBoxSizer(
        new wxStaticBox(this, wxID_ANY, _("Known (uncheck to ignore)")),
        wxVERTICAL);

    wxArrayString m_clKnownChoices;
    m_clKnown = new wxCheckListBox(sbSizerKnown->GetStaticBox(), wxID_ANY,
        wxDefaultPosition, wxDefaultSize, m_clKnownChoices, wxLB_SORT);
    sbSizerKnown->Add(m_clKnown, 1, wxALL | wxEXPAND, 5);

    bSizerSentences->Add(sbSizerKnown, 2, wxEXPAND, 5);

    wxStaticBoxSizer* sbSizerUnimplemented;
    sbSizerUnimplemented = new wxStaticBoxSizer(
        new wxStaticBox(this, wxID_ANY, _("Unimplemented")), wxVERTICAL);

    m_tUnimplemented = new wxTextCtrl(sbSizerUnimplemented->GetStaticBox(),
        wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);
    sbSizerUnimplemented->Add(m_tUnimplemented, 1, wxALL | wxEXPAND, 5);

    m_stTotalUnimplemented
        = new wxStaticText(sbSizerUnimplemented->GetStaticBox(), wxID_ANY,
            _("XXX"), wxDefaultPosition, wxDefaultSize, 0);
    m_stTotalUnimplemented->Wrap(-1);
    sbSizerUnimplemented->Add(m_stTotalUnimplemented, 0, wxALL, 5);

    bSizerSentences->Add(sbSizerUnimplemented, 1, wxEXPAND, 5);

    wxStaticBoxSizer* sbSizerUnknown;
    sbSizerUnknown = new wxStaticBoxSizer(
        new wxStaticBox(this, wxID_ANY, _("Unknown")), wxVERTICAL);

    m_tUnknown = new wxTextCtrl(sbSizerUnknown->GetStaticBox(), wxID_ANY,
        wxEmptyString, wxDefaultPosition, wxDefaultSize,
        wxTE_MULTILINE | wxTE_READONLY);
    sbSizerUnknown->Add(m_tUnknown, 1, wxALL | wxEXPAND, 5);

    m_stTotalUnknown = new wxStaticText(sbSizerUnknown->GetStaticBox(),
        wxID_ANY, _("XXX"), wxDefaultPosition, wxDefaultSize, 0);
    m_stTotalUnknown->Wrap(-1);
    sbSizerUnknown->Add(m_stTotalUnknown, 0, wxALL, 5);

    bSizerSentences->Add(sbSizerUnknown, 1, wxEXPAND, 5);

    bSizerMain->Add(bSizerSentences, 1, wxEXPAND, 5);

    m_sdbSizerButtons = new wxStdDialogButtonSizer();
    m_sdbSizerButtonsOK = new wxButton(this, wxID_OK);
    m_sdbSizerButtons->AddButton(m_sdbSizerButtonsOK);
    m_sdbSizerButtonsCancel = new wxButton(this, wxID_CANCEL);
    m_sdbSizerButtons->AddButton(m_sdbSizerButtonsCancel);
    m_sdbSizerButtons->Realize();

    bSizerMain->Add(m_sdbSizerButtons, 0, wxALL | wxEXPAND, 5);

    this->SetSizer(bSizerMain);
    this->Layout();

    this->Centre(wxBOTH);

    // Connect Events
    m_sdbSizerButtonsCancel->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(
            NSKPreferencesDialog::m_sdbSizerButtonsOnCancelButtonClick),
        NULL, this);
    m_sdbSizerButtonsOK->Connect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(
            NSKPreferencesDialog::m_sdbSizerButtonsOnOKButtonClick),
        NULL, this);
}

NSKPreferencesDialog::~NSKPreferencesDialog()
{
    // Disconnect Events
    m_sdbSizerButtonsCancel->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(
            NSKPreferencesDialog::m_sdbSizerButtonsOnCancelButtonClick),
        NULL, this);
    m_sdbSizerButtonsOK->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED,
        wxCommandEventHandler(
            NSKPreferencesDialog::m_sdbSizerButtonsOnOKButtonClick),
        NULL, this);
}
