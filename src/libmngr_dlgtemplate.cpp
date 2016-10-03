/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog for adjusting the template variables.
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
 *  $Id: libmngr_dlgtemplate.cpp 5387 2015-10-22 19:31:30Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_dlgtemplate.h"
#include <wx/fileconf.h>


#if !defined sizearray
	#define sizearray(a)	(sizeof(a) / sizeof((a)[0]))
#endif

struct TemplateDefInfo {
	const wxChar* descr;
	const wxChar* name;
	bool percent;
};

static TemplateDefInfo TemplateDefValue[] = {
	{ wxT("Body Pen thickness (mm)"), wxT("BP"), false },
	{ wxT("Text Size for Reference label (mm)"), wxT("TSR"), false },
	{ wxT("Text Size for Value label (mm)"), wxT("TSV"), false },
	{ wxT("Text Weight (%)"), wxT("TW"), true },
	{ wxT("Solder-To-Pad clearance (mm)"), wxT("STP"), false },
	{ wxT("Aux. Pad Solder Reduction (%)"), wxT("PSRA"), true },
};


libmngrDlgTemplateOpts::libmngrDlgTemplateOpts( wxWindow* parent )
:
DlgTemplateOpts( parent )
{
	int count = 0;
	while (TemplateDefValue[count].descr)
		count++;
	m_gridTemplateVars->ClearGrid();
	if (m_gridTemplateVars->GetNumberRows() != count) {
		m_gridTemplateVars->DeleteRows(0, m_gridTemplateVars->GetNumberRows());
		m_gridTemplateVars->InsertRows(0, count);
	}
	m_gridTemplateVars->EnableEditing(true);

	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
	wxString field;
	double value;
	for (int idx = 0; idx < sizearray(TemplateDefValue); idx++) {
		m_gridTemplateVars->SetCellValue((int)idx, 0, TemplateDefValue[idx].descr);
		m_gridTemplateVars->SetCellValue((int)idx, 1, TemplateDefValue[idx].name);
		m_gridTemplateVars->SetReadOnly(idx, 0, true);
		m_gridTemplateVars->SetReadOnly(idx, 1, true);
		field = wxT("template/");
		field += TemplateDefValue[idx].name;
		if (config->Read(field, &value)) {
			if (TemplateDefValue[idx].percent)
				field = wxString::Format(wxT("%d"), (int)value);
			else
				field = wxString::Format(wxT("%.2f"), value);
			m_gridTemplateVars->SetCellValue((int)idx, 2, field);
		}
		m_gridTemplateVars->SetReadOnly(idx, 2, false);
	}
	delete config;

	m_gridTemplateVars->AutoSize();
	m_gridTemplateVars->Fit();
}

void libmngrDlgTemplateOpts::OnOK( wxCommandEvent& event )
{
	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
	/* clear the existing search settings */
	config->DeleteGroup(wxT("template"));

	wxString field;
	double value;
	for (int idx = 0; idx < sizearray(TemplateDefValue); idx++) {
		field = m_gridTemplateVars->GetCellValue(idx, 2);
		if (field.length() > 0 && field.ToDouble(&value)) {
			field = wxT("template/");
			field += TemplateDefValue[idx].name;
			config->Write(field, value);
		}
	}
	delete config;
	event.Skip();
}
