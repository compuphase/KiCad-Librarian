/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog for the report settings (footprint sheet).
 *
 *  Copyright (C) 2013-2018 CompuPhase
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not
 *  use this file except in compliance with the License. You may obtain a copy
 *  of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 *  License for the specific language governing permissions and limitations
 *  under the License.
 *
 *  $Id: libmngr_dlgreport.cpp 5907 2018-12-14 22:05:40Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_dlgreport.h"
#include <wx/fileconf.h>

libmngrDlgReport::libmngrDlgReport( wxWindow* parent )
:
DlgReport( parent )
{
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());

    wxString format = config->Read(wxT("report/paper"), wxT("Letter"));
    long idx = m_choicePageSize->FindString(format);
    if (idx >= 0)
        m_choicePageSize->Select(idx);

    idx = config->Read(wxT("report/layout"), 0L);
    m_radioLayout->SetSelection(idx);

    idx = config->Read(wxT("report/includedescription"), 1L);
    m_chkDescription->SetValue(idx != 0);

    idx = config->Read(wxT("report/drawlabels"), 0L);
    m_chkValueLabels->SetValue(idx != 0);

    idx = config->Read(wxT("report/includepadinfo"), 1L);
    m_chkPadInfo->SetValue(idx != 0);

    idx = config->Read(wxT("report/fplist"), 1L);
    m_chkFPList->SetValue(idx != 0);

    idx = config->Read(wxT("report/index"), 1L);
    m_chkIndex->SetValue(idx != 0);

    idx = config->Read(wxT("report/fontsize"), 8L);
    m_spinFontSize->SetValue(idx);

    delete config;
}

void libmngrDlgReport::OnOK( wxCommandEvent& event )
{
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());

    int idx = m_choicePageSize->GetSelection();
    wxString format;
    if (idx >= 0)
        format = m_choicePageSize->GetString(idx);
    else
        format = wxT("Letter");
    config->Write(wxT("report/paper"), format);

    idx = m_chkDescription->GetValue();
    config->Write(wxT("report/includedescription"), idx);

    idx = m_radioLayout->GetSelection();
    config->Write(wxT("report/layout"), idx);

    idx = m_chkValueLabels->GetValue();
    config->Write(wxT("report/drawlabels"), idx);

    idx = m_chkPadInfo->GetValue();
    config->Write(wxT("report/includepadinfo"), idx);

    idx = m_chkFPList->GetValue();
    config->Write(wxT("report/fplist"), idx);

    idx = m_chkIndex->GetValue();
    config->Write(wxT("report/index"), idx);

    idx = m_spinFontSize->GetValue();
    config->Write(wxT("report/fontsize"), idx);

    delete config;
    event.Skip();
}
