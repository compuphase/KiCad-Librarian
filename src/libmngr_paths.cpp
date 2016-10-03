/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog for the search paths settings.
 *
 *  Copyright (C) 2013-2015 CompuPhase
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
 *  $Id: libmngr_paths.cpp 5387 2015-10-22 19:31:30Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_paths.h"
#include <wx/dirdlg.h>
#include <wx/fileconf.h>

libmngrDlgPaths::libmngrDlgPaths( wxWindow* parent )
:
DlgPaths( parent )
{
	/* fill in the list of search paths */
	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
	wxString path;
	wxString key;
	int idx = 1;
	for ( ;; ) {
		key = key.Format(wxT("paths/footprints%d"), idx);
		if (!config->Read(key, &path))
			break;
		m_lstFootprints->AppendString(path);
		idx++;
	}
	idx = 1;
	for ( ;; ) {
		key = key.Format(wxT("paths/symbols%d"), idx);
		if (!config->Read(key, &path))
			break;
		m_lstSymbols->AppendString(path);
		idx++;
	}

	delete config;

	m_btnRemoveFootprint->Enable(false);
	m_btnRemoveSymbol->Enable(false);
}

void libmngrDlgPaths::OnFootprintPathSelect( wxCommandEvent& event )
{
	int idx = m_lstFootprints->GetSelection();
	m_btnRemoveFootprint->Enable(idx >= 0 && idx < (int)m_lstFootprints->GetCount());
	event.Skip();
}

void libmngrDlgPaths::OnAddFootprintPath( wxCommandEvent& /*event*/ )
{
	/* open the add-path dialog */
	wxDirDialog dlg(NULL, wxT("Select search path"), wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (dlg.ShowModal() == wxID_OK) {
		wxString path = dlg.GetPath();
		m_lstFootprints->AppendString(path);
	}
}

void libmngrDlgPaths::OnRemoveFootprintPath( wxCommandEvent& /*event*/ )
{
	int idx = m_lstFootprints->GetSelection();
	wxASSERT(idx >= 0 && idx < (int)m_lstFootprints->GetCount());
	m_lstFootprints->Delete(idx);
	m_btnRemoveFootprint->Enable(false);
}

void libmngrDlgPaths::OnSymbolPathSelect( wxCommandEvent& event )
{
	int idx = m_lstSymbols->GetSelection();
	m_btnRemoveSymbol->Enable(idx >= 0 && idx < (int)m_lstSymbols->GetCount());
	event.Skip();
}

void libmngrDlgPaths::OnAddSymbolPath( wxCommandEvent& /*event*/ )
{
	/* open the add-path dialog */
	wxDirDialog dlg(NULL, wxT("Select search path"), wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
	if (dlg.ShowModal() == wxID_OK) {
		wxString path = dlg.GetPath();
		m_lstSymbols->AppendString(path);
	}
}

void libmngrDlgPaths::OnRemoveSymbolPath( wxCommandEvent& /*event*/ )
{
	int idx = m_lstSymbols->GetSelection();
	wxASSERT(idx >= 0 && idx < (int)m_lstSymbols->GetCount());
	m_lstSymbols->Delete(idx);
	m_btnRemoveSymbol->Enable(false);
}

void libmngrDlgPaths::OnOK( wxCommandEvent& event )
{
	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());

	/* clear the existing search paths */
	config->DeleteGroup(wxT("paths"));

	/* save the search paths */
	wxString path;
	wxString key;
	for (int idx = 0; idx < (int)m_lstFootprints->GetCount(); idx++) {
		path = m_lstFootprints->GetString(idx);
		if (path.IsEmpty())
			break;
		key = key.Format(wxT("paths/footprints%d"), idx + 1);
		config->Write(key, path);
	}
	for (int idx = 0; idx < (int)m_lstSymbols->GetCount(); idx++) {
		path = m_lstSymbols->GetString(idx);
		if (path.IsEmpty())
			break;
		key = key.Format(wxT("paths/symbols%d"), idx + 1);
		config->Write(key, path);
	}

	delete config;
	event.Skip();
}
