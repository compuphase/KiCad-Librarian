/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  The dialog for the user-interface settings.
 *
 *  Copyright (C) 2013-2017 CompuPhase
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
 *  $Id: libmngr_dlgoptions.cpp 5405 2015-11-20 09:40:55Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_dlgoptions.h"
#include <wx/fileconf.h>

libmngrDlgOptions::libmngrDlgOptions( wxWindow* parent )
:
DlgOptions( parent )
{
	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
	long idx;

	idx = config->Read(wxT("display/fontsize"), 8L);
	m_spinFontSize->SetValue(idx);

	idx = config->Read(wxT("display/dimoffset"), 50L);
	m_spinDimOffset->SetValue(idx);

    wxColour clrFootprintMode;
	config->Read(wxT("display/footprintbkgnd"), &clrFootprintMode, wxColour(0, 0, 0));
    m_colourFootprint->SetColour(clrFootprintMode);

    wxColour clrSchematicMode;
	config->Read(wxT("display/schematicbkgnd"), &clrSchematicMode, wxColour(255, 255, 255));
    m_colourSchematic->SetColour(clrSchematicMode);

    wxColour clr3DModelMode;
	config->Read(wxT("display/3dmodelbkgnd"), &clr3DModelMode, wxColour(0, 64, 0));
    m_colour3DModel->SetColour(clr3DModelMode);

	bool showlabels;
	config->Read(wxT("display/showlabels"), &showlabels, true);
	m_chkDrawLabels->SetValue(showlabels);

	bool centrecross;
	config->Read(wxT("display/centrecross"), &centrecross, true);
	m_chkDrawCentreCross->SetValue(centrecross);

	bool fullpaths;
	config->Read(wxT("display/fullpath"), &fullpaths, false);
	m_chkFullPaths->SetValue(fullpaths);

	bool copyvrml;
	config->Read(wxT("settings/copyvrml"), &copyvrml, true);
	m_chkCopyVRML->SetValue(copyvrml);

	bool disabletemplate;
	config->Read(wxT("settings/disabletemplate"), &disabletemplate, false);
	m_chkDisableTemplates->SetValue(disabletemplate);

	bool confirmoverwrite;
	config->Read(wxT("settings/confirmoverwrite"), &confirmoverwrite, true);
	m_chkConfirmOverwrite->SetValue(confirmoverwrite);

	bool confirmdelete;
	config->Read(wxT("settings/confirmdelete"), &confirmdelete, true);
	m_chkConfirmDelete->SetValue(confirmdelete);

	bool reloadsession;
	config->Read(wxT("settings/reloadsession"), &reloadsession, true);
	m_chkReloadSession->SetValue(reloadsession);

	delete config;
}

void libmngrDlgOptions::OnOK( wxCommandEvent& event )
{
	wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
	int idx;

	idx = m_spinFontSize->GetValue();
	config->Write(wxT("display/fontsize"), idx);

	idx = m_spinDimOffset->GetValue();
	config->Write(wxT("display/dimoffset"), idx);

    wxColour clrFootprintMode = m_colourFootprint->GetColour();
	config->Write(wxT("display/footprintbkgnd"), clrFootprintMode);

    wxColour clrSchematicMode = m_colourSchematic->GetColour();
	config->Write(wxT("display/schematicbkgnd"), clrSchematicMode);

    wxColour clr3DModelMode = m_colour3DModel->GetColour();
	config->Write(wxT("display/3dmodelbkgnd"), clr3DModelMode);

	bool showlabels = m_chkDrawLabels->GetValue();
	config->Write(wxT("display/showlabels"), showlabels);

	bool centrecross = m_chkDrawCentreCross->GetValue();
	config->Write(wxT("display/centrecross"), centrecross);

	bool fullpaths = m_chkFullPaths->GetValue();
	config->Write(wxT("display/fullpath"), fullpaths);

	bool copyvrml = m_chkCopyVRML->GetValue();
	config->Write(wxT("settings/copyvrml"), copyvrml);

	bool disabletemplate = m_chkDisableTemplates->GetValue();
	config->Write(wxT("settings/disabletemplate"), disabletemplate);

	bool confirmoverwrite = m_chkConfirmOverwrite->GetValue();
	config->Write(wxT("settings/confirmoverwrite"), confirmoverwrite);

	bool confirmdelete = m_chkConfirmDelete->GetValue();
	config->Write(wxT("settings/confirmdelete"), confirmdelete);

	bool reloadsession = m_chkReloadSession->GetValue();
	config->Write(wxT("settings/reloadsession"), reloadsession);

	delete config;
	event.Skip();
}
