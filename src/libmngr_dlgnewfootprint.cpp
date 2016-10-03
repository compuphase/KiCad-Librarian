/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog with the templates for new components.
 *
 *  Copyright (C) 2013-2016 CompuPhase
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
 *  $Id: libmngr_dlgnewfootprint.cpp 5580 2016-10-03 09:21:56Z thiadmer $
 */

#include "libmngr_dlgnewfootprint.h"
#include "librarymanager.h"
#include "libraryfunctions.h"
#include <wx/dir.h>
#include <wx/filename.h>
#include <wx/textfile.h>

libmngrDlgNewFootprint::libmngrDlgNewFootprint(wxWindow* parent)
    : DlgNewFootprint(parent)
{
	m_libraryname = wxEmptyString;
	m_templatename = wxEmptyString;
	m_partname = wxEmptyString;
    m_previews.Clear();
    m_currentpreview = 0;

	/* list all templates */
	wxDir dir(theApp->GetTemplatePath());
	if (!dir.IsOpened())
		return;	/* error message already given */
	wxArrayString list;
	dir.GetAllFiles(theApp->GetTemplatePath(), &list, wxT("*.mt"), wxDIR_FILES);

	/* remove the paths and extensions on all filenames, but add the brief description */
	for (unsigned idx = 0; idx < list.Count(); idx++) {
		wxFileName fname(list[idx]);
		wxString name = fname.GetName();
		wxString line = GetTemplateHeaderField(name, wxT("brief"), false);
		if (line.length() > 0)
			name += wxT(" - ") + line;
		list[idx] = name;
	}
	list.Sort();
	m_lstTemplates->InsertItems(list, 0);
}

void libmngrDlgNewFootprint::OnTemplateSelect(wxCommandEvent& /*event*/)
{
	int idx = m_lstTemplates->GetSelection();
	if (idx < 0)
		return;
	wxASSERT((unsigned)idx < m_lstTemplates->GetCount());

	/* get the template name */
	wxString line = m_lstTemplates->GetString(idx);
	wxString name = GetToken(&line);  /* the template is the start of the name */

	/* load the base image for the template */
    m_previews.Clear();
	wxString path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + name + wxT(".bmp");
	wxImage bitmap(path, wxBITMAP_TYPE_BMP);
	if (bitmap.IsOk()) {
        m_previews.Add(path);
		m_bmpExample->SetBitmap(bitmap);
    }

	/* get the prefix (which is the name if absent) */
	wxString prefix = GetTemplateHeaderField(name, wxT("prefix"), false);
	if (prefix.length() == 0)
		prefix = name;
	m_txtName->SetValue(prefix);

	/* load the description */
	wxString note = GetTemplateHeaderField(name, wxT("note"), false);
	m_txtDescription->SetValue(note);

    /* count the number of additional images */
    wxString field = GetTemplateHeaderField(name, wxT("model"), false);
    wxArrayString models = wxSplit(field, wxT(' '));
    for (idx = 0; idx < (int)models.Count(); idx++) {
        path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + wxT("model_") + models[idx] + wxT(".bmp");
        if (wxFileExists(path))
            m_previews.Add(path);
    }
    if (m_currentpreview >= (int)m_previews.Count())
        m_currentpreview = m_previews.Count() - 1;
    m_spinPreview->Enable(m_previews.Count() > 1);
}

void libmngrDlgNewFootprint::OnOk(wxCommandEvent& event)
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
		wxMessageBox(wxT("Please select a template and specify a name for the footprint."));
		return;
	}
	wxString prefix = GetTemplateHeaderField(m_templatename, wxT("prefix"), false);
	if (m_templatename.CmpNoCase(m_partname) == 0 || prefix.CmpNoCase(m_partname) == 0) {
		if (wxMessageBox(wxT("The footprint name is the same as the template name or the prefix.\nIs this what you want?"), wxT("Confirm footprint name"), wxYES_NO | wxICON_QUESTION) != wxYES)
			return;
	}

	/* optionally verify whether the footprint already exists in the library */
	if (m_libraryname.length() > 0 && ExistFootprint(m_libraryname, m_partname)) {
		wxString msg = wxString::Format(wxT("Footprint %s already exists.\nOverwrite?"), m_partname.c_str());
		if (wxMessageBox(msg, wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
			return;
	}

	event.Skip();
}

void libmngrDlgNewFootprint::OnNextImage(wxSpinEvent& /*event*/)
{
    if (m_previews.Count() > 1) {
        m_currentpreview = (m_currentpreview < (int)m_previews.Count() - 1) ? m_currentpreview + 1 : 0;
	    wxImage bitmap(m_previews[m_currentpreview], wxBITMAP_TYPE_BMP);
	    if (bitmap.IsOk())
    		m_bmpExample->SetBitmap(bitmap);
    }
}

void libmngrDlgNewFootprint::OnPrevImage(wxSpinEvent& /*event*/)
{
    if (m_previews.Count() > 1) {
        m_currentpreview = (m_currentpreview > 0) ? m_currentpreview - 1 : m_previews.Count() - 1;
	    wxImage bitmap(m_previews[m_currentpreview], wxBITMAP_TYPE_BMP);
	    if (bitmap.IsOk())
    		m_bmpExample->SetBitmap(bitmap);
    }
}
