/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog with the templates for new symbols.
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
 *  $Id: libmngr_dlgnewsymbol.cpp 5387 2015-10-22 19:31:30Z thiadmer $
 */
#include "libmngr_dlgnewsymbol.h"
#include "librarymanager.h"
#include "libraryfunctions.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/textfile.h>

libmngrDlgNewSymbol::libmngrDlgNewSymbol( wxWindow* parent )
:
DlgNewSymbol( parent )
{
	m_libraryname = wxEmptyString;
	m_templatename = wxEmptyString;
	m_partname = wxEmptyString;
	m_partref = wxEmptyString;

	/* list all templates */
	wxDir dir(theApp->GetTemplatePath());
	if (!dir.IsOpened())
		return;	/* error message already given */
	wxArrayString list;
	dir.GetAllFiles(theApp->GetTemplatePath(), &list, wxT("*.st"), wxDIR_FILES);

	/* remove the paths and extensions on all filenames, but add the brief description */
	for (unsigned idx = 0; idx < list.Count(); idx++) {
		wxFileName fname(list[idx]);
		wxString name = fname.GetName();
		wxString line = GetTemplateHeaderField(name, wxT("brief"), true);
		if (line.length() > 0)
			name += wxT(" - ") + line;
		list[idx] = name;
	}
	list.Sort();
	m_lstTemplates->InsertItems(list, 0);
}

void libmngrDlgNewSymbol::OnTemplateSelect( wxCommandEvent& /*event*/ )
{
	int idx = m_lstTemplates->GetSelection();
	if (idx < 0)
		return;
	wxASSERT((unsigned)idx < m_lstTemplates->GetCount());

	/* get the template name */
	wxString line = m_lstTemplates->GetString(idx);
	wxString name = GetToken(&line);  /* the template is the start of the name */

	/* load the image for the template */
	wxString path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + name + wxT(".bmp");
	wxImage bitmap(path, wxBITMAP_TYPE_BMP);
	if (bitmap.IsOk())
		m_bmpExample->SetBitmap(bitmap);

	/* get the prefix */
	wxString prefix = GetTemplateHeaderField(name, wxT("prefix"), true);
	if (prefix.length() == 0)
		prefix = wxT("U");
	m_txtPrefix->SetValue(prefix);

	/* load the description */
	wxString note = GetTemplateHeaderField(name, wxT("note"), true);
	m_txtDescription->SetValue(note);
}

void libmngrDlgNewSymbol::OnOk( wxCommandEvent& event )
{
	/* save the selected template & part names */
	int idx = m_lstTemplates->GetSelection();
	if (idx >= 0) {
		wxASSERT((unsigned)idx < m_lstTemplates->GetCount());
		wxString line = m_lstTemplates->GetString(idx);
		m_templatename = GetToken(&line);
	}

	m_partname = m_txtName->GetValue();
	if (m_templatename.length() == 0 || m_partname.length() == 0) {
		wxMessageBox(wxT("Please select a template and specify a name for the symbol."));
		return;
	}
	if (m_templatename.CmpNoCase(m_partname) == 0) {
		if (wxMessageBox(wxT("The symbol name is the same as the template name.\nIs this what you want?"), wxT("Confirm symbol name"), wxYES_NO | wxICON_QUESTION) != wxYES)
			return;
	}

	m_partref = m_txtPrefix->GetValue();
	if (m_partref.length() == 0)
		m_partref = wxT("U");

	/* optionally verify whether the symol already exists in the library */
	if (m_libraryname.length() > 0 && ExistSymbol(m_libraryname, m_partname)) {
		wxString msg = wxString::Format(wxT("Symbol %s already exists.\nOverwrite?"), m_partname.c_str());
		if (wxMessageBox(msg, wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
			return;
	}

	event.Skip();
}
