/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  This file contains the code for the main frame, which is almost all of the
 *  user-interface code.
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
 *  $Id: libmngr_frame.cpp 5907 2018-12-14 22:05:40Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_frame.h"
#include "libmngr_dlgoptions.h"
#include "libmngr_dlgnewfootprint.h"
#include "libmngr_dlgnewsymbol.h"
#include "libmngr_paths.h"
#include "libmngr_dlgreport.h"
#include "libmngr_dlgtemplate.h"
#include "pdfreport.h"
#include "remotelink.h"
#include "rpn.h"
#include "svnrev.h"
#if !defined NO_CURL
    #include "libmngr_dlgremotelink.h"
    #include "remotelink.h"
#endif
#if !defined NO_3DMODEL
    #include "vrmlsupport.h"
#endif
#include <wx/aboutdlg.h>
#include <wx/accel.h>
#include <wx/busyinfo.h>
#include <wx/choicdlg.h>
#include <wx/clipbrd.h>
#include <wx/cursor.h>
#include <wx/dir.h>
#include <wx/fileconf.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/mimetype.h>
#include <wx/regex.h>
#include <wx/stdpaths.h>
#include <wx/textdlg.h>
#include <wx/textfile.h>
#include <wx/tokenzr.h>
#include <wx/utils.h>
#include <math.h>

extern "C" {
  #include "unqlite.h"
}

#include "res/logo32.xpm"
#include "res/logo64.xpm"


#if !defined M_PI
    #define M_PI 3.14159265358979323846
#endif

#if !defined sizearray
    #define sizearray(a)    (sizeof(a) / sizeof((a)[0]))
#endif

#define LINE_OFFSET     5   /* in pixels (offset of the dimension line from the centre of the pad) */
#define MEASURE_GAP     2   /* factor of the font size (the font for the dimensions) */
#define SCALE_MIN       0.8
#define SCALE_DEFAULT   8
#define SCALE_MAX       80
#define SCALE_FACTOR    1.189207  /* pow(2, 0.25) */
#if defined _WIN32
    #define PANEL_WIDTH 200 /* default width of the sidebar, in pixels */
    #define EDITF_WIDTH 150
#else
    #define PANEL_WIDTH 250
    #define EDITF_WIDTH 175
#endif

#define IMG_WIDTH_FP    128
#define IMG_HEIGHT_FP   128
#define IMG_SCALE_FP    96
#define IMG_WIDTH_SYM   160
#define IMG_HEIGHT_SYM  160
#define IMG_SCALE_SYM   450

#define LEFTPANEL       -1
#define RIGHTPANEL      1
#define BOTHPANELS      0

const wxColour ENABLED(*wxWHITE);
const wxColour PROTECTED(224,224,224);
const wxColour CHANGED(255,192,192);


libmngrFrame::libmngrFrame(wxWindow* parent)
:
AppFrame(parent)
{
    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif

    SetIcon(wxIcon(logo32_xpm));

    /* I prefer the flat look, but that does not look right on GTK; so it is
       set only on Windows */
    #if defined _WIN32
        m_btnMove->SetWindowStyle(m_btnMove->GetWindowStyle() | wxBORDER_NONE);
        m_btnCopy->SetWindowStyle(m_btnCopy->GetWindowStyle() | wxBORDER_NONE);
        m_btnDelete->SetWindowStyle(m_btnDelete->GetWindowStyle() | wxBORDER_NONE);
        m_btnRename->SetWindowStyle(m_btnRename->GetWindowStyle() | wxBORDER_NONE);
        m_btnDuplicate->SetWindowStyle(m_btnDuplicate->GetWindowStyle() | wxBORDER_NONE);
        m_btnSavePart->SetWindowStyle(m_btnSavePart->GetWindowStyle() | wxBORDER_NONE);
        m_btnRevertPart->SetWindowStyle(m_btnRevertPart->GetWindowStyle() | wxBORDER_NONE);
    #endif

    wxAcceleratorEntry entries[9];
    entries[0].Set(wxACCEL_CTRL, '+', IDT_ZOOMIN);      // Ctrl-+
    entries[1].Set(wxACCEL_CTRL, '=', IDT_ZOOMIN);      // On US keyboards, Ctrl-= is needed instead of Ctrl-+
    entries[2].Set(wxACCEL_CTRL, WXK_NUMPAD_ADD, IDT_ZOOMIN);
    entries[3].Set(wxACCEL_CTRL, '-', IDT_ZOOMOUT);
    entries[4].Set(wxACCEL_CTRL, WXK_NUMPAD_SUBTRACT, IDT_ZOOMOUT);
    entries[5].Set(wxACCEL_CTRL, 'S', IDT_SAVE);
    entries[6].Set(wxACCEL_CTRL, 'Z', IDT_REVERT);
    entries[7].Set(wxACCEL_CTRL, 'V', IDM_PASTEGENERAL);
    entries[8].Set(wxACCEL_ALT, 'X', IDC_EXPORT);
    wxAcceleratorTable accel(sizearray(entries), entries);
    SetAcceleratorTable(accel);

    /* add bindings for commands that are not attached to a menu item or button */
    Connect(IDM_PASTEGENERAL, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnPasteGeneral));
    Connect(IDC_EXPORT, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnExportGeneral));

    /* restore application size */
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    long w = 0, h = 0;
    if (config->Read(wxT("settings/framewidth"), &w) && config->Read(wxT("settings/frameheight"), &h))
        SetSize(w, h);
    if (!config->Read(wxT("settings/scale"), &Scale) || Scale < SCALE_MIN || Scale > SCALE_MAX)
        Scale = SCALE_DEFAULT;

    config->Read(wxT("settings/symbolmode"), &SymbolMode, false);
    m_menubar->Check(IDM_SCHEMATICMODE, SymbolMode);
    config->Read(wxT("settings/comparemode"), &CompareMode, false);
    m_menubar->Check(IDM_COMPAREMODE, CompareMode);
    m_toolBar->EnableTool(IDT_LEFTFOOTPRINT, CompareMode);
    m_toolBar->EnableTool(IDT_RIGHTFOOTPRINT, CompareMode);
    if (CompareMode) {
        m_toolBar->ToggleTool(IDT_LEFTFOOTPRINT, true);
        m_toolBar->ToggleTool(IDT_RIGHTFOOTPRINT, true);
    }
    m_radioViewLeft->Enable(CompareMode);
    m_radioViewRight->Enable(CompareMode);
    m_radioViewLeft->SetValue(true);
    config->Read(wxT("settings/syncmode"), &SyncMode, false);
    m_menubar->Check(IDM_SYNCMODE, SyncMode);

    config->Read(wxT("display/footprintbkgnd"), &clrFootprintMode, wxColour(0, 0, 0));
    config->Read(wxT("display/schematicbkgnd"), &clrSchematicMode, wxColour(255, 255, 255));
    config->Read(wxT("display/3dmodelbkgnd"), &clr3DModelMode, wxColour(0, 64, 0));
    config->Read(wxT("display/showlabels"), &ShowLabels, true);
    config->Read(wxT("display/centrecross"), &DrawCentreCross, true);
    config->Read(wxT("display/fullpath"), &ShowFullPaths, false);
    config->Read(wxT("settings/copyvrml"), &CopyVRML, true);
    config->Read(wxT("settings/disabletemplate"), &DontRebuildTemplate, false);
    config->Read(wxT("settings/confirmoverwrite"), &ConfirmOverwrite, true);
    config->Read(wxT("settings/confirmdelete"), &ConfirmDelete, true);
    config->Read(wxT("settings/reloadsession"), &ReloadSession, true);
    ShowPinNumbers = true;
    ShowMeasurements = true;

    #if defined NO_3DMODEL
        m_toolBar->DeleteTool(IDT_3DVIEW);
    #else
        glCanvas = NULL;
        glContext = NULL;
    #endif
    ModelMode = false;

    bool measurements = true;
    config->Read(wxT("display/measurements"), &measurements, true);
    m_toolBar->ToggleTool(IDT_MEASUREMENTS, measurements);

    long pos;
    if (!config->Read(wxT("settings/splitterpanel"), &pos))
        pos = -1;
    if (pos <= 0)
        m_splitterViewPanel->Unsplit();
    m_menubar->Check(IDM_DETAILSPANEL, pos > 0);
    m_toolBar->ToggleTool(IDT_DETAILSPANEL, pos > 0);

    FontSizeLegend = config->Read(wxT("display/fontsize"), 8L);
    DimensionOffset = config->Read(wxT("display/dimoffset"), 50L);

    delete config;

    /* configure the status bar */
    int widths[] = { -1, EDITF_WIDTH };
    m_statusBar->SetFieldsCount(2, widths);
    m_statusBar->SetStatusText(wxT("(no filter)"), 1);
    m_editFilter = new wxSearchCtrl(m_statusBar, IDC_FILTER, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_editFilter->SetDescriptiveText(wxT("keywords"));
    m_editFilter->SetToolTip(wxT("Enter one or more keywords to filter the footprints/symbols on"));
    m_editFilter->ShowCancelButton(true);
    wxRect rect;
    m_statusBar->GetFieldRect(1, rect);
    m_editFilter->SetSize(rect.GetLeft(), rect.GetTop(), rect.GetWidth(), rect.GetHeight(), 0);
    m_editFilter->Hide();
    m_editFilter->Connect(wxEVT_COMMAND_TEXT_UPDATED, wxCommandEventHandler(libmngrFrame::OnFilterChange), NULL, this);
    m_editFilter->Connect(wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(libmngrFrame::OnFilterEnter), NULL, this);
    m_editFilter->Connect(wxEVT_SEARCHCTRL_SEARCH_BTN, wxCommandEventHandler(libmngrFrame::OnFilterEnter), NULL, this);
    m_editFilter->Connect(wxEVT_SEARCHCTRL_CANCEL_BTN, wxCommandEventHandler(libmngrFrame::OnFilterCancel), NULL, this);

    /* configure the list controls (columns) */
    m_listModulesLeft->InsertColumn(0, wxT("Part"), wxLIST_FORMAT_LEFT);
    m_listModulesLeft->InsertColumn(1, wxT("Library"), wxLIST_FORMAT_LEFT);
    m_listModulesLeft->InsertColumn(2, wxT("Path"), wxLIST_FORMAT_LEFT);
    m_listModulesRight->InsertColumn(0, wxT("Part"), wxLIST_FORMAT_LEFT);
    m_listModulesRight->InsertColumn(1, wxT("Library"), wxLIST_FORMAT_LEFT);
    m_listModulesRight->InsertColumn(2, wxT("Path"), wxLIST_FORMAT_LEFT);

    PinData[0] = PinData[1] = NULL;
    PinDataCount[0] = PinDataCount[1] = 0;
    SelectedPartLeft = SelectedPartRight = -1;
    PartEdited = FieldEdited = false;

    /* set starting state and labels for the buttons (no footprint/symbol selected yet) */
    ToggleMode(SymbolMode);

    #if defined NO_CURL
        /* disable the repository functions in the menu */
        m_menubar->Enable(IDM_DLGREMOTE, false);
    #endif

    /* set a single-shot timer to set the initial splitter positions (this action
       must be delayed, because of the internals of wxWidgets) */
    m_Timer = new wxTimer(this, IDM_TIMER);
    Connect(IDM_TIMER, wxEVT_TIMER, wxTimerEventHandler(libmngrFrame::OnTimer));
    m_Timer->Start(500);

    wxString path = theApp->GetFontFile();
    VFont.Read(path.mb_str(wxConvFile));
}

void libmngrFrame::OnTimer(wxTimerEvent& /*event*/)
{
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    int ok_count = 0, valid_count = 0;
    long pos;
    if (config->Read(wxT("settings/splitter"), &pos)) {
        valid_count++;
        if (pos == m_splitter->GetSashPosition())
            ok_count++;
        else
            m_splitter->SetSashPosition(pos);
    }
    if (config->Read(wxT("settings/splitterpanel"), &pos)) {
        valid_count++;
        wxSize size = m_splitterViewPanel->GetSize();
        if (pos <= 0 || pos == size.GetWidth() - m_splitterViewPanel->GetSashPosition())
            ok_count++;
        else
            m_splitterViewPanel->SetSashPosition(-pos);
    }
    delete config;

    static wxBusyInfo* busy = NULL;
    static int idle_count = 0;
    if (ok_count == 2) {
        /* all sashes already at the good position, wait a while and check whether to clear the timer */
        if (busy == NULL)
            busy = new wxBusyInfo("Initializing lay-out. Please wait...");
        if (++idle_count >= 3) {    /* position has stayed good for 1.5 seconds */
            m_Timer->Stop();
            delete busy;
            /* some code sequence to force the frame to come up to top */
            Iconize(true);  /* minimize first */
            Iconize(false); /* then restore */
            SetFocus();     /* give focus (probably redundant) */
            Raise();        /* bring window to front (probably doesn't do anything on modern versions of Windows */
            Show(true);     /* show the window (probably redundant) */
        }
    } else if (valid_count == 2) {
        /* force a resize, to lay out the subfields */
        wxSize sz = GetSize();
        SetSize(sz.GetWidth() + 1, sz.GetHeight());
        SetSize(sz.GetWidth(), sz.GetHeight());
        idle_count = 0;
    } else {
        /* no valid size/sash positions in the INI file -> just use the defaults */
        m_Timer->Stop();
        delete busy;
    }
}

void libmngrFrame::OnCloseApp(wxCloseEvent& event)
{
    CheckSavePart();
    for (int fp = 0; fp < 2; fp++)
        PartData[fp].Clear();

    #if !defined NO_CURL
        curlCleanup(true);
    #endif

    #if !defined NO_3DMODEL
        if (ModelMode)
            Scale = ScaleNormal;    /* restore previous scale value */
        sceneGraph.clear();
    #endif

    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());

    /* save the currently selected libraries and footprints/symbols */
    int idx = m_choiceModuleLeft->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleLeft->GetCount()) {
        wxString name = m_choiceModuleLeft->GetString(idx);
        if (name.length() > 0 && name[0] == wxT('('))
            name = wxEmptyString;
        config->Write(wxT("session/lib1"), name);
    }
    idx = m_choiceModuleRight->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleRight->GetCount()) {
        wxString name = m_choiceModuleRight->GetString(idx);
        if (name.length() > 0 && name[0] == wxT('('))
            name = wxEmptyString;
        config->Write(wxT("session/lib2"), name);
    }

    /* save window size and settings */
    wxSize size = GetSize();
    config->Write(wxT("settings/framewidth"), size.GetWidth());
    config->Write(wxT("settings/frameheight"), size.GetHeight());

    config->Write(wxT("settings/scale"), Scale);
    config->Write(wxT("settings/symbolmode"), SymbolMode);
    config->Write(wxT("settings/comparemode"), CompareMode);
    config->Write(wxT("settings/syncmode"), SyncMode);

    config->Write(wxT("settings/splitter"), m_splitter->GetSashPosition());
    if (m_splitterViewPanel->IsSplit() && m_toolBar->GetToolToggled(IDT_DETAILSPANEL)) {
        size = m_splitterViewPanel->GetSize();
        int pos = size.GetWidth() - m_splitterViewPanel->GetSashPosition();
        config->Write(wxT("settings/splitterpanel"), pos);
    } else {
        config->Write(wxT("settings/splitterpanel"), -1);
    }

    config->Write(wxT("display/measurements"), m_toolBar->GetToolToggled(IDT_MEASUREMENTS));

    delete config;
    event.Skip();
}

void libmngrFrame::OnNewLibrary(wxCommandEvent& /*event*/)
{
    wxString filter;
    if (SymbolMode)
        filter = wxT("Symbol libraries (*.lib)|*.lib");
    else
        filter = wxT("Legacy (*.mod)|*.mod|Legacy (mm) (*.mod)|*.mod|s-expression|*.pretty");
    wxFileDialog* dlg = new wxFileDialog(this, wxT("New library..."),
                                         wxEmptyString, wxEmptyString,
                                         filter, wxFD_SAVE);
    if (!SymbolMode)
        dlg->SetFilterIndex(1); /* by default, create libraries in legacy version 2 format */
    if (dlg->ShowModal() != wxID_OK)
        return;
    /* set default extension */
    wxFileName fname(dlg->GetPath());
    if (fname.GetExt().length() == 0) {
        if (SymbolMode)
            fname.SetExt(wxT("lib"));
        else if (dlg->GetFilterIndex() <= 1)
            fname.SetExt(wxT("mod"));
        else
            fname.SetExt(wxT("pretty"));
    }

    if (dlg->GetFilterIndex() <= 1) {
        wxTextFile file;
        if (!file.Create(fname.GetFullPath())) {
            if (!file.Open(fname.GetFullPath())) {
                wxMessageBox(wxT("Failed to create library ") + fname.GetFullPath());
                return;
            }
        }
        file.Clear();
        if (SymbolMode) {
            file.AddLine(wxT("EESchema-LIBRARY Version 2.3  Date: ") + wxNow());
            file.AddLine(wxT("#encoding utf-8"));
            file.AddLine(wxT("#"));
            file.AddLine(wxT("#End Library"));
        } else {
            file.AddLine(wxT("PCBNEW-LibModule-V1 ") + wxNow());
            file.AddLine(wxT("# encoding utf-8"));
            if (dlg->GetFilterIndex() == 1)
                file.AddLine(wxT("Units mm"));
            file.AddLine(wxT("$INDEX"));
            file.AddLine(wxT("$EndINDEX"));
            file.AddLine(wxT("$EndLIBRARY"));
        }
        file.Write();
        file.Close();
    } else {
        wxASSERT(!SymbolMode);
        if (!wxMkdir(fname.GetFullPath())) {
            wxMessageBox(wxT("Failed to create library ") + fname.GetFullPath());
            return;
        }
    }

    /* get current selections in the left and right combo-boxes */
    wxString leftlib;
    int idx = m_choiceModuleLeft->GetCurrentSelection();
    if (idx < 0 || idx >= (int)m_choiceModuleLeft->GetCount())
        leftlib = LIB_NONE;     /* assume "none" is selected when there is no selection */
    else
        leftlib = m_choiceModuleLeft->GetString(idx);
    wxASSERT(leftlib.length() > 0);
    wxString rightlib;
    idx = m_choiceModuleRight->GetCurrentSelection();
    if (idx < 0 || idx >= (int)m_choiceModuleRight->GetCount())
        rightlib = LIB_NONE;    /* assume "none" is selected when there is no selection */
    else
        rightlib = m_choiceModuleRight->GetString(idx);
    wxASSERT(rightlib.length() > 0);

    /* refresh comboboxes */
    CollectAllLibraries(false);
    EnableButtons(0);

    /* first try to restore the previous selections */
    idx = m_choiceModuleLeft->FindString(leftlib);
    if (idx >= 0)
        m_choiceModuleLeft->SetSelection(idx);
    idx = m_choiceModuleRight->FindString(rightlib);
    if (idx >= 0)
        m_choiceModuleRight->SetSelection(idx);

    /* check whether to auto-select the new library
     * if one of the comboboxes is set to "(None)", open it there
     * if one of the comboboxes is set to "(All)" or "(Repository)", open it in the other
     */
    int refresh = 0;
    /* first try left box */
    if (leftlib.CmpNoCase(LIB_NONE) == 0) {
        idx = m_choiceModuleLeft->FindString(fname.GetFullPath());
        if (idx >= 0) {
            m_choiceModuleLeft->SetSelection(idx);
            refresh = LEFTPANEL;
        }
    }
    /* then try the right box */
    if (refresh == 0) {
        if (rightlib.CmpNoCase(LIB_NONE) == 0) {
            idx = m_choiceModuleRight->FindString(fname.GetFullPath());
            if (idx >= 0) {
                m_choiceModuleRight->SetSelection(idx);
                refresh = RIGHTPANEL;
            }
        }
    }
    /* neither has "(None)" selected, see if either has "(All)" or "(Repository)"
       selected, left box first */
    if (refresh == 0) {
        if (leftlib.CmpNoCase(LIB_ALL) == 0 || leftlib.CmpNoCase(LIB_REPOS) == 0) {
            idx = m_choiceModuleRight->FindString(fname.GetFullPath());
            if (idx >= 0) {
                m_choiceModuleRight->SetSelection(idx);
                refresh = RIGHTPANEL;
            }
        }
    }
    if (refresh == 0) {
        if (rightlib.CmpNoCase(LIB_ALL) == 0 || rightlib.CmpNoCase(LIB_REPOS) == 0) {
            idx = m_choiceModuleLeft->FindString(fname.GetFullPath());
            if (idx >= 0) {
                m_choiceModuleLeft->SetSelection(idx);
                refresh = LEFTPANEL;
            }
        }
    }
    if (refresh == 0) {
        /* both sides have a library selected, then just vacate the right box */
        idx = m_choiceModuleRight->FindString(fname.GetFullPath());
        if (idx >= 0) {
            m_choiceModuleRight->SetSelection(idx);
            refresh = RIGHTPANEL;
        }
    }
    if (refresh == LEFTPANEL)
        HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
    else if (refresh == RIGHTPANEL)
        HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
    SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
    m_panelView->Refresh();
}

bool libmngrFrame::GetSelectedLibrary(wxString* library, int *side)
{
    /* first check which library to add the footprint/symbol to (or which to rename) */
    wxString leftname, rightname;
    int idx = m_choiceModuleLeft->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleLeft->GetCount()) {
        leftname = m_choiceModuleLeft->GetString(idx);
        if (leftname.length() > 0 && leftname[0] == wxT('('))
            leftname = wxEmptyString;
    }
    idx = m_choiceModuleRight->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleRight->GetCount()) {
        rightname = m_choiceModuleRight->GetString(idx);
        if (rightname.length() > 0 && rightname[0] == wxT('('))
            rightname = wxEmptyString;
    }
    wxString filename;
    if (leftname.length() > 0 && rightname.length() > 0 && leftname.CmpNoCase(rightname) != 0) {
        wxArrayString choices;
        choices.Add(leftname);
        choices.Add(rightname);
        wxSingleChoiceDialog dlg(this, wxT("Select library"), wxT("Library"), choices);
        if (dlg.ShowModal() != wxID_OK)
            return false;
        filename = dlg.GetStringSelection();
    } else if (leftname.length() > 0) {
        filename = leftname;
    } else if (rightname.length() > 0) {
        filename = rightname;
    } else {
        wxASSERT(leftname.length() == 0 && rightname.length() == 0);
        wxMessageBox(wxT("No local library is selected."));
        return false;
    }

    wxASSERT(side != NULL);
    *side = 0;
    if (filename.Cmp(leftname) == 0)
        *side -= 1;
    if (filename.Cmp(rightname) == 0)
        *side += 1;
    /* if at the end of this procedure, side == 0, it is equal to both left and right */

    wxASSERT(library != NULL);
    *library = filename;
    return true;
}

void libmngrFrame::OnRenameLibrary(wxCommandEvent& /*event*/)
{
    /* first check which library to rename */
    int side;
    wxString filename;
    if (!GetSelectedLibrary(&filename, &side))
        return;

    /* split the path off */
    wxASSERT(filename.length() > 0);
    wxFileName old_fname(filename);

    wxTextEntryDialog dlg(this, wxT("New name"), wxT("Rename ") + old_fname.GetName(), old_fname.GetName());
    if (dlg.ShowModal() == wxID_OK) {
        filename = dlg.GetValue();
        wxFileName new_fname(filename);
        new_fname.SetPath(old_fname.GetPath()); /* remove any path the user typed in, set the path of the old library */
        new_fname.SetExt(old_fname.GetExt());
        bool ok = wxRenameFile(old_fname.GetFullPath(), new_fname.GetFullPath(), false);
        if (ok) {
            /* refresh comboboxes */
            CollectAllLibraries(false);
            EnableButtons(0);
            /* load the renamed library */
            if (side <= 0) {
                int idx = m_choiceModuleLeft->FindString(new_fname.GetFullPath());
                if (idx >= 0)
                    m_choiceModuleLeft->SetSelection(idx);
                HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
            }
            if (side >= 0) {
                int idx = m_choiceModuleRight->FindString(new_fname.GetFullPath());
                if (idx >= 0)
                    m_choiceModuleRight->SetSelection(idx);
                HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
            }
            SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
            m_panelView->Refresh();
        } else {
            wxMessageBox(wxT("Failed to rename the library (access denied or conflicting names)"));
        }
    }
}

void libmngrFrame::OnNewFootprint(wxCommandEvent& /*event*/)
{
    if (CompareMode) {
        wxMessageBox(wxT("Compare mode must be switched off."));
        return;
    }
    if (SymbolMode) {
        wxMessageBox(wxT("Footprint mode must be selected (instead of Schematic mode)."));
        return;
    }
    CheckSavePart();

    /* first check which library to add the footprint to */
    int side;
    wxString filename;
    if (!GetSelectedLibrary(&filename, &side))
        return;
    if (filename.Right(10).CmpNoCase(wxT(".kicad_mod")) == 0 || filename.Right(4).Cmp(wxT(".emp")) == 0) {
        wxMessageBox(wxT("The selected destination file is an export file instead of a library.\nFootprints cannot be added to export files."));
        return;
    }
    m_radioViewLeft->SetValue(side <= 0);
    m_radioViewRight->SetValue(side > 0);

    /* set the dialog for the template */
    libmngrDlgNewFootprint dlg(this);
    dlg.SetLibraryName(filename);
    if (dlg.ShowModal() == wxID_OK) {
        /* load the template */
        wxArrayString templat;
        LoadTemplate(dlg.GetTemplateName(), &templat, false);
        /* extract the number of pins from the footprint name (this may fail
           and the template may overrule it; it is just a heuristic) */
        long pins = 0;
        wxString name = dlg.GetFootprintName();
        size_t len = name.Length();
        while (len > 0 && isdigit(name[len - 1]))
            len--;
        if (len > 0) {
            name = name.Mid(len);
            name.ToLong(&pins);
            if (pins > 100 || !ValidPinCount(pins, dlg.GetTemplateName(), false))   /* must match the #pins line in the header */
                pins = 0;
        }
        /* verify that the name is valid */
        wxString newname = dlg.GetFootprintName();
        int targettype = LibraryType(filename);
        if (targettype == VER_S_EXPR && !ValidateFilename(&newname))
            return;
        /* run over the expressions to create the initial shape */
        RPNexpression rpn;
        wxString description = GetTemplateHeaderField(dlg.GetTemplateName(), wxT("brief"), SymbolMode);
        if (pins > 0)
            rpn.SetVariable(RPNvariable("PT", pins));
        SetVarDefaults(&rpn, dlg.GetTemplateName(), newname, description);
        SetVarsFromTemplate(&rpn, dlg.GetTemplateName(), false);
        /* create a footprint from the template */
        wxArrayString module;
        if (!FootprintFromTemplate(&module, templat, rpn, false))
            return; /* error message(s) already given */
        if (targettype == VER_INVALID) {
            wxMessageBox(wxT("Unsupported file format for the target library."));
            return;
        } if (targettype == VER_S_EXPR) {
            /* translate the module to s-expression */
            wxArrayString tmpmod = module;
            TranslateToSexpr(&module, tmpmod);
        }
        /* save the footprint with initial settings (may need to erase the footprint
             first) */
        if (ExistFootprint(filename, newname))
            RemoveFootprint(filename, newname);
        if (!InsertFootprint(filename, newname, module, true)) {
            wxMessageBox(wxT("Operation failed."));
            return;
        }
        wxString vrmlpath = GetVRMLPath(filename, module);
        if (vrmlpath.Length() > 0) {
            wxString modelname = GetTemplateHeaderField(dlg.GetTemplateName(), wxT("model"), SymbolMode);
            if (modelname.length() == 0)
                modelname = dlg.GetTemplateName();
            else
                modelname = modelname.BeforeFirst(wxT(' '));    /* use first model (there may be more) */
            VRMLFromTemplate(vrmlpath, modelname, rpn);
        }
        /* refresh the library (or possibly both, if both lists contain the same library) */
        if (side <= 0)
            HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
        if (side >= 0)
            HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
        SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
        /* load the newly created part */
        RemoveSelection(m_listModulesLeft, &SelectedPartLeft);
        RemoveSelection(m_listModulesRight, &SelectedPartRight);
        OffsetX = OffsetY = 0;
        wxListCtrl* list = (side <= 0) ? m_listModulesLeft : m_listModulesRight;
        wxBusyCursor cursor;
        /* get the index of the new part in the footprint list */
        int idx;
        for (idx = 0; idx < list->GetItemCount(); idx++) {
            wxString symbol = list->GetItemText(idx);
            if (symbol.Cmp(dlg.GetFootprintName()) == 0)
                break;
        }
        wxASSERT(idx < list->GetItemCount());   /* it was just inserted, so it must be found */
        list->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        list->EnsureVisible(idx);
        LoadPart(idx, list, 0, 0);
        UpdateDetails(0);
        Update3DModel(PartData[0]);
        AutoZoom(0);
        m_panelView->Refresh();
        m_statusBar->SetStatusText(wxT("New footprint"));
        /* toggle the details panel on, if it was off */
        bool details = m_menubar->IsChecked(IDM_DETAILSPANEL);
        if (!details) {
            m_menubar->Check(IDM_DETAILSPANEL, true);
            m_toolBar->ToggleTool(IDT_DETAILSPANEL, true);
            ToggleDetailsPanel(true);
        }
    }
}

void libmngrFrame::OnNewSymbol(wxCommandEvent& /*event*/)
{
    if (CompareMode) {
        wxMessageBox(wxT("Compare mode must be switched off."));
        return;
    }
    if (!SymbolMode) {
        wxMessageBox(wxT("Schematic mode must be selected (instead of Footprint mode)."));
        return;
    }
    CheckSavePart();

    /* first check which library to add the footprint to */
    wxString filename;
    int side;
    if (!GetSelectedLibrary(&filename, &side))
        return;
    m_radioViewLeft->SetValue(side <= 0);
    m_radioViewRight->SetValue(side > 0);

    /* set the dialog for the template */
    libmngrDlgNewSymbol dlg(this);
    dlg.SetLibraryName(filename);
    if (dlg.ShowModal() == wxID_OK) {
        /* load the template */
        wxArrayString templat;
        LoadTemplate(dlg.GetTemplateName(), &templat, true);
        GetTemplateSections(dlg.GetTemplateName(), CustomPinSections[0], sizearray(CustomPinSections[0]));
        /* run over the expressions to create the initial shape */
        RPNexpression rpn;
        wxString description = GetTemplateHeaderField(dlg.GetTemplateName(), wxT("brief"), SymbolMode);
        SetVarDefaults(&rpn, dlg.GetTemplateName(), dlg.GetSymbolName(), description, dlg.GetSymbolRef());
        /* create default pin info structure from the pin count */
        int pins = 0;
        rpn.Set("PT");
        if (rpn.Parse() == RPN_OK) {
            RPNvalue v = rpn.Value();
            pins = (int)(v.Double() + 0.001);
        }
        if (pins == 0)
            pins = 2;   /* this is actually a template error; PT should be set */
        PinInfo* info = new PinInfo[pins];
        if (info == NULL)
            return;
        int totals[PinInfo::SectionCount];
        int counts[PinInfo::SectionCount];
        for (int s = 0; s < PinInfo::SectionCount; s++) {
            totals[s] = 0;
            counts[s] = 0;
            char str[20];
            sprintf(str, "PT:%d", s);
            rpn.Set(str);
            if (rpn.Parse() == RPN_OK) {
                RPNvalue v = rpn.Value();
                totals[s] = (int)(v.Double() + 0.001);
            }
        }
        int cursec = PinInfo::Left;
        for (int i = 0; i < pins; i++) {
            while (cursec < PinInfo::SectionCount && counts[cursec] >= totals[cursec])
                cursec++;
            if (cursec >= PinInfo::SectionCount)
                cursec = PinInfo::Left; /* on error, map remaining pins to left side */
            counts[cursec] += 1;
            info[i].seq = i;
            info[i].number = wxString::Format(wxT("%d"), i + 1);
            info[i].name = wxT("~");
            info[i].type = PinInfo::Passive;
            info[i].shape = PinInfo::Line;
            info[i].section = cursec;
            info[i].part = 0;
        }
        /* create a footprint from the template */
        wxArrayString symbol;
        bool result = SymbolFromTemplate(&symbol, templat, rpn, info, pins);
        delete[] info;
        if (!result)
            return; /* error message(s) already given */
        /* save the symbol with initial settings (may need to erase the symbol first) */
        if (ExistSymbol(filename, dlg.GetSymbolName()))
            RemoveSymbol(filename, dlg.GetSymbolName());
        if (!InsertSymbol(filename, dlg.GetSymbolName(), symbol)) {
            wxMessageBox(wxT("Operation failed."));
            return;
        }
        /* refresh the library (or possibly both, if both lists contain the same library) */
        if (side <= 0)
            HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
        if (side >= 0)
            HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
        SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
        /* load the newly created part */
        RemoveSelection(m_listModulesLeft, &SelectedPartLeft);
        RemoveSelection(m_listModulesRight, &SelectedPartRight);
        OffsetX = OffsetY = 0;
        wxListCtrl* list = (side <= 0) ? m_listModulesLeft : m_listModulesRight;
        wxBusyCursor cursor;
        /* get the index of the new part in the footprint list */
        int idx;
        for (idx = 0; idx < list->GetItemCount(); idx++) {
            wxString symbol = list->GetItemText(idx);
            if (symbol.Cmp(dlg.GetSymbolName()) == 0)
                break;
        }
        wxASSERT(idx < list->GetItemCount());   /* it was just inserted, so it must be found */
        list->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        list->EnsureVisible(idx);
        LoadPart(idx, list, 0, 0);
        UpdateDetails(0);
        m_panelView->Refresh();
        m_statusBar->SetStatusText(wxT("New symbol"));
        /* toggle the details panel on, if it was off */
        bool details = m_menubar->IsChecked(IDM_DETAILSPANEL);
        if (!details) {
            m_menubar->Check(IDM_DETAILSPANEL, true);
            m_toolBar->ToggleTool(IDT_DETAILSPANEL, true);
            ToggleDetailsPanel(true);
        }
    }
}

bool libmngrFrame::GetReportNameList(wxString* reportfile, wxString* library, wxArrayString* namelist)
{
    /* find which library to print */
    wxString leftname, rightname;
    int idx = m_choiceModuleLeft->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleLeft->GetCount()) {
        leftname = m_choiceModuleLeft->GetString(idx);
        if (leftname.length() > 0 && leftname[0] == wxT('('))
            leftname = wxEmptyString;
    }
    idx = m_choiceModuleRight->GetCurrentSelection();
    if (idx >= 0 && idx < (int)m_choiceModuleRight->GetCount()) {
        rightname = m_choiceModuleRight->GetString(idx);
        if (rightname.length() > 0 && rightname[0] == wxT('('))
            rightname = wxEmptyString;
    }
    wxASSERT(library != NULL);
    if (leftname.length() > 0 && rightname.length() > 0) {
        wxArrayString choices;
        choices.Add(leftname);
        choices.Add(rightname);
        wxSingleChoiceDialog dlg(this, wxT("Select library"), wxT("Library"), choices);
        if (dlg.ShowModal() != wxID_OK)
            return false;
        *library = dlg.GetStringSelection();
    } else if (leftname.length() > 0) {
        *library = leftname;
    } else if (rightname.length() > 0) {
        *library = rightname;
    } else {
        wxASSERT(leftname.length() == 0 && rightname.length() == 0);
        wxMessageBox(wxT("No local library is selected."));
        return false;
    }

    /* get the name of the report file */
    wxFileName fname(*library);
    fname.SetExt(wxT("pdf"));
    wxFileDialog* dlg = new wxFileDialog(this, wxT("Choose report filename..."),
                                         wxEmptyString, fname.GetFullPath(),
                                         wxT("Acrobat PDF (*.pdf)|*.pdf"),
                                         wxFD_SAVE);
    if (dlg->ShowModal() != wxID_OK)
        return false;
    /* set default extension */
    fname.Assign(dlg->GetPath());
    if (fname.GetExt().length() == 0)
        fname.SetExt(wxT("pdf"));
    wxASSERT(reportfile != NULL);
    *reportfile = fname.GetFullPath();

    /* collect the list of footprint names to print */
    wxListCtrl* list;
    if (library->Cmp(leftname) == 0)
        list = m_listModulesLeft;
    else
        list = m_listModulesRight;
    wxASSERT(namelist != NULL);
    namelist->Clear();
    for (long idx = 0; idx < list->GetItemCount(); idx++)
        namelist->Add(list->GetItemText(idx));

    return true;
}

void libmngrFrame::OnFootprintReport(wxCommandEvent& /*event*/)
{
    wxString reportfile;
    wxString library;
    wxArrayString modules;
    if (!GetReportNameList(&reportfile, &library, &modules))
        return;

    /* print the library, then show the report */
    wxString format;
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    format = config->Read(wxT("report/paper"), wxT("Letter"));
    long landscape = config->Read(wxT("report/layout"), 0L);
    long opt_labels = config->Read(wxT("report/drawlabels"), 0L);
    long opt_description = config->Read(wxT("report/includedescription"), 1L);
    long opt_padinfo = config->Read(wxT("report/includepadinfo"), 1L);
    long opt_index = config->Read(wxT("report/index"), 1L);
    long fontsize = config->Read(wxT("report/fontsize"), 8L);
    delete config;
    PdfReport report;
    report.SetPage(format, (landscape != 0));
    report.FootprintOptions(opt_description != 0, opt_index != 0, opt_padinfo != 0, opt_labels != 0);
    report.SetFont(fontsize);
    report.FootprintReport(this, library, modules, reportfile);
    m_statusBar->SetStatusText(wxT("Finished report"));

    #if wxMAJOR_VERSION < 3
        wxTheMimeTypesManager->ReadMailcap(wxT("/etc/mailcap"));    /* for wxWidgets 2.8 */
    #endif
    wxFileType *FileType = wxTheMimeTypesManager->GetFileTypeFromMimeType(wxT("application/pdf"));
    wxString Command = FileType->GetOpenCommand(reportfile);
    /* sometimes wildcards remain in the command */
    int pos;
    while ((pos = Command.Find(wxT('*'))) >= 0)
        Command.Remove(pos, 1);
    Command.Trim();
    /* finally, run it */
    wxExecute(Command);
}

void libmngrFrame::OnSymbolReport(wxCommandEvent& /*event*/)
{
    wxString reportfile;
    wxString library;
    wxArrayString symbols;
    if (!GetReportNameList(&reportfile, &library, &symbols))
        return;

    /* print the library, then show the report */
    wxString format;
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    format = config->Read(wxT("report/paper"), wxT("Letter"));
    long landscape = config->Read(wxT("report/layout"), 0L);
    long fontsize = config->Read(wxT("report/fontsize"), 8L);
    long opt_description = config->Read(wxT("report/includedescription"), 1L);
    long opt_index = config->Read(wxT("report/index"), 1L);
    long opt_fplist = config->Read(wxT("report/fplist"), 1L);
    delete config;
    PdfReport report;
    report.SetPage(format, (landscape != 0));
    report.SymbolOptions(opt_description != 0, opt_index != 0, opt_fplist != 0);
    report.SetFont(fontsize);
    report.SymbolReport(this, library, symbols, reportfile);
    m_statusBar->SetStatusText(wxT("Finished report"));

    #if wxMAJOR_VERSION < 3
        wxTheMimeTypesManager->ReadMailcap(wxT("/etc/mailcap"));
    #endif
    wxFileType *FileType = wxTheMimeTypesManager->GetFileTypeFromMimeType(wxT("application/pdf"));
    wxString Command = FileType->GetOpenCommand(reportfile);
    /* sometimes wildcards remain in the command */
    int pos;
    while ((pos = Command.Find(wxT('*'))) >= 0)
        Command.Remove(pos, 1);
    Command.Trim();
    /* finally, run it */
    wxExecute(Command);
}

void libmngrFrame::OnQuit(wxCommandEvent& /*event*/)
{
    Close(true);
}

void libmngrFrame::ToggleMode(bool symbolmode)
{
    SymbolMode = symbolmode;
    m_toolBar->ToggleTool(IDT_3DVIEW, false);

    /* hide or show the fields in the side panel */
    wxBoxSizer* bsizer;
    wxFlexGridSizer* fgSidePanel = dynamic_cast<wxFlexGridSizer*>(m_panelSettings->GetSizer());
    wxASSERT(fgSidePanel != 0);
    fgSidePanel->Show(m_lblFootprintFilter, SymbolMode);
    fgSidePanel->Show(m_txtFootprintFilter, SymbolMode);
    fgSidePanel->Show(m_lblPinNames, SymbolMode);
    fgSidePanel->Show(m_gridPinNames, SymbolMode);

    bsizer = dynamic_cast<wxBoxSizer*>(m_lblUnitSelect->GetContainingSizer());
    wxASSERT(bsizer != 0);
    bsizer->Show(m_lblUnitSelect, SymbolMode);
    bsizer->Show(m_spinUnitSelect, SymbolMode);

    fgSidePanel->Show(m_lblPadShape, !SymbolMode);
    fgSidePanel->Show(m_choicePadShape, !SymbolMode);
    fgSidePanel->Show(m_lblPadSize, !SymbolMode);
    bsizer = dynamic_cast<wxBoxSizer*>(m_txtPadWidth->GetContainingSizer());
    wxASSERT(bsizer != 0);
    fgSidePanel->Show(bsizer, !SymbolMode, true);
    fgSidePanel->Show(m_lblPitch, !SymbolMode);
    fgSidePanel->Show(m_txtPitch, !SymbolMode);
    fgSidePanel->Show(m_lblPadSpan, !SymbolMode);
    bsizer = dynamic_cast<wxBoxSizer*>(m_txtPadSpanX->GetContainingSizer());
    wxASSERT(bsizer != 0);
    fgSidePanel->Show(bsizer, !SymbolMode, true);
    fgSidePanel->Show(m_lblDrillSize, !SymbolMode);
    fgSidePanel->Show(m_txtDrillSize, !SymbolMode);
    fgSidePanel->Show(m_lblAuxPad, !SymbolMode);
    bsizer = dynamic_cast<wxBoxSizer*>(m_txtAuxPadWidth->GetContainingSizer());
    wxASSERT(bsizer != 0);
    fgSidePanel->Show(bsizer, !SymbolMode, true);
    fgSidePanel->Show(m_lblShape, !SymbolMode);
    bsizer = dynamic_cast<wxBoxSizer*>(m_choiceShape->GetContainingSizer());
    wxASSERT(bsizer != 0);
    fgSidePanel->Show(bsizer, !SymbolMode, true);

    fgSidePanel->Layout();
    m_panelSettings->Refresh();

    /* the "keywords" field for footprints is used for aliases in symbol mode */
    if (SymbolMode)
        m_lblAlias->SetLabel(wxT("Alias"));
    else
        m_lblAlias->SetLabel(wxT("Keywords"));

    /* "pins" is for symbols what "pads" is for footprints */
    if (SymbolMode)
        m_lblPadCount->SetLabel(wxT("Pin count"));
    else
        m_lblPadCount->SetLabel(wxT("Pad count"));

    /* disable options not relevant for schematic mode, or re-enable them */
    m_menubar->Enable(IDM_NEWFOOTPRINT, !SymbolMode);
    m_menubar->Enable(IDM_NEWSYMBOL, SymbolMode);
    m_menubar->Enable(IDM_REPORTFOOTPRINT, !SymbolMode);
    m_menubar->Enable(IDM_REPORTSYMBOL, SymbolMode);
    m_toolBar->EnableTool(IDT_MEASUREMENTS, !SymbolMode);
    m_toolBar->Refresh();

    /* use a different background colour for both modes */
    if (SymbolMode)
        m_panelView->SetBackgroundColour(clrSchematicMode);
    else
        m_panelView->SetBackgroundColour(clrFootprintMode);

    /* restore starting state, and refresh */
    OffsetX = OffsetY = 0;
    EnableButtons(0);
    CollectAllLibraries();
    if (ReloadSession) {
        wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
        wxString name = config->Read(wxT("session/lib1"), wxEmptyString);
        int idx = m_choiceModuleLeft->FindString(name);
        if (idx >= 0)
            m_choiceModuleLeft->SetSelection(idx);
        name = config->Read(wxT("session/lib2"), wxEmptyString);
        idx = m_choiceModuleRight->FindString(name);
        if (idx >= 0)
            m_choiceModuleRight->SetSelection(idx);
        delete config;
        HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
        HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
        Raise();
        ReloadSession = false;
    }
    UpdateDetails(0);
    m_panelView->Refresh();
}

void libmngrFrame::OnFootprintMode(wxCommandEvent& event)
{
    ToggleMode(false);
    event.Skip();
}

void libmngrFrame::OnSymbolMode(wxCommandEvent& event)
{
    ToggleMode(true);
    event.Skip();
}

void libmngrFrame::OnSearchPaths(wxCommandEvent& /*event*/)
{
    libmngrDlgPaths dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        m_statusBar->SetStatusText(wxT("Settings changed"));
        CollectAllLibraries();
    }
}

void libmngrFrame::OnRemoteLink(wxCommandEvent& /*event*/)
{
    #if !defined NO_CURL
        libmngrDlgRemoteLink dlg(this);
        if (dlg.ShowModal() == wxID_OK) {
            m_statusBar->SetStatusText(wxT("Settings changed"));
            CollectAllLibraries();
        }
    #endif
}

void libmngrFrame::OnReportSettings(wxCommandEvent& /*event*/)
{
    libmngrDlgReport dlg(this);
    if (dlg.ShowModal() == wxID_OK)
        m_statusBar->SetStatusText(wxT("Settings changed"));
}

void libmngrFrame::OnUIOptions(wxCommandEvent& /*event*/)
{
    libmngrDlgOptions dlg(this);
    if (dlg.ShowModal() == wxID_OK) {
        m_statusBar->SetStatusText(wxT("Settings changed"));
        /* refresh, with the new settings */
        wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
        FontSizeLegend = config->Read(wxT("display/fontsize"), 8L);
        DimensionOffset = config->Read(wxT("display/dimoffset"), 50L);
        config->Read(wxT("display/footprintbkgnd"), &clrFootprintMode, wxColour(0, 0, 0));
        config->Read(wxT("display/schematicbkgnd"), &clrSchematicMode, wxColour(255, 255, 255));
        config->Read(wxT("display/3dmodelbkgnd"), &clr3DModelMode, wxColour(0, 64, 0));
        config->Read(wxT("display/showlabels"), &ShowLabels, true);
        config->Read(wxT("display/centrecross"), &DrawCentreCross, true);
        config->Read(wxT("display/fullpath"), &ShowFullPaths, false);
        config->Read(wxT("settings/copyvrml"), &CopyVRML, true);
        config->Read(wxT("settings/disabletemplate"), &DontRebuildTemplate, false);
        config->Read(wxT("settings/confirmoverwrite"), &ConfirmOverwrite, true);
        config->Read(wxT("settings/confirmdelete"), &ConfirmDelete, true);
        config->Read(wxT("settings/reloadsession"), &ReloadSession, true);
        delete config;

        if (SymbolMode)
            m_panelView->SetBackgroundColour(clrSchematicMode);
        else
            m_panelView->SetBackgroundColour(clrFootprintMode);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnTemplateOptions(wxCommandEvent& /*event*/)
{
    libmngrDlgTemplateOpts dlg(this);
    if (dlg.ShowModal() == wxID_OK)
        m_statusBar->SetStatusText(wxT("Settings changed"));
}

void libmngrFrame::OnCompareMode(wxCommandEvent& /*event*/)
{
    CheckSavePart();
    wxBusyCursor cursor;
    CompareMode = m_menubar->IsChecked(IDM_COMPAREMODE);
    m_toolBar->EnableTool(IDT_LEFTFOOTPRINT, CompareMode);
    m_toolBar->EnableTool(IDT_RIGHTFOOTPRINT, CompareMode);
    m_toolBar->ToggleTool(IDT_LEFTFOOTPRINT, CompareMode);
    m_toolBar->ToggleTool(IDT_RIGHTFOOTPRINT, CompareMode);
    m_toolBar->Refresh();
    m_radioViewLeft->Enable(CompareMode);
    m_radioViewRight->Enable(CompareMode);
    m_radioViewLeft->SetValue(true);

    /* disable 3D model mode (if set) */
    if (ModelMode) {
        ModelMode = false;
        m_toolBar->ToggleTool(IDT_3DVIEW, ModelMode);
        #if !defined NO_3DMODEL
            sceneGraph.clear();
        #endif
    }

    if (CompareMode) {
        EnableButtons(0);
        /* reload the footprints, for both listboxes */
        long idx = m_listModulesLeft->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (idx >= 0)
            LoadPart(idx, m_listModulesLeft, m_choiceModuleLeft, 0);
        idx = m_listModulesRight->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (idx >= 0)
            LoadPart(idx, m_listModulesRight, m_choiceModuleRight, 1);
    } else {
        /* remove the selection in the right listctrl (we only keep the left footprint) */
        RemoveSelection(m_listModulesRight, &SelectedPartRight);
        PartData[1].Clear();
        /* check whether there is a selection in the left listctrl, only enable
           the buttons if so */
        long idx = m_listModulesLeft->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (idx >= 0)
            EnableButtons(LEFTPANEL);
        m_radioViewLeft->SetValue(true);
        m_radioViewRight->SetValue(false);
    }

    OffsetX = OffsetY = 0;
    UpdateDetails(0);   /* update details for the left component */
    m_panelView->Refresh();
}

void libmngrFrame::OnDetailsPanel(wxCommandEvent& /*event*/)
{
    bool details = m_menubar->IsChecked(IDM_DETAILSPANEL);
    m_toolBar->ToggleTool(IDT_DETAILSPANEL, details);
    ToggleDetailsPanel(details);
}

void libmngrFrame::OnSyncMode(wxCommandEvent& /*event*/)
{
    CheckSavePart();
    wxBusyCursor cursor;
    SyncMode = m_menubar->IsChecked(IDM_SYNCMODE);
    /* keep selections in both footprints */
    long selLeft = m_listModulesLeft->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    long selRight = m_listModulesRight->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    /* reload the module lists, for both listboxes */
    HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
    HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
    SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
    /* restore selections */
    if (SyncMode) {
        long sel = (selLeft < 0) ? selRight : selLeft;
        if (sel >= 0) {
            m_listModulesLeft->SetItemState(selLeft, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_listModulesRight->SetItemState(selRight, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_listModulesLeft->EnsureVisible(selLeft);
            m_listModulesRight->EnsureVisible(selRight);
        }
    } else {
        if (selLeft >= 0) {
            m_listModulesLeft->SetItemState(selLeft, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_listModulesLeft->EnsureVisible(selLeft);
        }
        if (selRight >= 0) {
            m_listModulesRight->SetItemState(selRight, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_listModulesRight->EnsureVisible(selRight);
        }
    }
    m_panelView->Refresh();
}

void libmngrFrame::OnFilterToggle(wxCommandEvent& /*event*/)
{
    if (m_editFilter->IsShown() && m_editFilter->HasFocus()) {
        wxString filter = m_editFilter->GetValue();
        m_editFilter->SetValue(wxEmptyString);  /* clear the filter */
        m_editFilter->Hide();
        if (filter.Length() > 0 || FilterChanged) {
            HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
            HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
            SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
            m_panelView->Refresh();
        }
    } else if (m_editFilter->IsShown()) {
        m_editFilter->SetSelection(-1, -1);
        m_editFilter->SetFocus();
    } else {
        m_editFilter->SetValue(wxEmptyString);  /* should already be empty */
        m_editFilter->SetBackgroundColour(ENABLED);
        m_editFilter->Show();
        m_editFilter->SetFocus();
    }
    m_menubar->Check(IDM_FILTER, m_editFilter->IsShown());
    FilterChanged = false;
}

void libmngrFrame::OnStatusBarDblClk(wxMouseEvent& event)
{
    /* check whether the click is in the second field */
    wxRect rect;
    m_statusBar->GetFieldRect(1, rect);
    if (rect.Contains(event.GetX(), event.GetY()) && !m_editFilter->IsShown()) {
        m_editFilter->SetValue(wxEmptyString);  /* should already be empty */
        m_editFilter->SetBackgroundColour(ENABLED);
        m_editFilter->Show();
        m_editFilter->SetFocus();
        FilterChanged = false;
        m_menubar->Check(IDM_FILTER, true);
    }
}

void libmngrFrame::OnFilterChange(wxCommandEvent& event)
{
    if (!FilterChanged) {
        m_editFilter->SetBackgroundColour(CHANGED);
        m_editFilter->Refresh();
        FilterChanged = true;
    }
    event.Skip();
}

void libmngrFrame::OnFilterEnter(wxCommandEvent& /*event*/)
{
    if (FilterChanged) {
        HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
        HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
        SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
        m_panelView->Refresh();
        m_editFilter->SetBackgroundColour(ENABLED);
        /* check whether the filter must be hidden */
        wxString filter = m_editFilter->GetValue();
        if (filter.Length() > 0)
            m_editFilter->Refresh();
        else
            m_editFilter->Hide();
        FilterChanged = false;
    }
}

void libmngrFrame::OnFilterCancel(wxCommandEvent& /*event*/)
{
    wxString filter = m_editFilter->GetValue();
    m_editFilter->SetValue(wxEmptyString);  /* clear the filter */
    m_editFilter->Hide();
    if (filter.Length() > 0 || FilterChanged) {
        HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
        HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
        SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnHelp(wxCommandEvent& /*event*/)
{
    wxString filename = theApp->GetDocumentationPath() + wxT(DIRSEP_STR) wxT("kicadlibrarian.pdf");
    #if wxMAJOR_VERSION < 3
        wxTheMimeTypesManager->ReadMailcap(wxT("/etc/mailcap"));
    #endif
    wxFileType *FileType = wxTheMimeTypesManager->GetFileTypeFromMimeType(wxT("application/pdf"));
    wxString Command = FileType->GetOpenCommand(filename);
    if (Command.Find(wxT('*')) > 0)
        Command = Command.BeforeFirst(wxT('*'));
    Command.Trim();
    wxExecute(Command);
}

void libmngrFrame::OnAbout(wxCommandEvent& /*event*/)
{
    wxIcon icon(logo64_xpm);
    wxString description = wxT("A utility to view and manage footprint and symbol libraries.\n");
    #if defined NO_CURL
        description += wxT("Built without support for remote repositories.");
    #else
        description += wxT("Including support for remote repositories.");
    #endif

    wxAboutDialogInfo info;
    info.SetName(wxT("KiCad Librarian"));
    info.SetVersion(wxT(SVN_REVSTR));
    info.SetDescription(description);
    info.SetCopyright(wxT("(C) 2013-2018 ITB CompuPhase"));
    info.SetIcon(icon);
    info.SetWebSite(wxT("http://www.compuphase.com/"));
    info.AddArtist(wxT("The logo of KiCad Librarian is designed by http://icons8.com/"));
    info.AddDeveloper(wxT("KiCad Librarian uses Haru PDF for the reports, Curl to access a remote repository, UnQlite for a cache for filtering and the wxWidgets GUI library"));
    wxAboutBox(info);
}

void libmngrFrame::OnMovePart(wxCommandEvent& /*event*/)
{
    wxString modname, source, target, author;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &source, &author);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &source, &author);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());

    wxListCtrl* tgtlist = NULL;
    int syncdir = 0;
    if (leftmod.length() > 0) {
        modname = leftmod;
        long idx = m_choiceModuleRight->GetCurrentSelection();
        wxASSERT(idx >= 0 && idx < (long)m_choiceModuleRight->GetCount());
        target = m_choiceModuleRight->GetString(idx);
        tgtlist = m_listModulesRight;
        syncdir = LEFTPANEL;
    } else if (rightmod.length() > 0) {
        modname = rightmod;
        long idx = m_choiceModuleLeft->GetCurrentSelection();
        wxASSERT(idx >= 0 && idx < (long)m_choiceModuleLeft->GetCount());
        target = m_choiceModuleLeft->GetString(idx);
        tgtlist = m_listModulesLeft;
        syncdir = RIGHTPANEL;
    }
    if (source.length() == 0 || target.length() == 0) {
        wxASSERT(SyncMode);
        wxMessageBox(wxT("This part does not exist."));
        return;
    }
    wxASSERT(source.length() > 0);
    wxASSERT(target.length() > 0);

    if (SymbolMode) {
        /* first remove the symbol from the target library (if it exists) */
        if (ExistSymbol(target, modname)) {
            if (ConfirmOverwrite
                && wxMessageBox(wxT("Overwrite \"") + modname + wxT("\"\nin ") + target + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
                return;
            RemoveSymbol(target, modname);
        }

        wxASSERT(PartData[0].Count() > 0);
        if (InsertSymbol(target, modname, PartData[0])) {
            RemoveSymbol(source, modname);
            if (target.CmpNoCase(LIB_REPOS) == 0) {
                wxString preview = ExportSymbolBitmap(modname);
                StoreSymbolInfo(modname, GetDescription(PartData[0], true),
                                GetKeywords(PartData[0], true), GetAliases(PartData[0]),
                                GetFootprints(PartData[0]), preview);
                wxRemoveFile(preview);
            }
            /* both libraries need to be refreshed */
            HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
            HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
            SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
            /* find the item in the target list, to scroll to that position */
            wxASSERT(tgtlist != NULL);
            long idx = tgtlist->FindItem(-1, modname);
            if (idx >= 0)
                tgtlist->EnsureVisible(idx);
            SyncScroll(syncdir);
            m_panelView->Refresh();
        } else {
            wxMessageBox(wxT("Operation failed."));
        }
    } else {
        /* get the types of the libraries (and check) */
        int sourcetype = LibraryType(source);
        int targettype = LibraryType(target);
        if (sourcetype == VER_INVALID) {
            wxMessageBox(wxT("Unsupported file format for the source library."));
            return;
        } else if (targettype == VER_INVALID) {
            wxMessageBox(wxT("Unsupported file format for the target library."));
            return;
        }
        /* also check whether the target is an export file (instead of a library) */
        if (target.Right(10).Cmp(wxT(".kicad_mod")) == 0 || target.Right(4).Cmp(wxT(".emp")) == 0) {
            wxMessageBox(wxT("The target file is an export file instead of a library.\nFootprints cannot be added to export files."));
            return;
        }

        if (ConfirmOverwrite
            && ExistFootprint(target, modname)
            && wxMessageBox(wxT("Overwrite \"") + modname + wxT("\"\nin ") + target + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
            return;

        if (sourcetype == VER_S_EXPR && targettype == VER_S_EXPR) {
            /* create full names for source and target, then move the file */
            wxFileName old_fname(source, modname + wxT(".kicad_mod"));
            wxFileName new_fname(target, modname + wxT(".kicad_mod"));
            if (source.Right(10).Cmp(wxT(".kicad_mod")) == 0)
                old_fname = wxFileName(source);
            if (wxRenameFile(old_fname.GetFullPath(), new_fname.GetFullPath(), true)) {
                HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
                HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                /* find the item in the target list, to scroll to that position */
                wxASSERT(tgtlist != NULL);
                long idx = tgtlist->FindItem(-1, modname);
                if (idx >= 0)
                    tgtlist->EnsureVisible(idx);
                SyncScroll(syncdir);
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed."));
            }
        } else {
            /* make a copy of the name, because the user may need to adjust it */
            wxString newname = modname;
            wxArrayString sourcedata;   /* create a copy, to avoid converting the original */
            if (sourcetype == VER_S_EXPR) {
                wxASSERT(targettype != VER_S_EXPR);
                TranslateToLegacy(&sourcedata, PartData[0]);
                sourcetype = VER_MM;
            } else if (targettype == VER_S_EXPR) {
                wxASSERT(sourcetype != VER_S_EXPR);
                wxArrayString data_mm = PartData[0];
                if (sourcetype == VER_MIL)
                    TranslateUnits(data_mm, false, true);
                TranslateToSexpr(&sourcedata, data_mm);
                sourcetype = VER_S_EXPR;
                /* verify that the name is valid */
                if (!ValidateFilename(&newname))
                    return;
            } else {
                wxASSERT(sourcetype != VER_S_EXPR && targettype != VER_S_EXPR);
                sourcedata = PartData[0];
            }
            RemoveFootprint(target, newname);   /* remove the footprint from the target library */
            wxASSERT(sourcedata.Count() > 0);
            if (InsertFootprint(target, newname, sourcedata, sourcetype >= VER_MM)) {
                RemoveFootprint(source, modname);
                if (target.CmpNoCase(LIB_REPOS) == 0) {
                    double span = 0;
                    if (Footprint[0].SpanHor <= EPSILON)
                        span = Footprint[0].SpanVer;
                    else if (Footprint[0].SpanVer <= EPSILON)
                        span = Footprint[0].SpanHor;
                    else
                        span = Footprint[0].PitchVertical ? Footprint[0].SpanHor : Footprint[0].SpanVer;
                    wxString preview = ExportFootprintBitmap(modname);
                    StoreFootprintInfo(modname, GetDescription(sourcedata, false), GetKeywords(PartData[0], false),
                                       Footprint[0].Pitch, span, Footprint[0].PadCount, preview);
                    wxRemoveFile(preview);
                }
                /* both libraries need to be refreshed */
                HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, BOTHPANELS);
                HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, BOTHPANELS);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                /* find the item in the target list, to scroll to that position */
                wxASSERT(tgtlist != NULL);
                long idx = tgtlist->FindItem(-1, modname);
                if (idx >= 0)
                    tgtlist->EnsureVisible(idx);
                SyncScroll(syncdir);
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed."));
            }
        }
        if (CopyVRML)
            CopyVRMLfile(source, target, author, PartData[0]);
    }
}

void libmngrFrame::OnCopyPart(wxCommandEvent& /*event*/)
{
    wxString modname, source, target, author;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &source, &author);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &source, &author);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());

    int refresh = 0;
    if (leftmod.length() > 0) {
        modname = leftmod;
        refresh = RIGHTPANEL;   /* after copy, must refresh the right list */
        long idx = m_choiceModuleRight->GetCurrentSelection();
        wxASSERT(idx >= 0 && idx < (long)m_choiceModuleRight->GetCount());
        target = m_choiceModuleRight->GetString(idx);
    } else if (rightmod.length() > 0) {
        modname = rightmod;
        refresh = LEFTPANEL;    /* after copy, must refresh the left list */
        long idx = m_choiceModuleLeft->GetCurrentSelection();
        wxASSERT(idx >= 0 && idx < (long)m_choiceModuleLeft->GetCount());
        target = m_choiceModuleLeft->GetString(idx);
    }
    if (source.length() == 0 || target.length() == 0) {
        wxASSERT(SyncMode);
        wxMessageBox(wxT("This part does not exist."));
        return;
    }
    wxASSERT(source.length() > 0);
    wxASSERT(target.length() > 0);
    wxASSERT(target.CmpNoCase(LIB_ALL) != 0);

    if (SymbolMode) {
        /* first remove the symbol from the target library (if it exists) */
        if (ExistSymbol(target, modname)) {
            if (ConfirmOverwrite
                && wxMessageBox(wxT("Overwrite \"") + modname + wxT("\"\nin ") + target + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
                return;
            RemoveSymbol(target, modname);
        }

        wxASSERT(PartData[0].Count() > 0);
        if (InsertSymbol(target, modname, PartData[0])) {
            if (target.CmpNoCase(LIB_REPOS) == 0) {
                wxString preview = ExportSymbolBitmap(modname);
                StoreSymbolInfo(modname, GetDescription(PartData[0], true), GetKeywords(PartData[0], true),
                                GetAliases(PartData[0]), GetFootprints(PartData[0]), preview);
                wxRemoveFile(preview);
            }
            if (refresh == LEFTPANEL)
                HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
            else if (refresh == RIGHTPANEL)
                HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
            SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
            SyncScroll(refresh);
            m_panelView->Refresh();
        } else {
            wxMessageBox(wxT("Operation failed."));
        }
    } else {
        /* get the types of the libraries (and check) */
        int sourcetype = LibraryType(source);
        int targettype = LibraryType(target);
        if (sourcetype == VER_INVALID) {
            wxMessageBox(wxT("Unsupported file format for the source library."));
            return;
        } else if (targettype == VER_INVALID) {
            wxMessageBox(wxT("Unsupported file format for the target library."));
            return;
        }
        /* also check whether the target is an export file (instead of a library) */
        if (target.Right(10).Cmp(wxT(".kicad_mod")) == 0 || target.Right(4).Cmp(wxT(".emp")) == 0) {
            wxMessageBox(wxT("The target file is an export file instead of a library.\nFootprints cannot be added to export files."));
            return;
        }

        if (ConfirmOverwrite
            && ExistFootprint(target, modname)
            && wxMessageBox(wxT("Overwrite \"") + modname + wxT("\"\nin ") + target + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
            return;

        if (sourcetype == VER_S_EXPR && targettype == VER_S_EXPR) {
            /* create full names for source and target, then copy the file */
            wxFileName old_fname(source, modname + wxT(".kicad_mod"));
            wxFileName new_fname(target, modname + wxT(".kicad_mod"));
            if (source.Right(10).Cmp(wxT(".kicad_mod")) == 0)
                old_fname = wxFileName(source);
            if (wxCopyFile(old_fname.GetFullPath(), new_fname.GetFullPath(), true)) {
                if (refresh == LEFTPANEL)
                    HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
                else if (refresh == RIGHTPANEL)
                    HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                SyncScroll(refresh);
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed."));
            }
        } else {
            wxArrayString sourcedata;   /* create a copy, to avoid converting the original */
            if (sourcetype == VER_S_EXPR) {
                wxASSERT(targettype != VER_S_EXPR);
                TranslateToLegacy(&sourcedata, PartData[0]);
                sourcetype = VER_MM;
            } else if (targettype == VER_S_EXPR) {
                wxASSERT(sourcetype != VER_S_EXPR);
                wxArrayString data_mm = PartData[0];
                if (sourcetype == VER_MIL)
                    TranslateUnits(data_mm, false, true);
                TranslateToSexpr(&sourcedata, data_mm);
                sourcetype = VER_S_EXPR;
                /* verify that the name is valid */
                if (!ValidateFilename(&modname))
                    return;
            } else {
                wxASSERT(sourcetype != VER_S_EXPR && targettype != VER_S_EXPR);
                sourcedata = PartData[0];
            }
            RemoveFootprint(target, modname);   /* remove the footprint from the target library */
            wxASSERT(sourcedata.Count() > 0);
            if (InsertFootprint(target, modname, sourcedata, sourcetype >= VER_MM)) {
                if (target.CmpNoCase(LIB_REPOS) == 0) {
                    double span = 0;
                    if (Footprint[0].SpanHor <= EPSILON)
                        span = Footprint[0].SpanVer;
                    else if (Footprint[0].SpanVer <= EPSILON)
                        span = Footprint[0].SpanHor;
                    else
                        span = Footprint[0].PitchVertical ? Footprint[0].SpanHor : Footprint[0].SpanVer;
                    wxString preview = ExportFootprintBitmap(modname);
                    StoreFootprintInfo(modname, GetDescription(sourcedata, false), GetKeywords(PartData[0], false),
                                       Footprint[0].Pitch, span, Footprint[0].PadCount, preview);
                    wxRemoveFile(preview);
                }
                if (refresh == LEFTPANEL)
                    HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
                else if (refresh == RIGHTPANEL)
                    HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                SyncScroll(refresh);
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed."));
            }
        }
        if (CopyVRML)
            CopyVRMLfile(source, target, author, PartData[0]);
    }
}

void libmngrFrame::OnDeletePart(wxCommandEvent& /*event*/)
{
    wxString modname, filename;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &filename);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &filename);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());

    int refresh = 0;
    if (leftmod.length() > 0) {
        modname = leftmod;
        refresh = LEFTPANEL;    /* after deletion, must refresh the left list */
    } else if (rightmod.length() > 0) {
        modname = rightmod;
        refresh = RIGHTPANEL;   /* after deletion, must refresh the right list */
    }

    if (filename.length() == 0) {
        wxASSERT(SyncMode);
        wxMessageBox(wxT("This part does not exist."));
        return;
    }
    wxASSERT(filename.length() > 0);

    if (!ConfirmDelete || wxMessageBox(wxT("Delete \"") + modname + wxT("\"\nfrom ") + filename + wxT("?"), wxT("Confirm deletion"), wxYES_NO | wxICON_QUESTION) == wxYES) {
        bool result;
        if (SymbolMode)
            result = RemoveSymbol(filename, modname);
        else
            result = RemoveFootprint(filename, modname);
        PartEdited = false;
        if (result) {
            PartData[0].Clear();
            PartData[1].Clear();    /* should already be clear (delete operation is inactive in compare mode) */
            if (refresh == LEFTPANEL)
                HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
            else if (refresh == RIGHTPANEL)
                HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
            SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
            SyncScroll(refresh);
            m_panelView->Refresh();
            UpdateDetails(0);
        } else {
            wxMessageBox(wxT("Operation failed"));
        }
    }
}

void libmngrFrame::OnRenamePart(wxCommandEvent& /*event*/)
{
    wxString modname, filename;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &filename);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &filename);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());

    int refresh = 0;
    if (leftmod.length() > 0) {
        modname = leftmod;
        refresh = LEFTPANEL;    /* after rename, must refresh the left list */
    } else if (rightmod.length() > 0) {
        modname = rightmod;
        refresh = RIGHTPANEL;   /* after rename, must refresh the right list */
    }

    if (filename.length() == 0) {
        wxASSERT(SyncMode);
        wxMessageBox(wxT("This part does not exist."));
        return;
    }
    wxASSERT(filename.length() > 0);

    wxTextEntryDialog dlg(this, wxT("New name"), wxT("Rename ") + modname, modname);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newname = dlg.GetValue();
        if (!SymbolMode) {
            /* verify that the new name is a valid symbol (in s-expression libraries,
                 the footprint name is a filename) */
            if (!ValidateFilename(&newname))
                return;
        }
        if (newname.length() > 0) {
            bool result;
            if (SymbolMode)
                result = RenameSymbol(filename, modname, newname);
            else
                result = RenameFootprint(filename, modname, newname);
            if (result) {
                wxASSERT(refresh == LEFTPANEL || refresh == RIGHTPANEL);
                wxChoice *choice = (refresh == LEFTPANEL) ? m_choiceModuleLeft : m_choiceModuleRight;
                wxListCtrl *list = (refresh == LEFTPANEL) ? m_listModulesLeft : m_listModulesRight;
                HandleLibrarySelect(choice, list, refresh);
                /* get the index of the new part in the footprint list */
                int idx;
                for (idx = 0; idx < list->GetItemCount(); idx++) {
                    wxString symbol = list->GetItemText(idx);
                    if (symbol.Cmp(newname) == 0)
                        break;
                }
                wxASSERT(idx < list->GetItemCount());   /* it was just renamed, so it must be found */
                list->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                list->EnsureVisible(idx);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                SyncScroll(refresh);
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed"));
            }
        }
    }
}

void libmngrFrame::OnDuplicatePart(wxCommandEvent& /*event*/)
{
    wxString modname, filename, author;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &filename, &author);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &filename, &author);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());

    int refresh = 0;
    if (leftmod.length() > 0) {
        modname = leftmod;
        refresh = LEFTPANEL;    /* after duplicate, must refresh the left list */
    } else if (rightmod.length() > 0) {
        modname = rightmod;
        refresh = RIGHTPANEL;   /* after duplicate, must refresh the right list */
    }

    if (filename.length() == 0) {
        wxASSERT(SyncMode);
        wxMessageBox(wxT("This part does not exist."));
        return;
    }
    wxASSERT(filename.length() > 0);

    /* check whether the same library is selected at both sides */
    wxString otherlib = wxEmptyString;
    if (refresh < 0) {
        long idx = m_choiceModuleRight->GetCurrentSelection();
        if (idx >= 0 && idx < (long)m_choiceModuleRight->GetCount())
            otherlib = m_choiceModuleRight->GetString(idx);
    } else if (refresh > 0) {
        long idx = m_choiceModuleLeft->GetCurrentSelection();
        if (idx >= 0 && idx < (long)m_choiceModuleLeft->GetCount())
            otherlib = m_choiceModuleLeft->GetString(idx);
    }
    bool refreshboth = (filename.Cmp(otherlib) == 0);

    CheckSavePart();
    wxTextEntryDialog dlg(this, wxT("New name"), wxT("Duplicate ") + modname, modname);
    if (dlg.ShowModal() == wxID_OK) {
        wxString newname = dlg.GetValue();
        if (!SymbolMode) {
            /* verify that the new name is a valid symbol (in s-expression libraries,
                 the footprint name is a filename) */
            if (!ValidateFilename(&newname))
                return;
        }
        if (newname.length() > 0) {
            bool result = false;
            if (SymbolMode) {
                /* first remove the symbol from the library (if it exists) */
                if (ExistSymbol(filename, newname)) {
                    if (ConfirmOverwrite
                        && wxMessageBox(wxT("Overwrite \"") + newname + wxT("\"\nin ") + filename + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
                        return;
                    RemoveSymbol(filename, newname);
                }
                wxASSERT(PartData[0].Count() > 0);
                wxArrayString symbol = PartData[0]; /* create a copy */
                RenameSymbol(&symbol, modname, newname);
                result = InsertSymbol(filename, newname, symbol);
            } else {
                int type = LibraryType(filename);
                if (type == VER_INVALID) {
                    wxMessageBox(wxT("Unsupported file format for the library."));
                    return;
                }
                /* first remove the footprint from the library (if it exists) */
                if (ConfirmOverwrite
                    && ExistFootprint(filename, newname)
                    && wxMessageBox(wxT("Overwrite \"") + newname + wxT("\"\nin ") + filename + wxT("?"), wxT("Confirm overwrite"), wxYES_NO | wxICON_QUESTION) != wxYES)
                    return;
                wxArrayString module;
                if (type == VER_S_EXPR) {
                    /* create full names for source and target, then copy the file */
                    wxFileName old_fname(filename, modname + wxT(".kicad_mod"));
                    wxFileName new_fname(filename, newname + wxT(".kicad_mod"));
                    result = wxCopyFile(old_fname.GetFullPath(), new_fname.GetFullPath(), true);
                    /* adjust fields in the new file to newname */
                    wxTextFile file;
                    if (!file.Open(new_fname.GetFullPath()))
                        return;
                    /* find the index */
                    for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
                        wxString line = file.GetLine(idx);
                        module.Add(line);
                    }
                    RenameFootprint(&module, modname, newname);
                    file.Clear();
                    for (unsigned idx = 0; idx < module.Count(); idx++)
                        file.AddLine(module[idx]);
                    file.Write();
                    file.Close();
                } else {
                    RemoveFootprint(filename, newname); /* remove the footprint from the target library */
                    wxASSERT(PartData[0].Count() > 0);
                    module = PartData[0];   /* create a copy */
                    RenameFootprint(&module, modname, newname);
                    result = InsertFootprint(filename, newname, module, type >= VER_MM);
                }
                /* make a copy of the VRML file too */
                if (CopyVRML)
                    CopyVRMLfile(modname, newname, author, module);
            }
            if (result) {
                wxBusyCursor cursor;
                if (refresh < 0 || refreshboth)
                    HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
                if (refresh > 0 || refreshboth)
                    HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
                /* get the index of the new part in the footprint/symbol list */
                wxASSERT(refresh == LEFTPANEL || refresh == RIGHTPANEL);
                wxListCtrl* list = (refresh == LEFTPANEL) ? m_listModulesLeft : m_listModulesRight;
                long idx;
                for (idx = 0; idx < list->GetItemCount(); idx++) {
                    wxString symbol = list->GetItemText(idx);
                    if (symbol.Cmp(newname) == 0)
                        break;
                }
                wxASSERT(idx < list->GetItemCount());   /* it was just inserted, so it must be found */
                list->SetItemState(idx, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
                list->EnsureVisible(idx);
                LoadPart(idx, list, 0, 0);
                UpdateDetails(0);
                SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
                SyncScroll(refresh);
                m_statusBar->SetStatusText(wxT("Duplicated symbol/footprint"));
                m_panelView->Refresh();
            } else {
                wxMessageBox(wxT("Operation failed"));
            }
        }
    }
}

bool libmngrFrame::ValidateFilename(wxString* name)
{
    /* verify that the new name is a valid symbol (in s-expression libraries,
         the footprint name is a filename) */
    bool ok;
    do {
        ok = true;
        for (unsigned i = 0; ok && i < wxFileName::GetForbiddenChars().length(); i++) {
            if (name->Find(wxFileName::GetForbiddenChars().GetChar(i)) >= 0) {
                ok = false;
                wxString msg = wxString::Format(wxT("The character '%c' is invalid in a symbol name; please choose a different name"),
                                                                    wxFileName::GetForbiddenChars().GetChar(i));
                wxTextEntryDialog dlg(this, msg, wxT("Rename ") + *name, *name);
                if (dlg.ShowModal() != wxID_OK)
                    return false;
            }
        }
    } while (!ok);
    return true;
}

void libmngrFrame::OnLeftLibSelect(wxCommandEvent& /*event*/)
{
    wxString leftmod = GetSelection(m_listModulesLeft);
    if (leftmod.length() > 0) {
        PartData[0].Clear();
        #if !defined NO_3DMODEL
            sceneGraph.clear();
        #endif
        UpdateDetails(0);
        m_panelView->Refresh();
    }
    WarnNoRepository(m_choiceModuleLeft);
    HandleLibrarySelect(m_choiceModuleLeft, m_listModulesLeft, LEFTPANEL);
    SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
    #if !defined NO_CURL
        curlCleanup(true);
    #endif
}

void libmngrFrame::OnRightLibSelect(wxCommandEvent& /*event*/)
{
    wxString rightmod = GetSelection(m_listModulesRight);
    if (rightmod.length() > 0) {
        if (CompareMode) {
            PartData[1].Clear();
            UpdateDetails(1);
        } else {
            PartData[0].Clear();
            UpdateDetails(0);
        }
        #if !defined NO_3DMODEL
            sceneGraph.clear();
        #endif
        m_panelView->Refresh();
    }
    WarnNoRepository(m_choiceModuleRight);
    HandleLibrarySelect(m_choiceModuleRight, m_listModulesRight, RIGHTPANEL);
    SynchronizeLibraries(m_listModulesLeft, m_listModulesRight);
    #if !defined NO_CURL
        curlCleanup(true);
    #endif
}

void libmngrFrame::SyncScroll(int side)
{
    wxASSERT(side == LEFTPANEL || side == RIGHTPANEL);
    if (SyncMode && m_listModulesLeft->GetItemCount() > 0 && m_listModulesRight->GetItemCount() > 0) {
        wxListCtrl* src = (side == LEFTPANEL) ? m_listModulesRight : m_listModulesLeft;
        wxListCtrl* tgt = (side == LEFTPANEL) ? m_listModulesLeft : m_listModulesRight;
        long total = src->GetItemCount();
        wxASSERT(total == tgt->GetItemCount());
        long top = src->GetTopItem();
        long bottom = top + src->GetCountPerPage();
        if (bottom > total)
            bottom = total;
        tgt->EnsureVisible(top);
        tgt->EnsureVisible(bottom - 1);
    }
}

void libmngrFrame::OnLeftModSelect(wxListEvent& event)
{
    CheckSavePart();
    wxBusyCursor cursor;
    SelectedPartLeft = event.GetIndex();
    if (!CompareMode) {
        EnableButtons(SelectedPartLeft >= 0 ? LEFTPANEL : 0);
        RemoveSelection(m_listModulesRight, &SelectedPartRight);
        OffsetX = OffsetY = 0;
        if (ModelMode) {
            Scale = SCALE_DEFAULT;
            ResizeModelViewport();
        }
    }
    SyncScroll(RIGHTPANEL);
    LoadPart(SelectedPartLeft, m_listModulesLeft, m_choiceModuleLeft, 0);
    m_radioViewLeft->SetValue(true);
    m_radioViewRight->SetValue(false);
    UpdateDetails(0);
    Update3DModel(PartData[0]);
    AutoZoom(0);
    m_panelView->Refresh();
}

void libmngrFrame::OnRightModSelect(wxListEvent& event)
{
    CheckSavePart();
    wxBusyCursor cursor;
    SelectedPartRight = event.GetIndex();
    if (!CompareMode) {
        EnableButtons(SelectedPartRight >= 0 ? RIGHTPANEL : 0);
        RemoveSelection(m_listModulesLeft, &SelectedPartLeft);
        OffsetX = OffsetY = 0;
        if (ModelMode) {
            Scale = SCALE_DEFAULT;
            ResizeModelViewport();
        }
    }
    SyncScroll(LEFTPANEL);
    LoadPart(SelectedPartRight, m_listModulesRight, m_choiceModuleRight, CompareMode ? 1 : 0);
    m_radioViewLeft->SetValue(false);
    m_radioViewRight->SetValue(true);
    UpdateDetails(CompareMode ? 1 : 0);
    Update3DModel(PartData[0]);
    AutoZoom(0);
    m_panelView->Refresh();
}

void libmngrFrame::OnDetailsPanelUnsplit(wxSplitterEvent& event)
{
    m_menubar->Check(IDM_DETAILSPANEL, false);
    event.Skip();
}

void libmngrFrame::OnViewStartDrag(wxMouseEvent& event)
{
    DragOrgX = event.GetX() - OffsetX;
    DragOrgY = event.GetY() - OffsetY;
}

void libmngrFrame::OnViewDrag(wxMouseEvent& event)
{
    if (event.Dragging()) {
        OffsetX = event.GetX() - DragOrgX;
        OffsetY = event.GetY() - DragOrgY;
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnViewCentre(wxMouseEvent& /*event*/)
{
    OffsetX = OffsetY = 0;
    if (ModelMode) {
        Scale = SCALE_DEFAULT;
        ResizeModelViewport();
    }
    m_panelView->Refresh();
}

void libmngrFrame::OnExportGeneral(wxCommandEvent& /*event*/)
{
    wxString leftpath, rightpath;
    wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &leftpath);
    wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &rightpath);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());
    wxString modname, path;
    if (leftmod.length() > 0) {
        modname = leftmod;
        path = leftpath;
    } else {
        modname = rightmod;
        path = rightpath;
    }
    if (modname.IsEmpty())
        return; /* nothing to export */

    wxString libname = path.AfterLast(wxT(DIRSEP_CHAR));
    wxString dirname = path.BeforeLast(wxT(DIRSEP_CHAR)) + wxT(DIRSEP_STR);   /* strip filename from the full path */

    if (SymbolMode) {
        ExportSymbolBitmap(modname);
    } else {
        ExportFootprintBitmap(modname, false, 0, dirname + wxT("icons"));
        ExportFootprintBitmap(modname, true, 1000, dirname + wxT("layouts"));
        CacheMetadata(path, modname, true, PartData[0], Footprint[0]);
        m_statusBar->SetStatusText(wxT("Exported footprint ") + modname);
    }
}

wxString libmngrFrame::ExportSymbolBitmap(const wxString& modname)
{
    /* save a few settings, these are changed only for the image output */
    bool showlabels = ShowLabels;
    bool cmpmode = CompareMode;
    double scale = Scale;
    double offsx = OffsetX;
    double offsy = OffsetY;
    ShowLabels = true;
    CompareMode = false;
    OffsetX = OffsetY = 0;

    /* estimate scale from body size */
    double size = (BodySize[0].BodyLength >= BodySize[0].BodyWidth) ? BodySize[0].BodyLength : BodySize[0].BodyWidth;
    size += 0.3 * 25.4; /* assume 0.3 inch pins on all sides */
    Scale = IMG_SCALE_SYM / size;

    /* create the bitmaps and DCs */
    wxBitmap *bmp = new wxBitmap(IMG_WIDTH_SYM, IMG_HEIGHT_SYM, 24);
    wxMemoryDC *mc = new wxMemoryDC(*bmp);
    wxGraphicsContext *gc = wxGraphicsContext::Create(*mc);
    /* fill the image with the background colour (default = white) */
    wxBrush brush(clrSchematicMode);
    gc->SetBrush(brush);
    gc->DrawRectangle(0, 0, IMG_WIDTH_SYM, IMG_HEIGHT_SYM);

    int transp[2] = { wxALPHA_OPAQUE, wxALPHA_OPAQUE };
    DrawSymbols(gc, IMG_WIDTH_SYM / 2, IMG_HEIGHT_SYM / 2, transp);
    delete gc;
    delete mc;

    /* create the output file */
    wxString path = modname;
    int idx;
    while ((idx = path.Find('/')) >= 0 || (idx = path.Find('\\')) >= 0)
        path[idx] = '-';
    while ((idx = path.Find(' ')) >= 0)
        path[idx] = '_';
    path = wxStandardPaths::Get().GetTempDir() + wxT(DIRSEP_STR) + path + wxT(".png");
    bmp->SaveFile(path, wxBITMAP_TYPE_PNG);
    delete bmp;

    /* clean up */
    ShowLabels = showlabels;
    CompareMode = cmpmode;
    Scale = scale;
    OffsetX = offsx;
    OffsetY = offsy;

    return path;
}

wxString libmngrFrame::ExportFootprintBitmap(const wxString& modname, bool bluescreen, int dpi, const wxString& DestPath)
{
    /* save a few settings, these are changed only for the image output */
    bool showlabels = ShowLabels;
    bool centrecross = DrawCentreCross;
    bool pinnumbers = ShowPinNumbers;
    bool measurements = ShowMeasurements;
    bool cmpmode = CompareMode;
    bool outline = OutlineMode;
    double scale = Scale;   /* save, to be restored later */
    double offsx = OffsetX;
    double offsy = OffsetY;
    ShowLabels = DrawCentreCross = ShowPinNumbers = ShowMeasurements = CompareMode = false;
    OutlineMode = bluescreen;
    OffsetX = OffsetY = 0;
    int ImgWidth = IMG_WIDTH_FP;
    int ImgHeight = IMG_HEIGHT_FP;

    double hsize, vsize;
    EstimateFootprintSize(&hsize, &vsize, Footprint[0], BodySize[0]);
    if (dpi == 0) {
        /* estimate scale from body size */
        if (hsize > vsize)
            Scale = IMG_SCALE_FP / hsize;
        else
            Scale = IMG_SCALE_FP / vsize;
        if (Scale > 600 / 25.4)
            Scale = 600 / 25.4; /* limit scale */
    } else {
        Scale = (double)dpi / 25.4;
        ImgWidth = (int)(hsize * Scale * 1.25);
        ImgHeight = (int)(vsize * Scale * 1.25);
    }

    /* create the bitmaps and DCs */
    wxBitmap *bmp = new wxBitmap(ImgWidth, ImgHeight, 24);
    wxMemoryDC *mc = new wxMemoryDC(*bmp);
    if (bluescreen)
        mc->SetBackground(wxBrush(wxColour(0,0,255)));
    else
        mc->SetBackground(*wxBLACK_BRUSH);
    mc->Clear();
    wxGraphicsContext *gc = wxGraphicsContext::Create(*mc);

    int transp[2] = { wxALPHA_OPAQUE, wxALPHA_OPAQUE };
    DrawFootprints(gc, ImgWidth / 2, ImgHeight / 2, transp);
    delete gc;
    delete mc;

    /* create the output file */
    wxString path = modname;
    int idx;
    while ((idx = path.Find('/')) >= 0 || (idx = path.Find('\\')) >= 0)
        path[idx] = '-';
    while ((idx = path.Find(' ')) >= 0)
        path[idx] = '_';
    path += wxT(".png");
    wxString fullpath = DestPath.IsEmpty() ? wxStandardPaths::Get().GetTempDir() : DestPath;
    int len = fullpath.Length();
    wxASSERT(len > 0);
    if (fullpath[len - 1] != DIRSEP_CHAR)
        fullpath += wxT(DIRSEP_STR);
    fullpath += path;

    if (bluescreen) {
        wxImage img = bmp->ConvertToImage();
        img.InitAlpha();
        /* apply bluescreen */
        wxASSERT(img.HasAlpha());
        for (int iy = 0; iy < img.GetHeight(); iy++) {
            unsigned char* pix = img.GetData() + iy * 3 * img.GetWidth();
            unsigned char* msk = img.GetAlpha() + iy * img.GetWidth();
            for (int ix = 0; ix < img.GetWidth(); ix++) {
                #define K0 16
                #define K1 16
                #define K2 16
                int alpha = (K0 * pix[1] - K1 * pix[2] + (K2 * 255)) / 16;
                wxASSERT(alpha >= 0);
                *msk = (alpha < 255) ? alpha : 255;
                pix[2] = (pix[2] <= pix[1]) ? pix[2] : pix[1];
                pix += 3;
                msk += 1;
            }
        }
        img.SaveFile(fullpath, wxBITMAP_TYPE_PNG);
    } else {
        bmp->SaveFile(fullpath, wxBITMAP_TYPE_PNG);
    }
    delete bmp;

    /* clean up */
    ShowLabels = showlabels;
    DrawCentreCross = centrecross;
    ShowPinNumbers = pinnumbers;
    CompareMode = cmpmode;
    Scale = scale;
    OffsetX = offsx;
    OffsetY = offsy;
    ShowMeasurements = measurements;
    OutlineMode = outline;

    return fullpath;
}

/** Draws a string using the plotter font, using the current pen. The brush is
 *  always reset to an empty brush.
 */
void libmngrFrame::DrawStrokeText(wxGraphicsContext *gc, float x, float y, const wxString& text)
{
    /* create a stroke array for the text (applying scale and rotation) */
    std::vector<CXFPolyLine> strokes;
    VFont.DrawText(text.wc_str(wxConvLibc), &strokes);

    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    for (size_t sidx = 0; sidx < strokes.size(); sidx++) {
        const CXFPolyLine *stroke = &strokes[sidx];
        wxPoint2DDouble *points = new wxPoint2DDouble[stroke->GetCount()];
        const CXFPoint* pt;
        for (size_t pidx = 0; (pt = stroke->GetPoint(pidx)) != 0; pidx++) {
            points[pidx].m_x = x + pt->m_x;
            points[pidx].m_y = y - pt->m_y;
        }
        gc->DrawLines(stroke->GetCount(), points);
        delete[] points;
    }
}

void libmngrFrame::EstimateSymbolSize(double *cx, double *cy, const BodyInfo& Body)
{
    const int PINLENGTH = 6;    /* assume pins are 6 mm long */
    const double SCALEFACTOR = 0.25;
    wxASSERT(cx != NULL && cy != NULL);
    *cx = (Body.BodyWidth + 2 * PINLENGTH) * SCALEFACTOR;
    *cy = (Body.BodyLength + 2 * PINLENGTH) * SCALEFACTOR;
}

void libmngrFrame::EstimateFootprintSize(double *cx, double *cy, const FootprintInfo& Footprint, const BodyInfo& Body)
{
    wxASSERT(cx != NULL && cy != NULL);
    *cx = Body.BodyWidth;
    *cy = Body.BodyLength;
    double padsize = Footprint.PadSize[0].GetX() <= Footprint.PadSize[0].GetY() ? Footprint.PadSize[0].GetX() : Footprint.PadSize[0].GetY();
    if (*cx < Footprint.Pitch + padsize)
        *cx = Footprint.Pitch + padsize;
    if (*cx < Footprint.SpanHor + padsize)
        *cx = Footprint.SpanHor + padsize;
    if (*cy < Footprint.Pitch + padsize)
        *cy = Footprint.Pitch + padsize;
    if (*cy < Footprint.SpanVer + padsize)
        *cy = Footprint.SpanVer + padsize;
}

void libmngrFrame::UpdateBoundingBox(CoordSize* bbox, double x, double y)
{
    wxASSERT(bbox);
    if (x < bbox->GetLeft())
        bbox->SetLeft(x);
    else if (x > bbox->GetRight())
        bbox->SetRight(x);
    if (y < bbox->GetTop())
        bbox->SetTop(y);
    else if (y > bbox->GetBottom())
        bbox->SetBottom(y);
}

void libmngrFrame::AutoZoom(int part)
{
    /* estimate footprint size */
    wxASSERT(part == 0 || part == 1);
    double cx, cy;
    if (SymbolMode)
        EstimateSymbolSize(&cx, &cy, BodySize[part]);
    else
        EstimateFootprintSize(&cx, &cy, Footprint[part], BodySize[part]);

    /* get viewport size */
    wxSize size = m_panelView->GetSize();

    /* check whether the shape is too large to fit in the viewport */
    if (cx * Scale > size.GetWidth() * 0.8)
        Scale = size.GetWidth() * 0.8 / cx;
    if (cy * Scale > size.GetHeight() * 0.8)
        Scale = size.GetHeight() * 0.8 / cy;

    /* check whether the shape is very small in the viewport */
    if (cx * Scale < size.GetWidth() / 4 && cy * Scale < size.GetHeight() /4) {
        Scale = size.GetWidth() * 0.8 / cx;
        if (cy * Scale > size.GetHeight() * 0.8)
            Scale = size.GetHeight() * 0.8 / cy;
    }

    /* check the bounds */
    if (Scale > SCALE_MAX)
        Scale = SCALE_MAX;
    else if (Scale < SCALE_MIN)
        Scale = SCALE_MIN;
}

void libmngrFrame::DrawSymbols(wxGraphicsContext *gc, int midx, int midy, const int transp[])
{
    #define DEFAULTPEN  1
    #define PINSHAPE_SZ 40
    wxBrush brush;
    wxColour clrForeground, clrBackground, clrText, clrHiddenText;
    double scale = Scale * 0.0254 * 0.25;
    double size_pinshape = PINSHAPE_SZ * scale;

    for (int fp = 0; fp < 2; fp++) {
        /* check whether the symbol is visible */
        if (fp == 0 && CompareMode && !m_toolBar->GetToolToggled(IDT_LEFTFOOTPRINT))
            continue;
        if (fp == 1 && (!CompareMode || !m_toolBar->GetToolToggled(IDT_RIGHTFOOTPRINT)))
            continue;
        if (PartData[fp].Count() == 0)
            continue;
        /* Draw the outline, plus optionally the texts */
        if (fp == 0) {
            clrForeground.Set(128, 16, 16, transp[fp]);
            clrBackground.Set(160, 160, 40, transp[fp]);
            clrText.Set(128, 96, 32, transp[fp]);
            clrHiddenText.Set(192, 160, 128, transp[fp]);
        } else {
            clrForeground.Set(16, 128, 16, transp[fp]);
            clrBackground.Set(40, 160, 128, transp[fp]);
            clrText.Set(128, 32, 128, transp[fp]);
            clrHiddenText.Set(192, 128, 192, transp[fp]);
        }
        for (int pass = 0; pass < 2; pass++) {
            /* on first pass, only draw filled shapes, on second pass, draw the rest */
            bool indraw = false;
            double pinname_offset = size_pinshape;
            bool show_pinnr = true, show_pinname = true;
            for (int idx = 0; idx < (int)PartData[fp].Count(); idx++) {
                wxString line = PartData[fp][idx];
                wxASSERT(line.Length() > 0);
                if (line[0] == wxT('#'))
                    continue;
                wxString token = GetToken(&line);
                if (token.CmpNoCase(wxT("DEF")) == 0 && pass > 0) {
                    GetToken(&line);    /* ignore name */
                    GetToken(&line);    /* ignore designator prefix */
                    GetToken(&line);    /* ignore reserved field */
                    pinname_offset = GetTokenLong(&line) * scale;
                    show_pinnr = GetToken(&line) == wxT('Y');
                    show_pinname = GetToken(&line) == wxT('Y');
                } else if (token[0] == wxT('F') && token.Length() >= 2 && isdigit(token[1]) && pass > 0) {
                    if (ShowLabels) {
                        wxString name = GetToken(&line);
                        double x = midx + GetTokenLong(&line) * scale;
                        double y = midy - GetTokenLong(&line) * scale;
                        long rawsize = GetTokenLong(&line);
                        double size = rawsize * scale;
                        wxString orient = GetToken(&line);
                        int angle = (orient == wxT('V')) ? 90 : 0;
                        wxString visflag = GetToken(&line);
                        bool visible = (visflag == wxT('V'));
                        wxString align = GetToken(&line);
                        int horalign = CXF_ALIGNCENTRE;
                        switch ((int)align[0]) {
                        case 'L':
                            horalign = CXF_ALIGNLEFT;
                            break;
                        case 'R':
                            horalign = CXF_ALIGNRIGHT;
                            break;
                        }
                        align = GetToken(&line);
                        int veralign = CXF_ALIGNCENTRE;
                        switch ((int)align[0]) {
                        case 'T':
                            veralign = CXF_ALIGNTOP;
                            break;
                        case 'B':
                            veralign = CXF_ALIGNBOTTOM;
                            break;
                        }
                        bool bold = (align.Length() >= 3 && align[2] == wxT('B'));
                        double penwidth = bold ? (size * 0.25) : (size * 0.15);
                        if (penwidth < DEFAULTPEN)
                            penwidth = DEFAULTPEN;
                        if (rawsize > 5) {
                            wxColour penclr = visible ? clrText : clrHiddenText;
                            wxPen pen(penclr, penwidth);
                            gc->SetPen(pen);
                            if (name.Length() > 0 && name[0] == '~')
                                name = name.Mid(1);
                            VFont.SetScale(size / CXF_CAPSHEIGHT, size / CXF_CAPSHEIGHT);
                            VFont.SetItalic(false);
                            VFont.SetOverbar(false);
                            VFont.SetRotation(angle);
                            VFont.SetAlign(horalign, veralign);
                            DrawStrokeText(gc, x, y, name);
                        }
                    }
                } else if (token.CmpNoCase(wxT("DRAW")) == 0) {
                    indraw = true;
                } else if (token.CmpNoCase(wxT("ENDDRAW")) == 0) {
                    indraw = false;
                } else if (indraw) {
                    double x, y, w, h, penwidth, length, size_nr, size_name, angle, endangle;
                    long count, orientation, bold, halign, valign, part;
                    bool visible, italic;
                    wxPoint2DDouble *points;
                    wxGraphicsPath path;
                    wxString name, pin, field;
                    wxPen pen(clrForeground);
                    switch ((int)token[0]) {
                    case 'A':
                        x = midx + GetTokenLong(&line) * scale;
                        y = midy - GetTokenLong(&line) * scale;
                        w = GetTokenLong(&line) * scale;
                        angle = GetTokenLong(&line) * M_PI / 1800.0;
                        endangle = GetTokenLong(&line) * M_PI / 1800.0;
                        part = GetTokenLong(&line);
                        if (part != 0 && part != SymbolUnitNumber[fp])
                            break;          /* ignore parts other than the selected one */
                        if (GetTokenLong(&line) > 1)
                            break;          /* ignore De Morgan converted shape */
                        if ((penwidth = GetTokenLong(&line) * scale) < DEFAULTPEN)
                            penwidth = DEFAULTPEN;
                        field = GetToken(&line);    /* fill parameter (save, analyze later) */
                        if (((field == wxT('f') || field == wxT('F')) && pass == 0) || (field == wxT('N') && pass > 0)) {
                            pen.SetWidth(penwidth);
                            gc->SetPen(pen);
                            if (field == wxT('f'))
                                gc->SetBrush(clrBackground);
                            else if (field == wxT('F'))
                                gc->SetBrush(clrForeground);
                            else
                                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                            path = gc->CreatePath();
                            /* calculate the angle of rotation */
                            while (angle > M_PI)
                                angle -= 2*M_PI;
                            while (angle < -M_PI)
                                angle += 2*M_PI;
                            while (endangle > M_PI)
                                endangle -= 2*M_PI;
                            while (endangle < angle)
                                endangle += 2*M_PI;
                            h = endangle - angle;
                            wxASSERT(h > -EPSILON);
                            wxASSERT(h < 2*M_PI + EPSILON);
                            path.AddArc(x, y, w, -angle, -endangle, (h > M_PI));
                            gc->DrawPath(path);
                        }
                        break;
                    case 'B':
                        //??? Bezier curves apparently not yet handled by the KiCad Symbol Editor
                        break;
                    case 'C':
                        x = midx + GetTokenLong(&line) * scale;
                        y = midy - GetTokenLong(&line) * scale;
                        w = GetTokenLong(&line) * scale;
                        part = GetTokenLong(&line);
                        if (part != 0 && part != SymbolUnitNumber[fp])
                            break;          /* ignore parts other than the selected one */
                        if (GetTokenLong(&line) > 1)
                            break;  /* ignore De Morgan converted shape */
                        if ((penwidth = GetTokenLong(&line) * scale) < DEFAULTPEN)
                            penwidth = DEFAULTPEN;
                        field = GetToken(&line);
                        if (((field == wxT('f') || field == wxT('F')) && pass == 0) || (field == wxT('N') && pass > 0)) {
                            pen.SetWidth(penwidth);
                            gc->SetPen(pen);
                            if (field == wxT('f'))
                                gc->SetBrush(clrBackground);
                            else if (field == wxT('F'))
                                gc->SetBrush(clrForeground);
                            else
                                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                            gc->DrawEllipse(x - w, y - w, 2 * w, 2 * w);
                        }
                        break;
                    case 'P':
                        count = (int)GetTokenLong(&line);
                        part = GetTokenLong(&line);
                        if (part != 0 && part != SymbolUnitNumber[fp])
                            break;          /* ignore parts other than the selected one */
                        if (GetTokenLong(&line) > 1)
                            break;  /* ignore De Morgan converted shape */
                        if ((penwidth = GetTokenLong(&line) * scale) < DEFAULTPEN)
                            penwidth = DEFAULTPEN;
                        wxASSERT(count > 0);
                        points = new wxPoint2DDouble[count + 1];    /* reserve 1 extra for filled polygons */
                        wxASSERT(points != NULL);
                        for (int p = 0; p < count; p++) {
                            points[p].m_x = midx + GetTokenLong(&line) * scale;
                            points[p].m_y = midy - GetTokenLong(&line) * scale;
                        }
                        field = GetToken(&line);
                        if (((field == wxT('f') || field == wxT('F')) && pass == 0) || (field == wxT('N') && pass > 0)) {
                            if (field == wxT('F') || field == wxT('f')) {
                                /* filled polygons are implicitly closed */
                                points[count] = points[0];
                                count += 1;
                            }
                            pen.SetWidth(penwidth);
                            gc->SetPen(pen);
                            if (field == wxT('f'))
                                gc->SetBrush(clrBackground);
                            else if (field == wxT('F'))
                                gc->SetBrush(clrForeground);
                            else
                                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                            gc->DrawLines(count, points);
                        }
                        delete[] points;
                        break;
                    case 'S':
                        x = GetTokenLong(&line) * scale;
                        y = GetTokenLong(&line) * scale;
                        w = GetTokenLong(&line) * scale - x;
                        h =  GetTokenLong(&line) * scale - y;
                        part = GetTokenLong(&line);
                        if (part != 0 && part != SymbolUnitNumber[fp])
                            break;          /* ignore parts other than the selected one */
                        if (GetTokenLong(&line) > 1)
                            break;  /* ignore De Morgan converted shape */
                        if ((penwidth = GetTokenLong(&line) * scale) < DEFAULTPEN)
                            penwidth = DEFAULTPEN;
                        field = GetToken(&line);
                        if (((field == wxT('f') || field == wxT('F')) && pass == 0) || (field == wxT('N') && pass > 0)) {
                            if (w < 0) {
                                x += w;
                                w = -w;
                            }
                            if (h < 0) {
                                y += h;
                                h = -h;
                            }
                            pen.SetWidth(penwidth);
                            gc->SetPen(pen);
                            if (field == wxT('f'))
                                gc->SetBrush(clrBackground);
                            else if (field == wxT('F'))
                                gc->SetBrush(clrForeground);
                            else
                                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                            gc->DrawRectangle(midx + x, midy - (y + h), w, h);
                        }
                        break;
                    case 'T':
                        if (pass > 0) {
                            angle = GetTokenLong(&line) / 10.0;
                            x = midx + GetTokenLong(&line) * scale;
                            y = midy - GetTokenLong(&line) * scale;
                            h = GetTokenLong(&line) * scale;    /* text size */
                            visible = GetTokenLong(&line) == 0;
                            part = GetTokenLong(&line);
                            if (part != 0 && part != SymbolUnitNumber[fp])
                                break;          /* ignore parts other than the selected one */
                            if (GetTokenLong(&line) > 1)
                                break;  /* ignore De Morgan converted shape */
                            name = GetToken(&line);
                            field = GetToken(&line);
                            italic = field.CmpNoCase(wxT("italic")) == 0;
                            bold = GetTokenLong(&line);
                            field = GetToken(&line);
                            if (field == wxT('L'))
                                halign = CXF_ALIGNLEFT;
                            else if (field == wxT('R'))
                                halign = CXF_ALIGNRIGHT;
                            else
                                halign = CXF_ALIGNCENTRE;
                            field = GetToken(&line);
                            if (field == wxT('T'))
                                valign = CXF_ALIGNTOP;
                            else if (field == wxT('B'))
                                valign = CXF_ALIGNBOTTOM;
                            else
                                valign = CXF_ALIGNCENTRE;
                            pen.SetWidth(bold ? h * 0.25 : h * 0.15);
                            pen.SetColour(visible ? clrText : clrHiddenText);
                            gc->SetPen(pen);
                            VFont.SetScale(h / CXF_CAPSHEIGHT, h / CXF_CAPSHEIGHT);
                            VFont.SetItalic(italic);
                            VFont.SetOverbar(false);
                            VFont.SetRotation((int)angle);
                            VFont.SetAlign(halign, valign);
                            DrawStrokeText(gc, x, y, name);
                        }
                        break;
                    case 'X':
                        if (pass > 0) {
                            name = GetToken(&line);
                            if (name == wxT('~'))
                                name = wxEmptyString;
                            pin = GetToken(&line);
                            if (pin == wxT('~'))
                                pin = wxEmptyString;
                            points = new wxPoint2DDouble[2];
                            wxASSERT(points != NULL);
                            points[0].m_x = midx + GetTokenLong(&line) * scale;
                            points[0].m_y = midy - GetTokenLong(&line) * scale;
                            length = GetTokenLong(&line) * scale;
                            field = GetToken(&line);
                            orientation = field[0];
                            size_nr = GetTokenLong(&line) * scale;
                            size_name = GetTokenLong(&line) * scale;
                            part = GetTokenLong(&line);
                            if (part != 0 && part != SymbolUnitNumber[fp])
                                break;          /* ignore parts other than the selected one */
                            if (GetTokenLong(&line) > 1)
                                break;  /* ignore De Morgan converted shape */
                            GetToken(&line);    /* ignore type */
                            field = GetToken(&line);    /* pin shape */
                            points[1] = points[0];
                            switch (orientation) {
                            case 'L':
                                points[1].m_x = points[0].m_x - length;
                                angle = 0;
                                break;
                            case 'R':
                                points[1].m_x = points[0].m_x + length;
                                angle = 0;
                                break;
                            case 'U':
                                points[1].m_y = points[0].m_y - length;
                                angle = 90;
                                break;
                            case 'D':
                                points[1].m_y = points[0].m_y + length;
                                angle = 90;
                                break;
                            default:
                                wxASSERT(false);
                                angle = 0;      /* just to avoid a compiler warning */
                            }
                            pen.SetWidth(DEFAULTPEN);
                            gc->SetPen(pen);
                            gc->DrawLines(2, points);
                            if (field.Length() > 0) {
                                gc->SetBrush(clrBackground);
                                if (field == wxT('I') || field == wxT("CI")) {
                                    /* inverted or inverted clock */
                                    wxPoint2DDouble ptShape = points[1];
                                    if (!Equal(points[0].m_x, ptShape.m_x)) {
                                        int sign = (points[0].m_x < ptShape.m_x) ? -1 : 1;
                                        ptShape.m_x += sign * (size_pinshape + 1) / 2;
                                    } else if (!Equal(points[0].m_y, ptShape.m_y)) {
                                        int sign = (points[0].m_y < ptShape.m_y) ? -1 : 1;
                                        ptShape.m_y += sign * (size_pinshape + 1) / 2;
                                    }
                                    gc->DrawEllipse(ptShape.m_x - size_pinshape / 2, ptShape.m_y - size_pinshape / 2, size_pinshape, size_pinshape);
                                }
                                if (field == wxT('C') || field == wxT("CI")) {
                                    /* clock or inverted clock */
                                    wxPoint2DDouble ptShape[3];
                                    for (int i = 0; i < 3; i++)
                                        ptShape[i] = points[1];
                                    if (!Equal(points[0].m_x, ptShape[1].m_x)) {
                                        int sign = (points[0].m_x < ptShape[1].m_x) ? 1 : -1;
                                        ptShape[1].m_x += sign * (size_pinshape + 1) / 2;
                                        ptShape[0].m_y -= (size_pinshape + 1) / 2;
                                        ptShape[2].m_y += (size_pinshape + 1) / 2;
                                    } else if (!Equal(points[0].m_y, ptShape[1].m_y)) {
                                        int sign = (points[0].m_y < ptShape[1].m_y) ? 1 : -1;
                                        ptShape[1].m_y += sign * (size_pinshape + 1) / 2;
                                        ptShape[0].m_x -= (size_pinshape + 1) / 2;
                                        ptShape[2].m_x += (size_pinshape + 1) / 2;
                                    }
                                    gc->DrawLines(3, ptShape);
                                }
                            }
                            gc->SetBrush(clrForeground);
                            gc->DrawEllipse(points[0].m_x - 2, points[0].m_y - 2, 4, 4);    /* cicle at the endpoint */
                            /* pin name and number */
                            pen.SetColour(clrText);
                            if (show_pinnr) {
                                pen.SetWidth(size_nr * 0.125);
                                gc->SetPen(pen);
                                VFont.SetScale(size_nr / CXF_CAPSHEIGHT, size_nr / CXF_CAPSHEIGHT);
                                VFont.SetItalic(false);
                                VFont.SetOverbar(false);
                                VFont.SetRotation((int)angle);
                                VFont.SetAlign(CXF_ALIGNCENTRE, CXF_ALIGNBOTTOM);
                                if (angle > EPSILON) {
                                    x = points[1].m_x;
                                    y = (points[0].m_y + points[1].m_y) / 2;
                                } else {
                                    x = (points[0].m_x + points[1].m_x) / 2;
                                    y = points[1].m_y;
                                }
                                DrawStrokeText(gc, x, y, pin);
                            }
                            if (show_pinname) {
                                pen.SetWidth(size_name * 0.15);
                                gc->SetPen(pen);
                                VFont.SetScale(size_name / CXF_CAPSHEIGHT, size_name / CXF_CAPSHEIGHT);
                                VFont.SetItalic(false);
                                if (name.length() > 0 && name[0] == wxT('~')) {
                                    name = name.Mid(1);
                                    VFont.SetOverbar(true);
                                }
                                if (pinname_offset < EPSILON) {
                                    /* pin name is outside the shape */
                                    VFont.SetAlign(CXF_ALIGNCENTRE, CXF_ALIGNTOP);
                                    if (angle > EPSILON) {
                                        x = points[1].m_x;
                                        y = (points[0].m_y + points[1].m_y) / 2;
                                    } else {
                                        x = (points[0].m_x + points[1].m_x) / 2;
                                        y = points[1].m_y;
                                    }
                                } else {
                                    /* pin name is inside the shape */
                                    if (pinname_offset < size_name / 2)
                                        pinname_offset = size_name / 2;
                                    if (angle > EPSILON) {
                                        x = points[1].m_x;
                                        if (points[0].m_y < points[1].m_y) {
                                            y = points[1].m_y + pinname_offset;
                                            VFont.SetAlign(CXF_ALIGNRIGHT, CXF_ALIGNCENTRE);
                                        } else {
                                            y = points[1].m_y - pinname_offset;
                                            VFont.SetAlign(CXF_ALIGNLEFT, CXF_ALIGNCENTRE);
                                        }
                                    } else {
                                        if (points[0].m_x < points[1].m_x) {
                                            x = points[1].m_x + pinname_offset;
                                            VFont.SetAlign(CXF_ALIGNLEFT, CXF_ALIGNCENTRE);
                                        } else {
                                            x = points[1].m_x - pinname_offset;
                                            VFont.SetAlign(CXF_ALIGNRIGHT, CXF_ALIGNCENTRE);
                                        }
                                        y = points[1].m_y;
                                    }
                                }
                                DrawStrokeText(gc, x, y, name);
                            }
                            delete[] points;
                        }
                        break;
                    } /* switch token */
                }
            } /* for (idx over PartData[fp]) */
        } /* for (pass) */
    }
}

void libmngrFrame::DrawPad(wxGraphicsContext *gc, double midx, double midy,
                           const wxString& padshape, const wxString& padpin,
                           double padx, double pady, double padwidth, double padheight,
                           double padrratio, double paddeltax, double paddeltay,
                           double pasteratio, double padrot,
                           double drillx, double drilly, double drillwidth, double drillheight,
                           const wxPen& penPad, const wxBrush& brushPad, const wxBrush& brushHole,
                           CoordSize *bbox)
{
    /* make the pad smaller in outline mode */
    if (OutlineMode) {
        if (padwidth > 0.3)
            padwidth -= 0.15;
        if (padheight > 0.3)
            padheight -= 0.15;
    }

    /* draw the pad */
    wxPoint2DDouble points[5];
    points[0].m_x = -padwidth/2 * Scale;
    points[0].m_y = -padheight/2 * Scale;
    points[1].m_x =  padwidth/2 * Scale;
    points[1].m_y = -padheight/2 * Scale;
    points[2].m_x =  padwidth/2 * Scale;
    points[2].m_y =  padheight/2 * Scale;
    points[3].m_x = -padwidth/2 * Scale;
    points[3].m_y =  padheight/2 * Scale;
    if (padshape.CmpNoCase(wxT("T")) == 0 || padshape.Cmp(wxT("trapezoid")) == 0) {
        if (!Equal(paddeltax, 0.0)) {
            points[0].m_y -= paddeltax * Scale / 2;
            points[1].m_y += paddeltax * Scale / 2;
            points[2].m_y -= paddeltax * Scale / 2;
            points[3].m_y += paddeltax * Scale / 2;
        }
        if (!Equal(paddeltay, 0.0)) {
            points[0].m_x += paddeltay * Scale / 2;
            points[1].m_x -= paddeltay * Scale / 2;
            points[2].m_x += paddeltay * Scale / 2;
            points[3].m_x -= paddeltay * Scale / 2;
        }
    }
    points[4] = points[0];

    /* make scaled pad for paste */
    wxPoint2DDouble pastepoints[5];
    memcpy(pastepoints, points, sizeof pastepoints);
    if (pasteratio < -EPSILON && pasteratio > -0.5 && !OutlineMode) {
        double d;
        d = points[1].m_x - points[0].m_x;
        pastepoints[0].m_x = points[0].m_x - d * pasteratio;
        pastepoints[1].m_x = points[1].m_x + d * pasteratio;
        d = points[2].m_x - points[3].m_x;
        pastepoints[2].m_x = points[2].m_x + d * pasteratio;
        pastepoints[3].m_x = points[3].m_x - d * pasteratio;
        d = points[3].m_y - points[0].m_y;
        pastepoints[0].m_y = points[0].m_y - d * pasteratio;
        pastepoints[3].m_y = points[3].m_y + d * pasteratio;
        d = points[2].m_y - points[1].m_y;
        pastepoints[1].m_y = points[1].m_y - d * pasteratio;
        pastepoints[2].m_y = points[2].m_y + d * pasteratio;
        pastepoints[4] = pastepoints[0];
    }

    /* apply rotation */
    if (padrot > EPSILON) {
        double angle = (padrot * M_PI / 180.0);
        for (int idx = 0; idx < 5; idx++) {
            wxDouble nx = points[idx].m_x * cos(angle) - points[idx].m_y * sin(angle);
            wxDouble ny = points[idx].m_x * sin(angle) + points[idx].m_y * cos(angle);
            points[idx].m_x = nx;
            points[idx].m_y = ny;
            /* same for paste aperture */
            nx = pastepoints[idx].m_x * cos(angle) - pastepoints[idx].m_y * sin(angle);
            ny = pastepoints[idx].m_x * sin(angle) + pastepoints[idx].m_y * cos(angle);
            pastepoints[idx].m_x = nx;
            pastepoints[idx].m_y = ny;
        }
    }

    /* move pad relative to footprint origin */
    for (int idx = 0; idx < 5; idx++) {
        points[idx].m_x += padx * Scale + midx;
        points[idx].m_y += pady * Scale + midy;
        UpdateBoundingBox(bbox, padx, pady);
        pastepoints[idx].m_x += padx * Scale + midx;
        pastepoints[idx].m_y += pady * Scale + midy;
    }

    gc->SetBrush(brushPad);
    gc->SetPen(penPad);

    /* avoid negative width/height for ellipses or obrounds */
    CoordSize cs(points[0].m_x, points[0].m_y, points[2].m_x - points[0].m_x, points[2].m_y - points[0].m_y);
    if (padshape.CmpNoCase(wxT("C")) == 0 || padshape.Cmp(wxT("circle")) == 0) {
        gc->DrawEllipse(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight());
    } else if (padshape.CmpNoCase(wxT("O")) == 0 || padshape.Cmp(wxT("oval")) == 0) {
        double dim = (cs.GetWidth() < cs.GetHeight()) ? cs.GetWidth() : cs.GetHeight();
        gc->DrawRoundedRectangle(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight(), dim / 2);
    } else if (padshape.CmpNoCase(wxT("D")) == 0 || padshape.Cmp(wxT("roundrect")) == 0) {
        wxASSERT(padrratio >= 0 && padrratio <= 0.5);
        double dim = (cs.GetWidth() < cs.GetHeight()) ? cs.GetWidth() : cs.GetHeight();
        gc->DrawRoundedRectangle(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight(), dim * padrratio);
    } else {
        gc->DrawLines(5, points);
    }

    /* draw solder paste ratio, if set */
    if (pasteratio < -EPSILON && pasteratio > -0.5 && !OutlineMode) {
        wxPen pastepen(wxColour(160,160,80), 1, wxPENSTYLE_DOT);
        gc->SetPen(pastepen);
        wxBrush pastebrush(wxColour(160,160,80), wxBRUSHSTYLE_FDIAGONAL_HATCH);
        gc->SetBrush(pastebrush);
        /* avoid negative width/height for ellipses or obrounds */
        cs.Set(pastepoints[0].m_x, pastepoints[0].m_y, pastepoints[2].m_x - pastepoints[0].m_x, pastepoints[2].m_y - pastepoints[0].m_y);
        if (padshape.CmpNoCase(wxT("C")) == 0 || padshape.Cmp(wxT("circle")) == 0) {
            gc->DrawEllipse(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight());
        } else if (padshape.CmpNoCase(wxT("O")) == 0 || padshape.Cmp(wxT("oval")) == 0) {
            double dim = (cs.GetWidth() < cs.GetHeight()) ? cs.GetWidth() : cs.GetHeight();
            gc->DrawRoundedRectangle(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight(), dim / 2);
        } else if (padshape.CmpNoCase(wxT("D")) == 0 || padshape.Cmp(wxT("roundrect")) == 0) {
            wxASSERT(padrratio >= 0 && padrratio <= 0.5);
            double dim = (cs.GetWidth() < cs.GetHeight()) ? cs.GetWidth() : cs.GetHeight();
            gc->DrawRoundedRectangle(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight(), dim * padrratio);
        } else {
            gc->DrawLines(5, pastepoints);
        }
        gc->SetPen(penPad);
        gc->SetBrush(brushPad);
    }

    /* optionally the hole in the pad */
    if (drillwidth > EPSILON) {
        if ((padshape == wxT('C') || padshape.Cmp(wxT("circle")) == 0) && padwidth - drillwidth < 0.05)
            gc->SetBrush(brushHole);
        else
            gc->SetBrush(*wxBLACK_BRUSH);
        if (drillheight > EPSILON) {
            if ((padrot > 45 && padrot < 135) || (padrot > 225 && padrot < 315)) {
                double t = drillwidth;
                drillwidth = drillheight;
                drillheight = t;
            }
            cs.Set((padx + drillx - drillwidth/2) * Scale + midx,
                   (pady + drilly - drillheight/2) * Scale + midy,
                   drillwidth * Scale, drillheight * Scale);
            gc->DrawRoundedRectangle(cs.GetX(), cs.GetY(), cs.GetWidth(), cs.GetHeight(),
                                     ((cs.GetWidth() < cs.GetHeight()) ? cs.GetWidth() : cs.GetHeight()) / 2);
        } else {
            gc->DrawEllipse((padx + drillx - drillwidth/2) * Scale + midx,
                            (pady + drilly - drillwidth/2) * Scale + midy,
                            drillwidth * Scale, drillwidth * Scale);
        }
    }

    /* draw the pin name inside the pad */
    if (ShowPinNumbers) {
        wxDouble tw, th, td, tex;
        gc->GetTextExtent(padpin, &tw, &th, &td, &tex);
        gc->DrawText(padpin, padx * Scale + midx - tw/2, pady * Scale + midy - th/2);
    }
}

void libmngrFrame::DrawFootprints(wxGraphicsContext *gc, int midx, int midy, const int transp[])
{
    CoordSize bbox;
    wxColour clrBody, clrText, clrHiddenText, clrPad, clrPadFill, clrPadRev, clrPadRevFill;
    wxPoint2DDouble points[5];
    wxPen pen;

    /* check whether there is at least one footprint visible */
    if (PartData[0].Count() == 0 && PartData[1].Count() == 0) {
        clrText.Set(128, 128, 128);
        wxFont font(24, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        font.SetPointSize(24);
        gc->SetFont(font, clrText);
        wxString text = wxT("FOOTPRINT DOES NOT EXIST");
        wxDouble tw, th, td, tex;
        gc->GetTextExtent(text, &tw, &th, &td, &tex);
        gc->DrawText(text, midx - tw/2, midy - th/2);
        return;
    }

    for (int fp = 0; fp < 2; fp++) {
        /* check whether the footprint is visible */
        if (fp == 0 && CompareMode && !m_toolBar->GetToolToggled(IDT_LEFTFOOTPRINT))
            continue;
        if (fp == 1 && (!CompareMode || !m_toolBar->GetToolToggled(IDT_RIGHTFOOTPRINT)))
            continue;
        if (PartData[fp].Count() == 0)
            continue;
        if (fp == 0) {
            clrBody.Set(192, 192, 96, transp[fp]);
            clrPad.Set(160, 0, 0, transp[fp]);
            clrPadFill.Set(160, 48, 48, transp[fp]);
            clrPadRev.Set(160, 0, 128, transp[fp]);
            clrPadRevFill.Set(160, 48, 128, transp[fp]);
            clrText.Set(240, 240, 64, transp[fp]);
            clrHiddenText.Set(160, 160, 50, transp[fp]);
        } else {
            clrBody.Set(192, 96, 192, transp[fp]);
            clrPad.Set(0, 160, 0, transp[fp]);
            clrPadFill.Set(48, 160, 48, transp[fp]);
            clrPadRev.Set(128, 160, 0, transp[fp]);
            clrPadRevFill.Set(128, 160, 48, transp[fp]);
            clrText.Set(240, 64, 240, transp[fp]);
            clrHiddenText.Set(160, 50, 160, transp[fp]);
        }
        if (OutlineMode)
            clrBody.Set(80, 80, 80, transp[fp]);
        /* Draw the outline, plus optionally the texts */
        pen.SetColour(clrBody);
        gc->SetPen(pen);
        gc->SetBrush(*wxTRANSPARENT_BRUSH);
        bool unit_mm = Footprint[fp].Type >= VER_MM;
        double module_angle = 0;    /* all angles should be corrected with the footprint angle */
        for (int idx = 0; idx < (int)PartData[fp].Count(); idx++) {
            wxString line = PartData[fp][idx];
            wxString token = GetToken(&line);
            if (token.CmpNoCase(wxT("Po")) == 0) {
                GetToken(&line);    /* ignore X position */
                GetToken(&line);    /* ignore Y position */
                if (line.length() > 0)
                    module_angle = GetTokenLong(&line) / 10.0;
            } else if (token.CmpNoCase(wxT("DS")) == 0) {
                double x1 = GetTokenDim(&line, unit_mm);
                double y1 = GetTokenDim(&line, unit_mm);
                double x2 = GetTokenDim(&line, unit_mm);
                double y2 = GetTokenDim(&line, unit_mm);
                double penwidth = GetTokenDim(&line, unit_mm);
                /* ignore layer */
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                points[0].m_x = x1 * Scale + midx;
                points[0].m_y = y1 * Scale + midy;
                points[1].m_x = x2 * Scale + midx;
                points[1].m_y = y2 * Scale + midy;
                gc->DrawLines(2, points);
                UpdateBoundingBox(&bbox, x1, y1);
                UpdateBoundingBox(&bbox, x2, y2);
            } else if (token.CmpNoCase(wxT("DC")) == 0) {
                double x = GetTokenDim(&line, unit_mm);
                double y = GetTokenDim(&line, unit_mm);
                double dx = GetTokenDim(&line, unit_mm);
                double dy = GetTokenDim(&line, unit_mm);
                double penwidth = GetTokenDim(&line, unit_mm);
                /* ignore layer */
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                dx -= x;
                dy -= y;
                double radius = sqrt(dx * dx + dy * dy);
                gc->DrawEllipse((x - radius) * Scale + midx, (y - radius) * Scale + midy,
                                2 * radius * Scale, 2 * radius * Scale);
                UpdateBoundingBox(&bbox, x - radius, y - radius, 2 * radius, 2 * radius);
            } else if (token.CmpNoCase(wxT("DA")) == 0) {
                double x = GetTokenDim(&line, unit_mm);
                double y = GetTokenDim(&line, unit_mm);
                double dx = GetTokenDim(&line, unit_mm);
                double dy = GetTokenDim(&line, unit_mm);
                double angle = GetTokenLong(&line) / 10.0;
                double penwidth = GetTokenDim(&line, unit_mm);
                /* ignore layer */
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                dx -= x;
                dy -= y;
                double radius = sqrt(dx * dx + dy * dy);
                double startangle = atan2(dy, dx);
                double endangle = startangle + (double)angle * M_PI / 180.0;
                if (endangle > 2 * M_PI)
                    endangle -= 2 * M_PI;
                wxGraphicsPath path = gc->CreatePath();
                path.AddArc(x * Scale + midx, y * Scale + midy, radius * Scale, startangle, endangle, true);
                gc->DrawPath(path);
                /* for the bounding box, assume that the arc is a circle (the
                   bounding box does not need to be exact) */
                UpdateBoundingBox(&bbox, x - radius, y - radius, 2 * radius, 2 * radius);
            } else if (token.CmpNoCase(wxT("DP")) == 0) {
                GetToken(&line);    /* ignore first four unknown values */
                GetToken(&line);
                GetToken(&line);
                GetToken(&line);
                long count = GetTokenLong(&line);
                wxASSERT(count > 0);
                double penwidth = GetTokenDim(&line, unit_mm);
                /* ignore layer */
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                if (OutlineMode) {
                    gc->SetBrush(*wxTRANSPARENT_BRUSH);
                } else {
                    wxBrush brush(clrBody);
                    gc->SetBrush(brush);
                }
                wxPoint2DDouble *pt = new wxPoint2DDouble[count];
                wxASSERT(pt != NULL);
                for (long p = 0; p < count; p++) {
                    idx++;
                    line = PartData[fp][idx];
                    token = GetToken(&line);
                    wxASSERT(token.CmpNoCase(wxT("Dl")) == 0);
                    double x = GetTokenDim(&line, unit_mm);
                    double y = GetTokenDim(&line, unit_mm);
                    pt[p].m_x = x * Scale + midx;
                    pt[p].m_y = y * Scale + midy;
                    UpdateBoundingBox(&bbox, x, y);
                }
                gc->DrawLines(count, pt);
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                delete[] pt;
            } else if (toupper(token[0]) == 'T' && isdigit(token[1])) {
                long field;
                if (ShowLabels || (token.Mid(1).ToLong(&field) && field >= 2)) {
                    double x = GetTokenDim(&line, unit_mm);
                    double y = GetTokenDim(&line, unit_mm);
                    double cy = GetTokenDim(&line, unit_mm);
                    double cx = GetTokenDim(&line, unit_mm);
                    double rot = NormalizeAngle(GetTokenLong(&line) / 10.0 - module_angle);
                    double penwidth = GetTokenDim(&line, unit_mm);
                    GetToken(&line);        /* ignore mirror flag */
                    wxString visflag = GetToken(&line);
                    bool visible = (visflag == wxT('V'));
                    GetToken(&line);        /* ignore layer */
                    bool italic = false;
                    if (line[0] == '"' || (line.length() > 1 && line[1] == '"')) {
                        /* the italic flag is absent or it is glued to the text */
                        if (line[0] != '"') {
                            italic = (token[0] == 'I');
                            line = line.Mid(1);
                        }
                        wxASSERT(line[0] == '"');
                        token = GetToken(&line);
                    } else {
                        token = GetToken(&line);
                        italic = (token.Length() > 0) && (token[0] == 'I');
                        token = GetToken(&line);
                    }
                    penwidth *= Scale;
                    if (penwidth < DEFAULTPEN)
                        penwidth = DEFAULTPEN;
                    wxPen tpen;
                    tpen.SetWidth(penwidth);
                    tpen.SetColour(visible ? clrText : clrHiddenText);
                    gc->SetPen(tpen);
                    VFont.SetScale(cx / CXF_CAPSHEIGHT * Scale, cy / CXF_CAPSHEIGHT * Scale);
                    VFont.SetItalic(italic);
                    VFont.SetOverbar(false);
                    VFont.SetRotation((int)rot);
                    VFont.SetAlign(CXF_ALIGNCENTRE, CXF_ALIGNCENTRE);
                    DrawStrokeText(gc, x * Scale + midx, y * Scale + midy, token);
                    UpdateBoundingBox(&bbox, x, y); //??? calculate the bounding box of the text
                }
            } else if (token.Cmp(wxT("(at")) == 0) {
                GetToken(&line);    /* ignore X */
                GetToken(&line);    /* ignore Y */
                if (line.length() > 0)
                    module_angle = GetTokenDouble(&line);
            } else if (token.Cmp(wxT("(fp_line")) == 0) {
                double x1=0, y1=0, x2=0, y2=0, penwidth=0;
                wxString section = GetSection(line, wxT("start"));
                if (section.length() > 0) {
                    x1 = GetTokenDim(&section, true);
                    y1 = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("end"));
                if (section.length() > 0) {
                    x2 = GetTokenDim(&section, true);
                    y2 = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("width"));
                if (section.length() > 0)
                    penwidth = GetTokenDim(&section, true);
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                points[0].m_x = x1 * Scale + midx;
                points[0].m_y = y1 * Scale + midy;
                points[1].m_x = x2 * Scale + midx;
                points[1].m_y = y2 * Scale + midy;
                gc->DrawLines(2, points);
                UpdateBoundingBox(&bbox, x1, y1);
                UpdateBoundingBox(&bbox, x2, y2);
            } else if (token.Cmp(wxT("(fp_circle")) == 0) {
                double x=0, y=0, dx=0, dy=0, penwidth=0;
                wxString section = GetSection(line, wxT("center"));
                if (section.length() > 0) {
                    x = GetTokenDim(&section, true);
                    y = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("end"));
                if (section.length() > 0) {
                    dx = GetTokenDim(&section, true);
                    dy = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("width"));
                if (section.length() > 0)
                    penwidth = GetTokenDim(&section, true);
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                dx -= x;
                dy -= y;
                double radius = sqrt(dx * dx + dy * dy);
                gc->DrawEllipse((x - radius) * Scale + midx, (y - radius) * Scale + midy,
                                2 * radius * Scale, 2 * radius * Scale);
                UpdateBoundingBox(&bbox, x - radius, y - radius, 2 * radius, 2 * radius);
            } else if (token.Cmp(wxT("(fp_arc")) == 0) {
                double x=0, y=0, dx=0, dy=0, penwidth=0;
                double angle = 0;
                wxString section = GetSection(line, wxT("start"));
                if (section.length() > 0) {
                    x = GetTokenDim(&section, true);
                    y = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("end"));
                if (section.length() > 0) {
                    dx = GetTokenDim(&section, true);
                    dy = GetTokenDim(&section, true);
                }
                section = GetSection(line, wxT("angle"));
                if (section.length() > 0)
                    angle = GetTokenDouble(&section);
                section = GetSection(line, wxT("width"));
                if (section.length() > 0)
                    penwidth = GetTokenDim(&section, true);
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                dx -= x;
                dy -= y;
                double radius = sqrt(dx * dx + dy * dy);
                double startangle = atan2(dy, dx);
                double endangle = startangle + (double)angle * M_PI / 180.0;
                if (endangle > 2 * M_PI)
                    endangle -= 2 * M_PI;
                wxGraphicsPath path = gc->CreatePath();
                path.AddArc(x * Scale + midx, y * Scale + midy, radius * Scale, startangle, endangle, true);
                gc->DrawPath(path);
                /* for the bounding box, assume that the arc is a circle (the
                   bounding box does not need to be exact) */
                UpdateBoundingBox(&bbox, x - radius, y - radius, 2 * radius, 2 * radius);
            } else if (token.Cmp(wxT("(fp_poly")) == 0) {
                double penwidth = 0;
                wxString section = GetSection(line, wxT("width"));
                if (section.length() > 0)
                    penwidth = GetTokenDim(&section, true);
                penwidth *= Scale;
                if (penwidth < DEFAULTPEN)
                    penwidth = DEFAULTPEN;
                pen.SetWidth(penwidth);
                gc->SetPen(pen);
                if (OutlineMode) {
                    gc->SetBrush(*wxTRANSPARENT_BRUSH);
                } else {
                    wxBrush brush(clrBody);
                    gc->SetBrush(brush);
                }
                /* first count the number of vertices */
                section = GetSection(line, wxT("pts"));
                long count = 0;
                while (GetSection(section, wxT("xy"), count).Length() > 0)
                    count++;
                wxPoint2DDouble *pt = new wxPoint2DDouble[count];
                wxASSERT(pt != NULL);
                for (long p = 0; p < count; p++) {
                    wxString subsect = GetSection(section, wxT("xy"), p);
                    wxASSERT(subsect.length() > 0);
                    double x = GetTokenDim(&subsect, true);
                    double y = GetTokenDim(&subsect, true);
                    pt[p].m_x = x * Scale + midx;
                    pt[p].m_y = y * Scale + midy;
                    UpdateBoundingBox(&bbox, x, y);
                }
                gc->DrawLines(count, pt);
                gc->SetBrush(*wxTRANSPARENT_BRUSH);
                delete[] pt;
            } else if (token.Cmp(wxT("(fp_text")) == 0) {
                wxString ident = GetToken(&line);
                if (ShowLabels || ident.Cmp(wxT("user")) == 0) {
                    double x = 0, y = 0, cx = 0, cy = 0, penwidth = 1;
                    double rot = 0;
                    token = GetToken(&line);    /* get the text itself */
                    bool visible = line.Find(wxT(" hide ")) ==  wxNOT_FOUND;
                    wxString section = GetSection(line, wxT("at"));
                    if (section.length() > 0) {
                        x = GetTokenDim(&section, true);
                        y = GetTokenDim(&section, true);
                        if (section.length() > 0)
                            rot = NormalizeAngle(GetTokenDouble(&section) - module_angle);
                    }
                    section = GetSection(line, wxT("effects"));
                    if (section.length() > 0) {
                        section = GetSection(section, wxT("font"));
                        if (section.length() > 0) {
                            wxString subsect = GetSection(section, wxT("size"));
                            if (subsect.length() > 0) {
                                cy = GetTokenDim(&subsect, true);
                                cx = GetTokenDim(&subsect, true);
                            }
                            subsect = GetSection(section, wxT("thickness"));
                            if (subsect.length() > 0)
                                penwidth = GetTokenDim(&subsect, true);
                        }
                    }
                    penwidth *= Scale;
                    if (penwidth < DEFAULTPEN)
                        penwidth = DEFAULTPEN;
                    wxPen tpen;
                    tpen.SetWidth(penwidth);
                    tpen.SetColour(visible ? clrText : clrHiddenText);
                    gc->SetPen(tpen);
                    VFont.SetScale(cx / CXF_CAPSHEIGHT * Scale, cy / CXF_CAPSHEIGHT * Scale);
                    VFont.SetOverbar(false);
                    VFont.SetRotation((int)rot);
                    VFont.SetAlign(CXF_ALIGNCENTRE, CXF_ALIGNCENTRE);
                    DrawStrokeText(gc, x * Scale + midx, y * Scale + midy, token);
                    UpdateBoundingBox(&bbox, x, y); //??? calculate the bounding box of the text
                }
            }
        }

        /* draw the pads */
        wxBrush brushStd, brushRev;
        wxPen penStd, penRev;
        if (OutlineMode) {
            brushStd = *wxTRANSPARENT_BRUSH;
            brushRev = *wxTRANSPARENT_BRUSH;
            penStd.SetColour(clrPad);
            penStd.SetWidth(0.2 * Scale);
            penRev.SetColour(clrPadRev);
            penRev.SetWidth(0.2 * Scale);
        } else {
            brushStd.SetColour(clrPadFill);
            brushStd.SetStyle(wxBRUSHSTYLE_SOLID);
            brushRev.SetColour(clrPadRevFill);
            brushRev.SetStyle(wxBRUSHSTYLE_SOLID);
            penStd.SetColour(clrPad);
            penStd.SetWidth(1);
            penRev.SetColour(clrPadRev);
            penRev.SetWidth(1);
        }
        gc->SetBrush(brushStd);
        gc->SetPen(penStd);

        wxBrush brushHole;
        brushHole.SetColour(wxColour(224, 224, 0, transp[fp]));

        wxFont font(FontSizeLegend, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        font.SetPointSize(FontSizeLegend);
        gc->SetFont(font, clrHiddenText);

        bool inpad = false;
        double padx = 0, pady = 0, padwidth = 0, padheight = 0, padrot = 0;
        double paddeltax = 0, paddeltay = 0;
        double drillx = 0, drilly = 0, drillwidth = 0, drillheight = 0;
        double pasteratio = 0.0;
        double padrratio = 0.5;
        bool padsmd = false, padbottomside = false;
        wxPoint2DDouble pastepoints[5];
        wxString padpin, padshape;
        for (int idx = 0; idx < (int)PartData[fp].Count(); idx++) {
            wxString line = PartData[fp][idx];
            if (line[0] == wxT('$')) {
                if (line.CmpNoCase(wxT("$PAD")) == 0) {
                    inpad = true;
                    drillwidth = drillheight = 0;
                    pasteratio = 0.0;
                } else if (line.CmpNoCase(wxT("$EndPAD")) == 0) {
                    wxBrush *brushPad = padbottomside ? &brushRev : &brushStd;
                    wxPen *penPad = padbottomside ? &penRev : &penStd;
                    DrawPad(gc, midx, midy, padshape, padpin,
                            padx, pady, padwidth, padheight, padrratio,
                            paddeltax, paddeltay, pasteratio, padrot,
                            drillx, drilly, drillwidth, drillheight,
                            *penPad, *brushPad, brushHole, &bbox);
                    inpad = false;
                }
                continue;
            } else if (line[0] == wxT('(') && line.Left(4).Cmp(wxT("(pad")) == 0) {
                GetToken(&line);    /* ignore "(pad" */
                padpin = GetToken(&line);
                wxString tmp = GetToken(&line);
                padsmd = (tmp.CmpNoCase(wxT("smd")) == 0);
                padshape = GetToken(&line);
                padrot = 0;         /* preset pad rotation (frequently omitted) */
                wxString section = GetSection(line, wxT("at"));
                if (section.length() > 0) {
                    padx = GetTokenDim(&section, true);
                    pady = GetTokenDim(&section, true);
                    if (section.length() > 0)
                        padrot = NormalizeAngle(GetTokenDouble(&section) - module_angle);
                }
                section = GetSection(line, wxT("size"));
                if (section.length() > 0) {
                    padwidth = GetTokenDim(&section, true);
                    padheight = GetTokenDim(&section, true);
                }
                drillx = drilly = 0;    /* preset drill parameters (as these are optional) */
                drillwidth = drillheight = 0;
                section = GetSection(line, wxT("drill"));
                if (section.length() > 0) {
                    wxString sectoffset = GetSection(section, wxT("offset"));
                    if (section.Left(4).Cmp(wxT("oval"))==0) {
                        GetToken(&section);
                        drillwidth = GetTokenDim(&section, true);
                        drillheight = GetTokenDim(&section, true);
                    } else {
                        drillwidth = GetTokenDim(&section, true);
                    }
                    if (sectoffset.length() > 0) {
                        drillx = GetTokenDim(&sectoffset, true);
                        drilly = GetTokenDim(&sectoffset, true);
                    }
                }
                section = GetSection(line, wxT("solder_paste_margin_ratio"));
                if (section.length() > 0)
                    pasteratio = GetTokenDouble(&section);
                else
                    pasteratio = 0.0;
                if (padsmd) {
                    /* for SMD pads, check the side (for through-hole, don't
                       care because these go through all sides) */
                    section = GetSection(line, wxT("layers"));
                    if (section.length() > 0 && section.Find(wxT("B.Cu")) >= 0 && section.Find(wxT("F.Cu")) < 0)
                        padbottomside = true;
                }
                if (padshape.Cmp(wxT("trapezoid")) == 0) {
                    section = GetSection(line, wxT("rect_delta"));
                    if (section.length() > 0) {
                        paddeltax = GetTokenDim(&section, true);
                        paddeltay = GetTokenDim(&section, true);
                    }
                } else if (padshape.Cmp(wxT("roundrect")) == 0) {
                    section = GetSection(line, wxT("roundrect_rratio"));
                    if (section.length() > 0)
                        padrratio = GetTokenDouble(&section);
                }
                /* draw the pad */
                wxBrush *brushPad = padbottomside ? &brushRev : &brushStd;
                wxPen *penPad = padbottomside ? &penRev : &penStd;
                DrawPad(gc, midx, midy, padshape, padpin,
                        padx, pady, padwidth, padheight, padrratio,
                        paddeltax, paddeltay, pasteratio, padrot,
                        drillx, drilly, drillwidth, drillheight,
                        *penPad, *brushPad, brushHole, &bbox);
                continue;
            }
            if (!inpad)
                continue;
            wxString token = GetToken(&line);
            if (token.CmpNoCase(wxT("Po")) == 0) {
                padx = GetTokenDim(&line, unit_mm); /* this is relative to the footprint position, but in a footprint file, the position is always 0 */
                pady = GetTokenDim(&line, unit_mm);
            } else if (token.CmpNoCase(wxT("Sh")) == 0) {
                padpin = GetToken(&line);
                padshape = GetToken(&line);
                padwidth = GetTokenDim(&line, unit_mm);
                padheight = GetTokenDim(&line, unit_mm);
                paddeltax = GetTokenDim(&line, unit_mm);
                paddeltay = GetTokenDim(&line, unit_mm);
                padrot = NormalizeAngle(GetTokenLong(&line) / 10.0 - module_angle);
                padrratio = (line.Length() > 0) ? GetTokenDouble(&line) : 0;
                if (padshape == 'R' && padrratio > EPSILON)
                    padshape = 'D';
            } else if (token.CmpNoCase(wxT("At")) == 0) {
                token = GetToken(&line);
                padsmd = (token.CmpNoCase(wxT("SMD")) == 0);
                GetToken(&line);    /* ignore legacy field */
                token = GetToken(&line);
                long mask;
                if (token.ToLong(&mask, 16) && (mask & 0xffff) == 1 && padsmd)
                    padbottomside = true;
            } else if (token.CmpNoCase(wxT("Dr")) == 0) {
                drillwidth = GetTokenDim(&line, unit_mm);
                drillx = GetTokenDim(&line, unit_mm);   /* this is relative to the pad position */
                drilly = GetTokenDim(&line, unit_mm);
                token = GetToken(&line);
                if (token == wxT('O')) {
                    drillwidth = GetTokenDim(&line, unit_mm);
                    drillheight = GetTokenDim(&line, unit_mm);
                }
            } else if (token.CmpNoCase(wxT(".SolderPasteRatio")) == 0) {
                pasteratio = GetTokenDouble(&line);
            }
        }

        /* draw the measures */
        if (m_toolBar->GetToolToggled(IDT_MEASUREMENTS) && ShowMeasurements) {
            pen.SetColour(0, 192, 192);
            pen.SetWidth(1);
            gc->SetPen(pen);

            font.SetPointSize(FontSizeLegend);
            font.SetWeight(wxFONTWEIGHT_BOLD);
            gc->SetFont(font, wxColour(0, 192, 192));

            /* create a normalized pad size for the gap and extent, for the case that the primary
               pad is rotated */
            CoordPair NormPad = Footprint[fp].PadSize[0];
            if (Footprint[fp].PadRightAngle[0])
                NormPad.Set(NormPad.GetY(), NormPad.GetX());

            CoordPair padpins[2];
            if (Footprint[fp].Pitch > EPSILON) {
                /* decide whether to also draw the pad extent (only when both spans are invalid) */
                bool drawextent = Equal(Footprint[fp].SpanHor, 0) && Equal(Footprint[fp].SpanVer, 0);
                if (Footprint[fp].PitchVertical) {
                    /* decide whether to print the clearance between the pads */
                    int pos = Equal(Footprint[fp].PitchPins[0].GetX(), Footprint[fp].PitchPins[1].GetX()) ? 1 : 0;
                    /* decide where to put the pitch dimension: left or right (prefer left) */
                    int orientation = (Footprint[fp].PitchPins[0].GetX() > 0 && Footprint[fp].PitchPins[1].GetX() > 0) ? 270 : 90;
                    DrawDimension(gc, midx, midy, orientation, pos, Footprint[fp].Pitch,
                                  Footprint[fp].PitchPins, bbox);
                    if (pos == 1) {
                        if (Footprint[fp].PitchPins[0].GetY() < Footprint[fp].PitchPins[1].GetY()) {
                            padpins[0].Set(Footprint[fp].PitchPins[0].GetX(), Footprint[fp].PitchPins[0].GetY() + NormPad.GetY() / 2);
                            padpins[1].Set(Footprint[fp].PitchPins[1].GetX(), Footprint[fp].PitchPins[1].GetY() - NormPad.GetY() / 2);
                        } else {
                            padpins[1].Set(Footprint[fp].PitchPins[0].GetX(), Footprint[fp].PitchPins[0].GetY() + NormPad.GetY() / 2);
                            padpins[0].Set(Footprint[fp].PitchPins[1].GetX(), Footprint[fp].PitchPins[1].GetY() - NormPad.GetY() / 2);
                        }
                        DrawDimension(gc, midx, midy, orientation, 0,
                                      Footprint[fp].Pitch - NormPad.GetY(),
                                      padpins, bbox);
                    }
                    if (drawextent && pos > 0) {
                        if (Footprint[fp].PitchPins[0].GetY() < Footprint[fp].PitchPins[1].GetY()) {
                            padpins[0].Set(Footprint[fp].PitchPins[0].GetX(), Footprint[fp].PitchPins[0].GetY() - NormPad.GetY() / 2);
                            padpins[1].Set(Footprint[fp].PitchPins[1].GetX(), Footprint[fp].PitchPins[1].GetY() + NormPad.GetY() / 2);
                        } else {
                            padpins[1].Set(Footprint[fp].PitchPins[0].GetX(), Footprint[fp].PitchPins[0].GetY() - NormPad.GetY() / 2);
                            padpins[0].Set(Footprint[fp].PitchPins[1].GetX(), Footprint[fp].PitchPins[1].GetY() + NormPad.GetY() / 2);
                        }
                        DrawDimension(gc, midx, midy, orientation, pos + 1,
                                      Footprint[fp].Pitch + NormPad.GetY(),
                                      padpins, bbox);
                    }
                } else {
                    /* decide whether to print the clearance between the pads */
                    int pos = Equal(Footprint[fp].PitchPins[0].GetY(), Footprint[fp].PitchPins[1].GetY()) ? 1 : 0;
                    /* decide where to put the pitch dimension: top or bottom (prefer top) */
                    int orientation = (Footprint[fp].PitchPins[0].GetY() > 0 && Footprint[fp].PitchPins[1].GetY() > 0) ? 180 : 0;
                    DrawDimension(gc, midx, midy, orientation, pos, Footprint[fp].Pitch,
                                  Footprint[fp].PitchPins, bbox);
                    if (pos == 1) {
                        if (Footprint[fp].PitchPins[0].GetX() < Footprint[fp].PitchPins[1].GetX()) {
                            padpins[0].Set(Footprint[fp].PitchPins[0].GetX() + NormPad.GetX() / 2, Footprint[fp].PitchPins[0].GetY());
                            padpins[1].Set(Footprint[fp].PitchPins[1].GetX() - NormPad.GetX() / 2, Footprint[fp].PitchPins[1].GetY());
                        } else {
                            padpins[1].Set(Footprint[fp].PitchPins[0].GetX() + NormPad.GetX() / 2, Footprint[fp].PitchPins[0].GetY());
                            padpins[0].Set(Footprint[fp].PitchPins[1].GetX() - NormPad.GetX() / 2, Footprint[fp].PitchPins[1].GetY());
                        }
                        DrawDimension(gc, midx, midy, orientation, 0,
                                      Footprint[fp].Pitch - NormPad.GetX(),
                                      padpins, bbox);
                    }
                    if (drawextent && pos > 0) {
                        if (Footprint[fp].PitchPins[0].GetX() < Footprint[fp].PitchPins[1].GetX()) {
                            padpins[0].Set(Footprint[fp].PitchPins[0].GetX() - NormPad.GetX() / 2, Footprint[fp].PitchPins[0].GetY());
                            padpins[1].Set(Footprint[fp].PitchPins[1].GetX() + NormPad.GetX() / 2, Footprint[fp].PitchPins[1].GetY());
                        } else {
                            padpins[1].Set(Footprint[fp].PitchPins[0].GetX() - NormPad.GetX() / 2, Footprint[fp].PitchPins[0].GetY());
                            padpins[0].Set(Footprint[fp].PitchPins[1].GetX() + NormPad.GetX() / 2, Footprint[fp].PitchPins[1].GetY());
                        }
                        DrawDimension(gc, midx, midy, orientation, pos + 1,
                                      Footprint[fp].Pitch + NormPad.GetX(),
                                      padpins, bbox);
                    }
                }
            }
            /* check whether to print either span, or both */
            bool DrawVerSpan = true;
            bool DrawHorSpan = true;
            if (Equal(Footprint[fp].SpanHor, Footprint[fp].SpanVer)) {
                /* both are equal, draw one of the two (at most) */
                if (Footprint[fp].PitchVertical)
                    DrawVerSpan = false;    /* pitch is vertical, so draw span horizontal */
                else
                    DrawHorSpan = false;    /* pitch is horizontal, so draw span vertical */
            }
            if (DrawVerSpan && Footprint[fp].SpanVer > EPSILON) {
                /* decide where to put the vertical span: left or right (prefer right) */
                int orientation = (Footprint[fp].SpanVerPins[0].GetX() < 0 && Footprint[fp].SpanVerPins[1].GetX() < 0) ? 90 : 270;
                int stackpos = (Footprint[fp].SpanVer - NormPad.GetY() > 0.1) ? 1 : 0;
                DrawDimension(gc, midx, midy, orientation, stackpos, Footprint[fp].SpanVer,
                              Footprint[fp].SpanVerPins, bbox);
                /* draw the gap */
                if (stackpos > 0) {
                    if (Footprint[fp].SpanVerPins[0].GetY() < Footprint[fp].SpanVerPins[1].GetY()) {
                        padpins[0].Set(Footprint[fp].SpanVerPins[0].GetX(), Footprint[fp].SpanVerPins[0].GetY() + NormPad.GetY() / 2);
                        padpins[1].Set(Footprint[fp].SpanVerPins[1].GetX(), Footprint[fp].SpanVerPins[1].GetY() - NormPad.GetY() / 2);
                    } else {
                        padpins[1].Set(Footprint[fp].SpanVerPins[0].GetX(), Footprint[fp].SpanVerPins[0].GetY() + NormPad.GetY() / 2);
                        padpins[0].Set(Footprint[fp].SpanVerPins[1].GetX(), Footprint[fp].SpanVerPins[1].GetY() - NormPad.GetY() / 2);
                    }
                    DrawDimension(gc, midx, midy, orientation, 0,
                                  Footprint[fp].SpanVer - NormPad.GetY(),
                                  padpins, bbox);
                }
                /* draw the extent */
                if (Footprint[fp].SpanVerPins[0].GetY() < Footprint[fp].SpanVerPins[1].GetY()) {
                    padpins[0].Set(Footprint[fp].SpanVerPins[0].GetX(), Footprint[fp].SpanVerPins[0].GetY() - NormPad.GetY() / 2);
                    padpins[1].Set(Footprint[fp].SpanVerPins[1].GetX(), Footprint[fp].SpanVerPins[1].GetY() + NormPad.GetY() / 2);
                } else {
                    padpins[1].Set(Footprint[fp].SpanVerPins[0].GetX(), Footprint[fp].SpanVerPins[0].GetY() - NormPad.GetY() / 2);
                    padpins[0].Set(Footprint[fp].SpanVerPins[1].GetX(), Footprint[fp].SpanVerPins[1].GetY() + NormPad.GetY() / 2);
                }
                DrawDimension(gc, midx, midy, orientation, stackpos + 1,
                              Footprint[fp].SpanVer + NormPad.GetY(),
                              padpins, bbox);
            }
            if (DrawHorSpan && Footprint[fp].SpanHor > EPSILON) {
                /* decide where to put the horizontal span: top or bottom (prefer bottom) */
                int orientation = (Footprint[fp].SpanHorPins[0].GetY() < 0 && Footprint[fp].SpanHorPins[1].GetY() < 0) ? 0 : 180;
                int stackpos = (Footprint[fp].SpanHor - NormPad.GetX() > 0.1) ? 1 : 0;
                DrawDimension(gc, midx, midy, orientation, stackpos, Footprint[fp].SpanHor,
                              Footprint[fp].SpanHorPins, bbox);
                /* draw the gap */
                if (stackpos > 0) {
                    if (Footprint[fp].SpanHorPins[0].GetX() < Footprint[fp].SpanHorPins[1].GetX()) {
                        padpins[0].Set(Footprint[fp].SpanHorPins[0].GetX() + NormPad.GetX() / 2, Footprint[fp].SpanHorPins[0].GetY());
                        padpins[1].Set(Footprint[fp].SpanHorPins[1].GetX() - NormPad.GetX() / 2, Footprint[fp].SpanHorPins[1].GetY());
                    } else {
                        padpins[1].Set(Footprint[fp].SpanHorPins[0].GetX() + NormPad.GetX() / 2, Footprint[fp].SpanHorPins[0].GetY());
                        padpins[0].Set(Footprint[fp].SpanHorPins[1].GetX() - NormPad.GetX() / 2, Footprint[fp].SpanHorPins[1].GetY());
                    }
                    DrawDimension(gc, midx, midy, orientation, 0,
                                  Footprint[fp].SpanHor - NormPad.GetX(),
                                  padpins, bbox);
                }
                /* draw the extent */
                if (Footprint[fp].SpanHorPins[0].GetX() < Footprint[fp].SpanHorPins[1].GetX()) {
                    padpins[0].Set(Footprint[fp].SpanHorPins[0].GetX() - NormPad.GetX() / 2, Footprint[fp].SpanHorPins[0].GetY());
                    padpins[1].Set(Footprint[fp].SpanHorPins[1].GetX() + NormPad.GetX() / 2, Footprint[fp].SpanHorPins[1].GetY());
                } else {
                    padpins[1].Set(Footprint[fp].SpanHorPins[0].GetX() - NormPad.GetX() / 2, Footprint[fp].SpanHorPins[0].GetY());
                    padpins[0].Set(Footprint[fp].SpanHorPins[1].GetX() + NormPad.GetX() / 2, Footprint[fp].SpanHorPins[1].GetY());
                }
                DrawDimension(gc, midx, midy, orientation, stackpos + 1,
                              Footprint[fp].SpanHor + NormPad.GetX(),
                              padpins, bbox);
            }

        } /* if (draw measures) */
    } /* for (fp) */

    /* draw cross for the centre point */
    if (DrawCentreCross) {
        pen.SetColour(192, 192, 192);
        pen.SetWidth(1);
        gc->SetPen(pen);
        #define CROSS_SIZE 12
        points[0].m_x = midx - CROSS_SIZE;
        points[0].m_y = midy;
        points[1].m_x = midx + CROSS_SIZE;
        points[1].m_y = midy;
        gc->DrawLines(2, points);
        points[0].m_x = midx;
        points[0].m_y = midy - CROSS_SIZE;
        points[1].m_x = midx;
        points[1].m_y = midy + CROSS_SIZE;
        gc->DrawLines(2, points);
    }
#if 0 //??? only for testing bounding box
    pen.SetColour(192, 0, 192);
    pen.SetWidth(1);
    gc->SetPen(pen);
    gc->SetBrush(*wxTRANSPARENT_BRUSH);
    gc->DrawRectangle(bbox.GetX() * Scale + midx, bbox.GetY() * Scale + midy,
                      bbox.GetWidth() * Scale, bbox.GetHeight() * Scale);
#endif
}

void libmngrFrame::ResizeModelViewport()
{
#if !defined NO_3DMODEL
    wxASSERT(glCanvas && glContext);

    wxSize sz = m_panelView->GetSize();
    glViewport(0, 0, sz.x, sz.y);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    CyberX3D::ViewpointNode *view = sceneGraph.getViewpointNode();
    if (view == NULL)
        view = sceneGraph.getDefaultViewpointNode();

    double fov = (view->getFieldOfView() / 3.14f) * 180.0f * (SCALE_DEFAULT / Scale);   /* a smaller FOV zooms in */
    double aspect = (double)sz.x / sz.y;
    const double zNear = 0.1;
    const double zFar = 10000.0;
    const double pi = 3.1415926535897932384626433832795;

    /* Calculate the distance from 0 of the y clipping plane. Basically trig to calculate position
       of clipper at zNear.
       Note: tan( double ) uses radians but OpenGL works in degrees so we convert degrees to radians
       by dividing by 360 then multiplying by pi.
       Formula below corrected by Carsten Jurenz:
            fH = tan( (fov / 2) / 180 * pi ) * zNear;
       Which can be reduced to:
            fH = tan( fov / 360 * pi ) * zNear;
    */
    double fH = tan( fov / 360 * pi ) * zNear;
    /* Calculate the distance from 0 of the x clipping plane based on the aspect ratio. */
    double fW = fH * aspect;
    /* Finally call glFrustum, this is all gluPerspective does anyway! This is why we calculate
       half the distance between the clipping planes - glFrustum takes an offset from zero for
       each clipping planes distance. (Saves 2 divides) */
    glFrustum(-fW, fW, -fH, fH, zNear, zFar);

    glMatrixMode(GL_MODELVIEW);
#endif
}

void libmngrFrame::DrawModels(float xangle, float yangle)
{
#if !defined NO_3DMODEL
    if (glCanvas == NULL) {
        int attribList[] = { WX_GL_RGBA,
                             WX_GL_DOUBLEBUFFER,
                             #if defined _WIN32
                               WX_GL_SAMPLE_BUFFERS, GL_TRUE, // Multi-sampling (apparently not currently supported on Linux)
                             #endif
                             WX_GL_DEPTH_SIZE, 16,
                             0, 0 };
        wxSize sz = m_panelView->GetSize();
        glCanvas = new wxGLCanvas(m_panelView, wxID_ANY, attribList, m_panelView->GetPosition(), m_panelView->GetSize(),
                                  m_panelView->GetWindowStyleFlag() | wxTRANSPARENT_WINDOW, wxT("PartCanvas"));
        #if defined _WIN32
            wxBoxSizer *pSizer = dynamic_cast<wxBoxSizer*>(m_panelView->GetSizer());
            if (!pSizer) {
                pSizer = new wxBoxSizer(wxVERTICAL);
                m_panelView->SetSizer(pSizer);
            }
            pSizer->Add(glCanvas, 1, wxGROW|wxALL, 0);
        #else
            /* for unknown reasons, without a re-parent, the mouse interaction
               on glCanvas (on a panel) will not work on GTK */ //??? check whether this still is the case
            glCanvas->Reparent(m_panelView->GetParent());
            glCanvas->SetSize(sz);
            // For the model to appear, slightly enlarge the main window, or hide and then re-show the side panel with the parameters.
        #endif
        glCanvas->Show();
        glCanvas->Refresh(true);

        glCanvas->Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(libmngrFrame::OnViewCentre), NULL, this);
        glCanvas->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(libmngrFrame::OnViewStartDrag), NULL, this);
        glCanvas->Connect(wxEVT_MOTION, wxMouseEventHandler(libmngrFrame::OnViewDrag), NULL, this);
    }
    if (glContext == NULL) {
        glContext = new wxGLContext(glCanvas);
        glContext->SetCurrent(*glCanvas);
        ResizeModelViewport();
    }

    SetSceneGraphBackground(clr3DModelMode.GetRGB());
    DrawSceneGraph(&sceneGraph, OGL_RENDERING_TEXTURE, xangle - 30.0, yangle);
    glCanvas->SwapBuffers();
#endif
}

void libmngrFrame::DrawDimension(wxGraphicsContext *gc, int midx, int midy,
                                 int orientation, int stack, double value,
                                 const CoordPair pins[2], const CoordSize& bbox)
{
    wxASSERT(orientation == 0 || orientation == 90 || orientation == 180 || orientation == 270);
    bool horz = (orientation == 90 || orientation == 270);
    int sign = (orientation == 180 || orientation == 270) ? 1 : -1;

    /* calculate the text size first (needed for the offset for stacking dimensions) */
    wxString text = wxString::Format(wxT("%.2f (%.1f)"), value, value / 0.0254);
    wxDouble tw, th, td, tex;
    gc->GetTextExtent(text, &tw, &th, &td, &tex);


    wxPoint2DDouble points[2];
    if (horz) {
        double pos = (sign > 0) ? bbox.GetRight() : bbox.GetLeft();
        double xt = pos * Scale + midx + sign * (DimensionOffset + stack * th * MEASURE_GAP);
        for (int idx = 0; idx < 2; idx++) {
            points[0].m_x = pins[idx].GetX() * Scale + midx + sign * LINE_OFFSET;
            points[1].m_x = xt + sign * LINE_OFFSET;
            points[0].m_y = points[1].m_y = pins[idx].GetY() * Scale + midy;
            gc->DrawLines(2, points);
        }

        points[0].m_x = points[1].m_x = xt;
        points[0].m_y = pins[0].GetY() * Scale + midy;
        points[1].m_y = pins[1].GetY() * Scale + midy;
        gc->DrawLines(2, points);

        double yt = (pins[0].GetY() + pins[1].GetY()) / 2 * Scale + midy;
        gc->DrawText(text, (sign > 0) ? xt + LINE_OFFSET : xt - th - LINE_OFFSET, yt + tw/2, M_PI / 2);
    } else {
        double pos = (sign > 0) ? bbox.GetBottom() : bbox.GetTop();
        double yt = pos * Scale + midy + sign * (DimensionOffset + stack * th * MEASURE_GAP);
        for (int idx = 0; idx < 2; idx++) {
            points[0].m_x = points[1].m_x = pins[idx].GetX() * Scale + midx;
            points[0].m_y = pins[idx].GetY() * Scale + midy + sign * LINE_OFFSET;
            points[1].m_y = yt + sign * LINE_OFFSET;
            gc->DrawLines(2, points);
        }

        points[0].m_x = pins[0].GetX() * Scale + midx;
        points[1].m_x = pins[1].GetX() * Scale + midx;
        points[0].m_y = points[1].m_y = yt;
        gc->DrawLines(2, points);

        double xt = (pins[0].GetX() + pins[1].GetX()) / 2 * Scale + midx;
        gc->DrawText(text, xt - tw/2, (sign > 0) ? yt + LINE_OFFSET : yt - th - LINE_OFFSET);
    }
}

void libmngrFrame::OnPaintViewport(wxPaintEvent& /*event*/)
{
    wxPaintDC dc(m_panelView);
    wxGraphicsContext *gc = wxGraphicsContext::Create(dc);
    if (PartData[0].Count() == 0 && PartData[1].Count() == 0) {
        wxColour clrText = wxColour(128, 128, 128);
        wxFont font(24, wxFONTFAMILY_SWISS, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        font.SetPointSize(24);
        gc->SetFont(font, clrText);
        wxString text = SymbolMode ? wxT("NO SYMBOL TO DISPLAY") : (ModelMode ? wxT("NO MODEL TO DISPLAY") : wxT("NO FOOTPRINT TO DISPLAY"));
        wxSize sz = m_panelView->GetClientSize();
        wxDouble tw, th, td, tex;
        gc->GetTextExtent(text, &tw, &th, &td, &tex);
        gc->DrawText(text, sz.GetWidth() / 2 + OffsetX - tw/2, sz.GetHeight() / 2 + OffsetY - th/2);
        return;
    }

    int transp[2] = { wxALPHA_OPAQUE, 0x80 };   /* this is the transparency for the overlay */
    if (!CompareMode || !m_toolBar->GetToolToggled(IDT_LEFTFOOTPRINT))
        transp[1] = wxALPHA_OPAQUE;

    wxSize sz = m_panelView->GetClientSize();
    long midx = sz.GetWidth() / 2 + OffsetX;
    long midy = sz.GetHeight() / 2 + OffsetY;

    if (SymbolMode)
        DrawSymbols(gc, midx, midy, transp);
    else if (ModelMode)
        DrawModels(OffsetY / 5.0, OffsetX / 5.0);
    else
        DrawFootprints(gc, midx, midy, transp);
}

void libmngrFrame::OnSizeViewport(wxSizeEvent& /*event*/)
{
    if (ModelMode) {
        #if !defined NO_3DMODEL
            wxASSERT(glCanvas && glContext);
        #endif
        ResizeModelViewport();
    }
    m_panelView->Refresh();
    /* move the filter edit field (if it exists already) */
    if (m_statusBar->GetFieldsCount() > 1) {
        wxRect rect;
        m_statusBar->GetFieldRect(1, rect);
        m_editFilter->SetSize(rect.GetLeft(), rect.GetTop(), rect.GetWidth(), rect.GetHeight(), 0);
    }
}

void libmngrFrame::OnZoomIn(wxCommandEvent& /*event*/)
{
    if (Scale < SCALE_MAX) {
        Scale *= SCALE_FACTOR;
        if (ModelMode)
            ResizeModelViewport();
        m_panelView->Refresh();
    } else {
        Scale = SCALE_MAX;
    }
}

void libmngrFrame::OnZoomOut(wxCommandEvent& /*event*/)
{
    if (Scale > SCALE_MIN) {
        Scale /= SCALE_FACTOR;
        if (ModelMode)
            ResizeModelViewport();
        m_panelView->Refresh();
    } else {
        Scale = SCALE_MIN;
    }
}

void libmngrFrame::On3DView(wxCommandEvent& /*event*/)
{
#if !defined NO_3DMODEL
    /* not active in symbol mode */
    if (SymbolMode) {
        wxMessageBox(wxT("3D view is only relevant in footprint mode."));
        m_toolBar->ToggleTool(IDT_3DVIEW, false);
        return;
    }

    /* turn off compare mode (if needed) */
    if (CompareMode) {
        CompareMode = false;
        m_toolBar->EnableTool(IDT_LEFTFOOTPRINT, CompareMode);
        m_toolBar->EnableTool(IDT_RIGHTFOOTPRINT, CompareMode);
        m_toolBar->ToggleTool(IDT_LEFTFOOTPRINT, CompareMode);
        m_toolBar->ToggleTool(IDT_RIGHTFOOTPRINT, CompareMode);
        m_toolBar->Refresh();
        m_radioViewLeft->Enable(CompareMode);
        m_radioViewRight->Enable(CompareMode);
        m_radioViewLeft->SetValue(true);
        /* remove the selection in the right listctrl (we only keep the left footprint) */
        RemoveSelection(m_listModulesRight, &SelectedPartRight);
        PartData[1].Clear();
        /* check whether there is a selection in the left listctrl, only enable
           the buttons if so */
        long idx = m_listModulesRight->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (idx >= 0)
            EnableButtons(LEFTPANEL);
        m_radioViewLeft->SetValue(true);
        m_radioViewRight->SetValue(false);
    }

    ModelMode = m_toolBar->GetToolToggled(IDT_3DVIEW);
    if (ModelMode) {
        ScaleNormal = Scale;    /* save the current value */
        Scale = SCALE_DEFAULT;
        m_panelView->SetBackgroundColour(clr3DModelMode);
    } else {
        m_panelView->SetBackgroundColour(clrFootprintMode);
        sceneGraph.clear();
        if (glContext != NULL)
            delete glContext;
        if (glCanvas != NULL) {
            glCanvas->Disconnect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(libmngrFrame::OnViewCentre), NULL, this);
            glCanvas->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(libmngrFrame::OnViewStartDrag), NULL, this);
            glCanvas->Disconnect(wxEVT_MOTION, wxMouseEventHandler(libmngrFrame::OnViewDrag), NULL, this);
            delete glCanvas;
        }
        glCanvas = NULL;
        glContext = NULL;
        Scale = ScaleNormal;    /* restore previous scale value */
    }

    OffsetX = OffsetY = 0;
    UpdateDetails(0);       /* update details for the left component */
    if (ModelMode) {
        sceneGraph.clear();
        DrawModels(0, 0);   /* only to create the canvas and the context, this is needed for loading the VRML file as well */
        Update3DModel(PartData[0]);
    }
    m_panelView->Refresh();
#endif
}

void libmngrFrame::OnShowMeasurements(wxCommandEvent& /*event*/)
{
    m_panelView->Refresh();
}

void libmngrFrame::OnShowDetails(wxCommandEvent& /*event*/)
{
    bool details = m_toolBar->GetToolToggled(IDT_DETAILSPANEL);
    m_menubar->Check(IDM_DETAILSPANEL, details);
    ToggleDetailsPanel(details);
}

void libmngrFrame::OnShowLabels(wxCommandEvent& /*event*/)
{
    m_panelView->Refresh();
}

void libmngrFrame::OnShowLeftFootprint(wxCommandEvent& /*event*/)
{
    m_panelView->Refresh();
}

void libmngrFrame::OnShowRightFootprint(wxCommandEvent& /*event*/)
{
    m_panelView->Refresh();
}

void libmngrFrame::OnShowLeftDetails(wxCommandEvent& /*event*/)
{
    UpdateDetails(0);
}

void libmngrFrame::OnShowRightDetails(wxCommandEvent& /*event*/)
{
    UpdateDetails(1);
}

void libmngrFrame::OnUnitSelect(wxSpinEvent& /*event*/)
{
    int fp = 0;
    if (CompareMode && m_toolBar->GetToolToggled(IDT_RIGHTFOOTPRINT))
        fp = 1;
    SymbolUnitNumber[fp] = m_spinUnitSelect->GetValue();
    m_panelView->Refresh();
}

void libmngrFrame::ToggleDetailsPanel(bool onoff)
{
    if (onoff && !m_splitterViewPanel->IsSplit()) {
        m_panelSettings->Show();
        m_splitterViewPanel->SplitVertically(m_panelView, m_panelSettings, -PANEL_WIDTH);
    } else if (!onoff && m_splitterViewPanel->IsSplit()) {
        m_splitterViewPanel->Unsplit(); /* this implicitly hides the panel at the right */
    }
}

void libmngrFrame::EnableButtons(int side)
{
    bool valid = false;
    int idx;

    switch (side) {
    case LEFTPANEL:
        idx = m_choiceModuleRight->GetCurrentSelection();
        if (idx >= 0 && idx < (int)m_choiceModuleRight->GetCount()) {
            wxString target = m_choiceModuleRight->GetString(idx);
            valid = (target[0] != '(') || target.CmpNoCase(LIB_REPOS) == 0;
            idx = m_choiceModuleLeft->GetCurrentSelection();
            if (idx >= 0) {
                wxString source = m_choiceModuleLeft->GetString(idx);
                if (source.CmpNoCase(target) == 0)
                    valid = false;
            }
        }
        m_btnMove->Enable(valid);
        m_btnCopy->Enable(valid);
        m_btnDelete->Enable(true);
        m_btnRename->Enable(true);
        m_btnDuplicate->Enable(true);
        m_btnMove->SetLabel(wxT("&Move >>>"));
        m_btnCopy->SetLabel(wxT("&Copy >>>"));
        break;
    case 0:
        m_btnMove->Enable(false);
        m_btnCopy->Enable(false);
        m_btnDelete->Enable(false);
        m_btnRename->Enable(false);
        m_btnDuplicate->Enable(false);
        m_btnMove->SetLabel(wxT("&Move"));
        m_btnCopy->SetLabel(wxT("&Copy"));
        break;
    case RIGHTPANEL:
        idx = m_choiceModuleLeft->GetCurrentSelection();
        if (idx >= 0 && idx < (int)m_choiceModuleLeft->GetCount()) {
            wxString target = m_choiceModuleLeft->GetString(idx);
            valid = (target[0] != '(') || target.CmpNoCase(LIB_REPOS) == 0;
            idx = m_choiceModuleRight->GetCurrentSelection();
            if (idx >= 0) {
                wxString source = m_choiceModuleRight->GetString(idx);
                if (source.CmpNoCase(target) == 0)
                    valid = false;
            }
        }
        m_btnMove->Enable(valid);
        m_btnCopy->Enable(valid);
        m_btnDelete->Enable(true);
        m_btnRename->Enable(true);
        m_btnDuplicate->Enable(true);
        m_btnMove->SetLabel(wxT("<<< &Move"));
        m_btnCopy->SetLabel(wxT("<<< &Copy"));
        break;
    default:
        wxASSERT(false);
    }
}

/** GetSelection() returns the symbol/footprint name of the first (and only)
 *  selected item in the given list control.
 *
 *  The library path is optionally returned (if both "choice" and "library" are
 *  passed in). The "author" name is only returned when the library path is also
 *  returned (and when the parameter is not NULL).
 *
 *  If no item is selected in the list control, the function returns
 *  wxEmptyString and the library path and author name are not altered.
 */
wxString libmngrFrame::GetSelection(wxListCtrl* list, wxChoice* choice, wxString* library, wxString* author)
{
    /* returns only the first selected item */
    long row = list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row == -1)
        return wxEmptyString;
    wxString symbol = list->GetItemText(row);
    if (library) {
        wxASSERT(choice);   /* choice control must be known to retrieve the library */
        long idx = choice->GetCurrentSelection();
        wxASSERT(idx >= 0 && idx < (long)choice->GetCount());
        wxString source = choice->GetString(idx);
        if (source.CmpNoCase(LIB_REPOS) == 0) {
            *library = source;
            if (author) {
                wxListItem item;
                item.SetId(row);
                item.SetColumn(1);
                item.SetMask(wxLIST_MASK_TEXT);
                wxCHECK(list->GetItem(item), wxEmptyString);
                *author = item.GetText();
            }
        } else {
            wxListItem item;
            item.SetId(row);
            item.SetColumn(2);
            item.SetMask(wxLIST_MASK_TEXT);
            wxCHECK(list->GetItem(item), wxEmptyString);
            *library = item.GetText();
            if (author)
                *author = wxEmptyString;
        }
    }
    return symbol;
}

bool libmngrFrame::RemoveSelection(wxListCtrl* list, int* index)
{
    int count = 0;
    long idx = -1;
    for (;;) {
        idx = list->GetNextItem(idx, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (idx == -1)
            break;
        #if defined _WIN32
            list->SetItemState(idx, 0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        #else
            list->SetItemState(idx, 0, wxLIST_STATE_SELECTED);
        #endif
        count++;
    }
    wxASSERT(index != NULL);
    *index = -1;
    return (count > 0); /* return true if a selection was indeed removed */
}

static int CompareStringNoCase(const wxString& first, const wxString& second)
{
    return first.CmpNoCase(second);
}

void libmngrFrame::CollectLibraries(const wxString &path, wxArrayString *list)
{
    wxDir dir(path);
    if (!dir.IsOpened())
        return; /* error message already given */
    if (SymbolMode) {
        dir.GetAllFiles(path, list, wxT("*.lib"), wxDIR_FILES);
        dir.GetAllFiles(path, list, wxT("*.sym"), wxDIR_FILES);
    } else {
        dir.GetAllFiles(path, list, wxT("*.mod"), wxDIR_FILES);
        dir.GetAllFiles(path, list, wxT("*.emp"), wxDIR_FILES);
        dir.GetAllFiles(path, list, wxT("*.kicad_mod"), wxDIR_FILES);
        /* collect s-expression libraries separately, because GetAllFiles() does not
             return directories */
        wxString basename;
        bool cont = dir.GetFirst(&basename, wxEmptyString, wxDIR_FILES | wxDIR_DIRS);
        while (cont) {
            wxFileName name(basename);
            if (name.GetExt().CmpNoCase(wxT("pretty")) == 0)
                list->Add(path + wxT(DIRSEP_STR) + basename);
            cont = dir.GetNext(&basename);
        }
    }
}

class wxDirTraverserTree : public wxDirTraverser
{
public:
    wxDirTraverserTree(wxArrayString* pathlist) : m_pathlist(pathlist) { }

    virtual wxDirTraverseResult OnFile(const wxString& /*filename*/) wxOVERRIDE {
        return wxDIR_CONTINUE;
    }
    virtual wxDirTraverseResult OnDir(const wxString& dirname) wxOVERRIDE {
        m_pathlist->Add(dirname);
        return wxDIR_CONTINUE;
    }
private:
    wxArrayString* m_pathlist;
    wxDECLARE_NO_COPY_CLASS(wxDirTraverserTree);
};

void libmngrFrame::CollectAllLibraries(bool eraselists)
{
    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif

    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());

    bool recurse = false;
    config->Read(wxT("path/recurse"), &recurse);

    /* get the list of paths (straightforward without recursion) */
    wxArrayString pathlist;
    wxString path;
    wxString key;
    unsigned idx = 1;
    for ( ;; ) {
        if (SymbolMode)
            key = wxString::Format(wxT("paths/symbols%d"), idx);
        else
            key = wxString::Format(wxT("paths/footprints%d"), idx);
        if (!config->Read(key, &path))
            break;
        pathlist.Add(path);
        if (recurse) {
            wxDir dir(path);
            if (dir.IsOpened()) {
                wxDirTraverserTree traverser(&pathlist);
                dir.Traverse(traverser, wxEmptyString,  wxDIR_DIRS);
            }
        }
        idx++;
    }
    delete config;

    /* get the library list */
    wxArrayString liblist;
    for (idx = 0; idx < pathlist.Count(); idx++)
        CollectLibraries(pathlist[idx], &liblist);

    liblist.Sort(CompareStringNoCase);
    m_choiceModuleLeft->Clear();
    m_choiceModuleLeft->Append(LIB_NONE);
    if (liblist.Count() > 0)
        m_choiceModuleLeft->Append(LIB_ALL);
    #if !defined NO_CURL
        m_choiceModuleLeft->Append(LIB_REPOS);
    #endif
    if (liblist.Count() > 0)
        m_choiceModuleLeft->Append(liblist);
    m_choiceModuleLeft->SetSelection(0);
    m_choiceModuleRight->Clear();
    m_choiceModuleRight->Append(LIB_NONE);
    if (liblist.Count() > 0)
        m_choiceModuleRight->Append(LIB_ALL);
    #if !defined NO_CURL
        m_choiceModuleRight->Append(LIB_REPOS);
    #endif
    if (liblist.Count() > 0)
        m_choiceModuleRight->Append(liblist);
    m_choiceModuleRight->SetSelection(0);

    if (eraselists) {
        m_listModulesLeft->DeleteAllItems();
        m_listModulesRight->DeleteAllItems();
        PartData[0].Clear();
        PartData[1].Clear();
        #if !defined NO_3DMODEL
            sceneGraph.clear();
        #endif
    }

    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif
}

void libmngrFrame::WarnNoRepository(wxChoice* choice)
{
    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif

    int idx = choice->GetCurrentSelection();
    if (idx < 0 || idx >= (int)choice->GetCount())
        return;
    wxString path = choice->GetString(idx);
    wxASSERT(path.length() > 0);
    if (path.CmpNoCase(LIB_REPOS) != 0)
        return;
    /* so the repository is selected, check whether it was defined */
    #if defined NO_CURL
        wxMessageBox(wxT("The repository functions are disabled in this version."));
        choice->SetSelection(0);
    #else
        wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
        path = config->Read(wxT("repository/url"));
        delete config;
        if (path.Length() == 0) {
            wxMessageBox(wxT("No repository is configured.\nPlease configure a repository before using it."));
            choice->SetSelection(0);
        }
    #endif
}

/* CompareNames() compares two strings with an algorithm that compares sequences
 * of digits separately from sequences of other characters; so that SOIC8 comes
 * before SOIC10
 */
int libmngrFrame::CompareNames(const wxString& name1, const wxString& name2)
{
    int cmp = 0;
    unsigned i1 = 0, i2 = 0;
    while (i1 < name1.length() && i2 < name2.length() && cmp == 0) {
        if (isdigit(name1[i1]) && isdigit(name2[i2])) {
            unsigned l1, l2;
            for (l1 = i1; l1 < name1.length() && (isdigit(name1[l1]) || name1[l1] == '.'); l1++)
                /* nothing */;
            for (l2 = i2; l2 < name2.length() && (isdigit(name2[l2]) || name2[l2] == '.'); l2++)
                /* nothing */;
            double v1, v2;
            name1.Mid(i1, l1 - i1).ToDouble(&v1);
            name2.Mid(i2, l2 - i2).ToDouble(&v2);
            cmp = (long)((v2 -  v1) * 1000);
            i1 = l1;
            i2 = l2;
        } else {
            cmp = toupper(name2[i2]) - toupper(name1[i1]);
            i1++;
            i2++;
        }
    }
    if (cmp == 0) {
        /* strings match up to the shortest length, check whether one of the two is longer */
        if (i1 < name1.length())
            cmp--;
        if (i2 < name2.length())
            cmp++;
    }
    return cmp;
}

long libmngrFrame::GetListPosition(const wxString &name, const wxListCtrl* list)
{
    long low, high;
    low = 0;
    high = list->GetItemCount() - 1;
    while (low <= high) {
        long mid = (low + high) / 2;
        wxString item = list->GetItemText(mid);
        /* use a comparison algorithm that compares sequences of digits
           separately from sequences of other characters; so that SOIC8
           comes before SOIC10 */
        int cmp = CompareNames(name, item);
        if (cmp >= 0)
            high = mid - 1;
        else
            low = mid + 1;
    }
    return low;
}

/* HandleLibrarySelect() updates the module list for the given side (in "choice" and "list).
 * The "side" parameter also indicates which side is updated, but it can also be set to
 * BOTHPANELS; this parameter determines how to handle compare mode and whether to keep
 * the current selections in the module lists.
 */
void libmngrFrame::HandleLibrarySelect(wxChoice* choice, wxListCtrl* list, int side)
{
    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif
    CheckSavePart();

    wxASSERT(side >= LEFTPANEL && side <= RIGHTPANEL);  /* -1 = left, 1 = right, 0 = both */
    int idx = choice->GetCurrentSelection();
    if (idx < 0 || idx >= (int)choice->GetCount())
        return;

    FieldEdited = false;
    PartEdited = false;

    /* make sure to clear any component in the viewport */
    if (CompareMode) {
        if (side == BOTHPANELS || side == RIGHTPANEL) {
            PartData[1].Clear();
            UpdateDetails(1);
        } else {
            PartData[0].Clear();
            UpdateDetails(0);
        }
    } else {
        PartData[0].Clear();
        UpdateDetails(0);
    }
    #if !defined NO_3DMODEL
        sceneGraph.clear();
    #endif

    if (side == LEFTPANEL || side == BOTHPANELS) {
        if (RemoveSelection(m_listModulesLeft, &SelectedPartLeft))
            EnableButtons(0);
    }
    if (side == RIGHTPANEL || side == BOTHPANELS) {
        if (RemoveSelection(m_listModulesRight, &SelectedPartRight))
            EnableButtons(0);
    }

    list->DeleteAllItems();
    wxString filename = choice->GetString(idx);
    wxASSERT(filename.length() > 0);
    if (filename.CmpNoCase(LIB_NONE) == 0)
        return;

    wxString filter = wxEmptyString;
    if (m_editFilter->IsShown()) {
        filter = m_editFilter->GetValue();
        filter.Trim(false);
        filter.Trim(true);
        filter.MakeLower();
    }

    wxString title = SymbolMode ? wxT("Collecting symbols") : wxT("Collecting footprints");
    wxString msg = (filter.Length() > 0) ? wxT("Filtering library, please wait...") : wxT("Please wait...");
    wxProgressDialog* progress = new wxProgressDialog(title, msg, 100, this, wxPD_APP_MODAL|wxPD_AUTO_HIDE|wxPD_ELAPSED_TIME|wxPD_ESTIMATED_TIME|wxPD_REMAINING_TIME);

    if (filename.CmpNoCase(LIB_ALL) == 0) {
        progress->SetRange(choice->GetCount() - 1);
        for (idx = 0; idx < (int)choice->GetCount(); idx++) {
            wxString filename = choice->GetString(idx);
            wxASSERT(filename.length() > 0);
            if (filename.CmpNoCase(LIB_NONE) == 0 || filename.CmpNoCase(LIB_ALL) == 0 || filename.CmpNoCase(LIB_REPOS) == 0)
                continue;
            if (SymbolMode)
                CollectSymbols(filename, list, filter);
            else
                CollectFootprints(filename, list, filter);
            progress->Update(idx);
        }
    } else if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            wxMessageBox(wxT("The repository functions are disabled in this version."));
        #else
            wxArrayString data;
            wxString msg = curlList(&data, SymbolMode ? wxT("symbols") : wxT("footprints"), filter);
            if (msg.length() == 0) {
                for (unsigned idx = 0; idx < data.Count(); idx++) {
                    wxString line = data[idx];
                    int pos = line.Find(wxT('\t'));
                    if (pos <= 0)
                        continue;
                    wxString name = line.Left(pos);
                    wxString lname = name.Lower();
                    long insertpos = GetListPosition(name, list);
                    long item = list->InsertItem(insertpos, name);
                    list->SetItem(item, 1, line.Mid(pos + 1));
                    list->SetItem(item, 2, wxEmptyString);  /* no full path for the repository */
                }
            } else {
                wxMessageBox(wxT("Error listing repository\n\nRepository message: ") + msg);
            }
        #endif
    } else {
        if (SymbolMode)
            CollectSymbols(filename, list, filter, progress);
        else
            CollectFootprints(filename, list, filter, progress);
    }

    list->SetColumnWidth(0, wxLIST_AUTOSIZE);
    list->SetColumnWidth(1, wxLIST_AUTOSIZE);
    list->SetColumnWidth(2, 0); /* add an invisible column for the full path to the library */
    /* column widths are sometimes a few pixels off, so here is a kludge */
    list->SetColumnWidth(0, list->GetColumnWidth(0) + 2);
    list->SetColumnWidth(1, list->GetColumnWidth(1) + 2);

    delete progress;
    if (SymbolMode)
        msg = wxString::Format(wxT("Loaded %d symbols"), list->GetItemCount());
    else
        msg = wxString::Format(wxT("Loaded %d footprints"), list->GetItemCount());
    m_statusBar->SetStatusText(msg);

    if (SelectedPartLeft >= m_listModulesLeft->GetItemCount())
        SelectedPartLeft = -1;
    if (SelectedPartRight >= m_listModulesRight->GetItemCount())
        SelectedPartRight = -1;
    if (side == RIGHTPANEL && SelectedPartLeft >= 0) {
        LoadPart(SelectedPartLeft, m_listModulesLeft, m_choiceModuleLeft, 0);
        UpdateDetails(0);
        Update3DModel(PartData[0]);
        AutoZoom(0);
        m_panelView->Refresh();
    } else if (side == LEFTPANEL && SelectedPartRight >= 0) {
        LoadPart(SelectedPartRight, m_listModulesRight, m_choiceModuleRight, CompareMode ? 1 : 0);
        UpdateDetails(CompareMode ? 1 : 0);
        if (!CompareMode)
            Update3DModel(PartData[0]);
        AutoZoom(0);
        m_panelView->Refresh();
    }
}

void libmngrFrame::CollectSymbols(const wxString &path, wxListCtrl* list, const wxString& filter, wxProgressDialog* progress)
{
    wxString libname;
    if (ShowFullPaths) {
        libname = path;
    } else {
        wxFileName fname(path);
        libname = fname.GetFullName();
    }

    wxTextFile file;
    if (!file.Open(path)) {
        wxMessageBox(wxT("Failed to open symbol library ") + path);
        return;
    }

    if (progress)
        progress->SetRange(file.GetLineCount() - 1);

    /* verify the header */
    wxString line = file.GetLine(0);
    if (line.Left(16).CmpNoCase(wxT("EESchema-LIBRARY")) != 0
            && line.Left(13).CmpNoCase(wxT("EESchema-LIB ")) != 0)
    {
        wxMessageBox(wxT("Symbol library ") + path + wxT(" has an unsupported format."));
        file.Close();
        return;
    }
    m_statusBar->SetStatusText(wxT("Scanning ") + path);

    /* collect the symbol definitions in the file */
    for (unsigned idx = 1; idx < file.GetLineCount(); idx++) {
        line = file.GetLine(idx);
        if (line[0] == wxT('D') && line.Left(4).Cmp(wxT("DEF ")) == 0) {
            line = line.Mid(4);
            wxString name = GetToken(&line);
            /* remove leading tilde, this is an "non-visible" flag */
            if (name[0] == wxT('~'))
                name = name.Mid(1);
            bool match = true;
            if (filter.Length() > 0) {
                match = false;
                wxString lname = name.Lower();
                if (lname.Find(filter) >= 0)
                    match = true;
                wxArrayString symbol;
                if (!match && LoadSymbol(path, name, wxEmptyString, false, &symbol)) {
                    wxString field = GetDescription(symbol, true);
                    field.MakeLower();
                    if (line.Find(filter) >= 0)
                        match = true;
                    field = GetAliases(symbol);
                    field.MakeLower();
                    if (line.Find(filter) >= 0)
                        match = true;
                }
            }
            if (!match)
                continue;
            long insertpos = GetListPosition(name, list);
            long item = list->InsertItem(insertpos, name);
            list->SetItem(item, 1, libname);
            list->SetItem(item, 2, path);
        }
       if (progress)
            progress->Update(idx);
    }

    file.Close();
}

bool libmngrFrame::MatchFootprint(const wxString& libname, const wxString& symname, const wxString& filter)
{
    if (filter.Length() == 0)
        return true;    /* if no filter is present, every part matches */

    /* although we could try matching on the footprint name first (which is quick
       and does not require database lookup), it is not very useful: on a mismatch,
       the footprint would still be loaded and cached in the database and only a
       small number of footprints would immediately match on their names (so the
       performance gain is negligible) */

    /* check whether the footprint is already in the library */
    wxString path = libname.AfterLast(wxT(DIRSEP_CHAR));
    path = path.BeforeLast(wxT('.')) + wxT(".db");  /* change extension */
    path = theApp->GetUserDataPath() + wxT(DIRSEP_STR) + path;

    bool store_symbol = false;
    if (!wxFileExists(path))
        store_symbol = true;    /* database not found -> store footprint */
    unqlite *pDb;
    int rc = unqlite_open(&pDb, path.c_str(), UNQLITE_OPEN_CREATE);
    if (rc != UNQLITE_OK)
        return false;           /* failure opening/creating the database */
    if (!store_symbol) {
        /* database exists, see whether the entry already exists */
        unsigned char buffer[100];
        unqlite_int64 size = sizeof buffer;
        rc = unqlite_kv_fetch(pDb, symname.c_str(), -1, buffer, &size);
        if (rc == UNQLITE_NOTFOUND)
            store_symbol = true;    /* key was not found, must store */
    }

    if (store_symbol) {
        wxArrayString footprint;
        int version;
        if (LoadFootprint(libname, symname, wxEmptyString, false, &footprint, &version)) {
            unqlite_close(pDb); /* close the database, for caching the data */
            FootprintInfo fp(version);
            TranslatePadInfo(&footprint, &fp);
            CacheMetadata(libname, symname, true, footprint, fp);
            /* re-open the database */
            rc = unqlite_open(&pDb, path.c_str(), UNQLITE_OPEN_CREATE);
            if (rc != UNQLITE_OK)
                return false;
        }
    }

    /* get the metadata from the database */
    wxString metadata;
    unqlite_int64 size = 0;
    rc = unqlite_kv_fetch(pDb, symname.c_str(), -1, NULL, &size);
    if (rc == UNQLITE_OK) {
        size_t sz = (size_t)(size + 1); /* allocate one character extra for the '\0' terminator */
        unsigned char* buffer = (unsigned char*)malloc(sz * sizeof(char));
        if (buffer) {
            rc = unqlite_kv_fetch(pDb, symname.c_str(), -1, buffer, &size);
            wxASSERT(rc == UNQLITE_OK);
            buffer[sz - 1] = '\0';      /* zero-terminate the buffer */
            metadata = wxString(buffer);
            free(buffer);
        }
    }
    unqlite_close(pDb);

    /* extract fields from the metadata */
    wxString keywords = symname.Lower();    /* footprint name is matched on too */
    wxString pins = wxEmptyString;
    wxString pitch = wxEmptyString;
    wxString span = wxEmptyString;
    metadata = metadata.Lower();
    wxStringTokenizer tokenizer(metadata, wxT("\n"));
    while (tokenizer.HasMoreTokens()) {
        wxString token = tokenizer.GetNextToken();
        if (token.Left(12).Cmp(wxT("description=")) == 0 || token.Left(9).Cmp(wxT("keywords=")) == 0)
            keywords += wxT(" ") + token.AfterFirst(wxT('='));
        else if (token.Left(5).Cmp(wxT("pads=")) == 0)
            pins = token.AfterFirst(wxT('='));
        else if (token.Left(6).Cmp(wxT("pitch=")) == 0)
            pitch = token.AfterFirst(wxT('='));
        else if (token.Left(5).Cmp(wxT("span=")) == 0)
            span = token.AfterFirst(wxT('='));
    }

    /* break filter text into words, match each word */
    bool allmatch = true;
    tokenizer.SetString(filter, wxT(" ,"), wxTOKEN_STRTOK);
    while (allmatch && tokenizer.HasMoreTokens()) {
        bool match = false;
        wxString token = tokenizer.GetNextToken();
        if (token.Left(5).Cmp(wxT("pins:")) == 0 || token.Left(5).Cmp(wxT("pads:")) == 0) {
            /* match both on total pin count and on regular pin count */
            long count = 0;
            token.AfterFirst(wxT(':')).ToLong(&count);
            long nonregpins = 0, regularpins = 0;
            pins.ToLong(&regularpins);
            if (pins.Find(wxT('+')) > 0)
                pins.AfterFirst(wxT('+')).ToLong(&nonregpins);
            match = (count == regularpins || count == regularpins + nonregpins);
        } else if (token.Left(6).Cmp(wxT("pitch:")) == 0) {
            token = token.AfterFirst(wxT(':'));
            match = pitch.Left(token.Length()).Cmp(token) == 0;
        } else if (token.Left(5).Cmp(wxT("span:")) == 0) {
            token = token.AfterFirst(wxT(':'));
            match = span.Left(token.Length()).Cmp(token) == 0;
        } else {
            if (keywords.Find(token) >= 0)
                match = true;
        }
        allmatch = allmatch && match;
    }

    return allmatch;
}

void libmngrFrame::CollectFootprints(const wxString &path, wxListCtrl* list, const wxString& filter, wxProgressDialog* progress)
{
    wxFileName fname(path);
    wxString libname = ShowFullPaths ? path : fname.GetFullName();

    if (wxFileName::DirExists(path)) {
        m_statusBar->SetStatusText(wxT("Scanning ") + path);
        wxDir dir(path);
        if (!dir.IsOpened())
            return; /* error message already given */
        wxArrayString modlist;
        dir.GetAllFiles(path, &modlist, wxT("*.kicad_mod"), wxDIR_FILES);
        if (progress)
            progress->SetRange(modlist.Count() - 1);
        for (unsigned idx = 0; idx < modlist.Count(); idx++) {
            wxFileName modfile(modlist[idx]);
            wxString name = modfile.GetName();
            if (MatchFootprint(path, name, filter)) {
                long insertpos = GetListPosition(name, list);
                long item = list->InsertItem(insertpos, name);
                list->SetItem(item, 1, libname);
                list->SetItem(item, 2, path);
            }
            if (progress)
                progress->Update(idx);
        }
    } else if (fname.GetExt().Cmp(wxT("kicad_mod")) == 0) {
        wxString name = fname.GetName();
        if (MatchFootprint(path, name, filter)) {
            long insertpos = GetListPosition(name, list);
            long item = list->InsertItem(insertpos, name);
            list->SetItem(item, 1, libname);
            list->SetItem(item, 2, path);
        }
    } else {
        wxTextFile file;
        if (!file.Open(path)) {
            wxMessageBox(wxT("Failed to open footprint library ") + path);
            return;
        }

        /* verify the header */
        wxString line = file.GetLine(0);
        line = line.Left(19);
        if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
            wxMessageBox(wxT("Footprint library ") + path + wxT(" has an unsupported format."));
            file.Close();
            return;
        }
        m_statusBar->SetStatusText(wxT("Scanning ") + path);

        /* search the index */
        unsigned idx = 1;
        while (idx < file.GetLineCount()) {
            line = file.GetLine(idx);
            idx++;
            if (line.CmpNoCase(wxT("$INDEX")) == 0)
                break;
        }
        /* find the end of the index */
        unsigned tail = idx;
        while (tail < file.GetLineCount()) {
            line = file.GetLine(tail);
            if (line.CmpNoCase(wxT("$EndINDEX")) == 0)
                break;
            tail++;
        }
        /* read the modules (browse through the index) */
        if (progress) {
            progress->SetRange(tail - 1);
            wxASSERT(idx > 0);
            progress->Update(idx - 1);
        }
        while (idx < tail) {
            line = file.GetLine(idx);
            wxASSERT(line.CmpNoCase(wxT("$EndINDEX")) != 0);
            if (MatchFootprint(path, line, filter)) {
                long insertpos = GetListPosition(line, list);
                long item = list->InsertItem(insertpos, line);
                list->SetItem(item, 1, libname);
                list->SetItem(item, 2, path);
            }
            if (progress)
                progress->Update(idx);
            idx++;
        }

        file.Close();
    }
}

void libmngrFrame::SynchronizeLibraries(wxListCtrl* list1, wxListCtrl* list2)
{
    if (!SyncMode || list1->GetItemCount() == 0 || list2->GetItemCount() == 0)
        return;     /* don't synchronize if one of the lists is empty */

    /* first pass, remove phantom entries in both lists */
    for (int pass = 0; pass < 2; pass++) {
        wxListCtrl* list = (pass == 0) ? list1 : list2;
        long row = 0;
        while (row < list->GetItemCount()) {
            wxListItem info;
            info.SetId(row);
            info.SetColumn(1);
            info.SetMask(wxLIST_MASK_TEXT);
            list->GetItem(info);
            if (info.GetText().Length() == 0)
                list->DeleteItem(row);
            else
                row++;
        }
    }

    /* second pass, synchronize by adding phantom entries */
    long row = 0;
    while (row < list1->GetItemCount() || row < list2->GetItemCount()) {
        wxString sym1 = (row < list1->GetItemCount()) ? list1->GetItemText(row) : (wxString)wxEmptyString;
        wxString sym2 = (row < list2->GetItemCount()) ? list2->GetItemText(row) : (wxString)wxEmptyString;
        wxASSERT(sym1.Length() > 0 || sym2.Length() > 0);
        int cmp;
        if (sym1.Length() == 0)
            cmp = -1;   /* always copy from right to left when the left element is missing */
        else if (sym2.Length() == 0)
            cmp = 1;
        else
            cmp = CompareNames(sym1, sym2);
        if (cmp != 0) {
            /* mismatch on the names (a symbol in one list does not appear in the other) */
            wxListCtrl *list = NULL;
            wxString name;
            if (cmp < 0) {
                /* insert symbol from the right list into the left list */
                list = list1;
                name = sym2;
            } else if (cmp > 0) {
                /* insert symbol from the left list into the right list */
                list = list2;
                name = sym1;
            }
            wxASSERT(list != NULL);
            long insertpos = GetListPosition(name, list);
            long item = list->InsertItem(insertpos, name);
            list->SetItemTextColour(item, wxColour(160,96,96));
        }
        row++;
    }
}

void libmngrFrame::LoadPart(int index, wxListCtrl* list, wxChoice* choice, int fp)
{
    m_statusBar->SetStatusText(wxEmptyString);
    PartData[fp].Clear();

    /* get the name of the symbol and the library it is in */
    wxString symbol = list->GetItemText(index);
    wxASSERT(symbol.length() > 0);
    wxString author = wxEmptyString;
    wxString filename = wxEmptyString;
    /* also check the library selection, in the case of the repository,
       the user name is in column 1 (instead of the filename) */
    if (choice) {
        long libidx = choice->GetSelection();
        wxASSERT(libidx >= 0 && libidx < (long)choice->GetCount());
        filename = choice->GetString(libidx);
    }

    /* in case the module lists are synchronized, the user may click on an item that
       is not in the library */
    if (SyncMode) {
        wxListItem info;
        info.SetId(index);
        info.SetColumn(1);
        info.SetMask(wxLIST_MASK_TEXT);
        list->GetItem(info);
        if (info.GetText().Length() == 0) {
            /* clear everything and quit */
            Footprint[fp].Clear(VER_INVALID);
            BodySize[fp].Clear();
            LabelData[fp].Clear();
            if (PinData[fp] != NULL) {
                delete[] PinData[fp];
                PinData[fp] = 0;
                PinDataCount[fp] = 0;
            }
            FieldEdited = false;
            PartEdited = false;
            PinNamesEdited = false;
            return;
        }
    }

    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        FromRepository[fp] = true;
        wxListItem info;
        info.SetId(index);
        info.SetColumn(1);
        info.SetMask(wxLIST_MASK_TEXT);
        list->GetItem(info);
        author = info.GetText();
    } else {
        FromRepository[fp] = false;
        wxListItem info;
        info.SetId(index);
        info.SetColumn(2);
        info.SetMask(wxLIST_MASK_TEXT);
        list->GetItem(info);
        filename = info.GetText();
    }

    int version = VER_INVALID;
    if (SymbolMode) {
        if (!LoadSymbol(filename, symbol, author, DontRebuildTemplate, &PartData[fp])) {
            wxTextFile file;
            if (!file.Open(filename))
                wxMessageBox(wxT("Failed to open library ") + filename);
            else
                wxMessageBox(wxT("Symbol ") + symbol + wxT(" is not present."));
        }
    } else {
        if (!LoadFootprint(filename, symbol, author, DontRebuildTemplate, &PartData[fp], &version)) {
            /* check what the error is */
            wxTextFile file;
            if (!wxFileName::DirExists(filename) && !file.Open(filename))
                wxMessageBox(wxT("Failed to open library ") + filename);
            else if (version == VER_INVALID)
                wxMessageBox(wxT("Library ") + filename + wxT(" has an unsupported format."));
            else
                wxMessageBox(wxT("Footprint ") + symbol + wxT(" is not present."));
            return;
        }
    }

    /* verify that the symbol/footprint was actually found */
    wxASSERT(PartData[fp].Count() > 0);

    Footprint[fp].Clear(version);
    BodySize[fp].Clear();
    LabelData[fp].Clear();
    if (PinData[fp] != NULL) {
        delete[] PinData[fp];
        PinData[fp] = 0;
        PinDataCount[fp] = 0;
    }
    if (SymbolMode) {
        /* extract pin names and order */
        GetPinNames(PartData[fp], NULL, &PinDataCount[fp]);
        if (PinDataCount[fp] > 0) {
            PinData[fp] = new PinInfo[PinDataCount[fp]];
            wxASSERT(PinData[fp] != NULL);
            GetPinNames(PartData[fp], PinData[fp], NULL);
        }
    } else {
        /* get the pin pitch and pad size, plus the body size */
        Footprint[fp].Type = version;
        TranslatePadInfo(&PartData[fp], &Footprint[fp]);
        /* optionally cache the metadata (pitch, courtyard, descriptions) */
        CacheMetadata(filename, symbol, false, PartData[fp], Footprint[fp]);
    }
    GetBodySize(PartData[fp], &BodySize[fp], SymbolMode, Footprint[fp].Type >= VER_MM);
    GetTextLabelSize(PartData[fp], &LabelData[fp], SymbolMode, Footprint[fp].Type >= VER_MM);
    if (SymbolMode) {
        /* re-assign the pins to custom sections (now that the body size is known) */
        wxString templatename = GetTemplateName(PartData[fp]);
        GetTemplateSections(templatename, CustomPinSections[fp], sizearray(CustomPinSections[fp]));
        ReAssignPins(PinData[fp], PinDataCount[fp], BodySize[fp], CustomPinSections[fp], sizearray(CustomPinSections[fp]));
    }
    SymbolUnitNumber[fp] = 1;
    FieldEdited = false;
    PartEdited = false;
    PinNamesEdited = false;
}


void libmngrFrame::OnPasteGeneral(wxCommandEvent& event)
{
    if (SymbolMode) {
        /* check that the pin list table has the focus */
        if (FindFocus() == m_gridPinNames->GetGridWindow()) {
            OnPastePinList(event);
            return;
        }
    }

    /* if not handled, search on */
    event.Skip();
}


void libmngrFrame::OnTextFieldChange(wxCommandEvent& event)
{
    wxTextCtrl* field = (wxTextCtrl*)event.GetEventObject();
    if (field->IsEditable())
        FieldEdited = true;
    event.Skip();
}

void libmngrFrame::OnChoiceFieldChange(wxCommandEvent& event)
{
    FieldEdited = true;
    wxControl* ctrl = (wxControl*)event.GetEventObject();
    wxWindowID id = ctrl->GetId();
    if (id == m_choicePadShape->GetId())
        ChangePadInfo((wxControl*)m_choicePadShape);
    else if (id == m_choiceShape->GetId())
        ChangeShape((wxControl*)m_choiceShape);
    event.Skip();
}

void libmngrFrame::OnEnterTextInfo(wxCommandEvent& event)
{
    ChangeTextInfo((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusTextInfo(wxFocusEvent& event)
{
    ChangeTextInfo((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangeTextInfo(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        /* adjust the footprint or symbol */
        wxASSERT(PartData[0].Count() > 0);
        wxString field = m_txtDescription->GetValue();
        field = field.Trim(true);
        field = field.Trim(false);
        SetDescription(PartData[0], field, SymbolMode);

        field = m_txtAlias->GetValue();
        field = field.Trim(true);
        field = field.Trim(false);
        if (SymbolMode)
            SetAliases(PartData[0], field);
        else
            SetKeywords(PartData[0], field, SymbolMode);

        if (SymbolMode) {
            field = m_txtFootprintFilter->GetValue();
            field = field.Trim(true);
            field = field.Trim(false);
            SetFootprints(PartData[0], field);
        }
    }
}

static const wxString PinTypeNames[] = { wxT("input"), wxT("output"), wxT("bi-dir"),
                                         wxT("tristate"), wxT("passive"), wxT("open-collector"),
                                         wxT("open-emitter"), wxT("non-connect"), wxT("unspecified"),
                                         wxT("power-in"), wxT("power-out") };
static const wxString PinShapeNames[] = { wxT("line"), wxT("inverted"), wxT("clock"), wxT("inv.clock"), wxT("*") };
static const wxString PinSectionNames[] = { wxT("left"), wxT("right"), wxT("top"), wxT("bottom") };

wxString libmngrFrame::GetPinSectionName(int side, int index)
{
    wxASSERT(side == 0 || side == 1);
    wxASSERT(index >= 0 && index < PinInfo::SectionCount);
    if (index < sizearray(PinSectionNames))
        return PinSectionNames[index];

    index -= sizearray(PinSectionNames);
    if (CustomPinSections[side][index].IsValid())
        return CustomPinSections[side][index].Name();
    return wxEmptyString;
}

void libmngrFrame::OnPinNameChanged(wxGridEvent& event)
{
    static bool reenter = false;
    if (!reenter) {
        reenter = true;
        m_gridPinNames->SetCellBackgroundColour(event.GetRow(), event.GetCol(), CHANGED);
        #if 0
            m_gridPinNames->AutoSizeColumn(event.GetCol());
        #endif
        PinNamesEdited = true;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        bool refreshtemplate = false;

        wxASSERT(m_gridPinNames->GetNumberRows() == PinDataCount[0]);   /* pins cannot be added or removed in the table */
        for (int idx = 0; idx < m_gridPinNames->GetNumberRows(); idx++) {
            PinData[0][idx].number = m_gridPinNames->GetCellValue(idx, 0);
            wxString field = m_gridPinNames->GetCellValue(idx, 1);
            field.Replace(wxT(" "), wxT("_"));
            long part = 0;
            if (field.Left(1).IsNumber() && field[1] == wxT(':')) {
                field.Left(1).ToLong(&part);
                field = field.Mid(2);
                field.Trim(false);
            }
            PinData[0][idx].name = field;

            field = m_gridPinNames->GetCellValue(idx, 2);
            for (int t = 0; t < sizearray(PinTypeNames); t++)
                if (field.CmpNoCase(PinTypeNames[t]) == 0)
                    PinData[0][idx].type = t;

            field = m_gridPinNames->GetCellValue(idx, 3);
            for (int s = 0; s < sizearray(PinShapeNames); s++)
                if (field.CmpNoCase(PinShapeNames[s]) == 0)
                    PinData[0][idx].shape = s;

            field = m_gridPinNames->GetCellValue(idx, 4);
            for (int s = 0; s < PinInfo::SectionCount; s++) {
                wxString sectionname = GetPinSectionName(0, s);
                if (field.CmpNoCase(sectionname) == 0) {
                    if (PinData[0][idx].section != s) {
                        refreshtemplate = true;
                        PinData[0][idx].section = s;
                    }
                }
            }
        }

        SetPinNames(PartData[0], PinData[0], PinDataCount[0]);
        if (refreshtemplate)
            RebuildTemplate();
        m_panelView->Refresh();

        reenter = false;
    }
    event.Skip();
}

void libmngrFrame::OnPinRightClick(wxGridEvent& event)
{
    wxASSERT((wxGrid*)event.GetEventObject() == m_gridPinNames);
    if (CompareMode || PinDataCount[0] == 0) {
        event.Skip();
        return;
    }

    int row = event.GetRow();
    int col = event.GetCol();

    /* only allow a popup menu when the column is read-write */
    if (m_gridPinNames->IsReadOnly(row, col)) {
        event.Skip();
        return;
    }

    /* make sure that the cell clicked on, is selected too (but also make sure
       that cells in other columns are deselected) */
    bool addselection = true;
    int total = 0;
    for (int r = 0; r < m_gridPinNames->GetNumberRows() && addselection; r++) {
        for (int c = 0; c < m_gridPinNames->GetNumberCols() && addselection; c++) {
            if (m_gridPinNames->IsInSelection(r, c)) {
                if (c == col)
                    total++;
                else
                    addselection = false;
            }
        }
    }
    if (total == 0 || col <= 1)
        addselection = false;
    m_gridPinNames->SelectBlock(row, col, row, col, addselection);

    /* create a popup menu (depending on the column that was clicked) */
    wxString templatename = GetTemplateName(PartData[0]);
    wxMenu *menuPopup = new wxMenu;
    switch (col) {
    case 0: /* pin */
    case 1: /* name */
        menuPopup->Append(IDM_SWAPABOVE, wxT("Move up"), wxT("Swap this pin with the one above"));
        Connect(IDM_SWAPABOVE, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnSwapPins));
        menuPopup->Append(IDM_SWAPBELOW, wxT("Move down"), wxT("Swap this pin with the one below"));
        Connect(IDM_SWAPBELOW, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnSwapPins));
        menuPopup->AppendSeparator();
        menuPopup->Append(IDM_PASTEPINLIST, wxT("Paste pin list"), wxT("Import a list of pin assignments from the clipboard"));
        Connect(IDM_PASTEPINLIST, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnPastePinList));
        if (DontRebuildTemplate || templatename.length() == 0) {
            menuPopup->Enable(IDM_SWAPABOVE, false);
            menuPopup->Enable(IDM_SWAPBELOW, false);
        }
        break;
    case 2: /* type */
        for (int i = 0; i < sizearray(PinTypeNames); i++) {
            menuPopup->Append(IDM_PINTYPE + i, PinTypeNames[i]);
            Connect(IDM_PINTYPE + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnSetPinType));
        }
        break;
    case 3: /* shape */
        for (int i = 0; i < sizearray(PinShapeNames) - 1; i++) {
            menuPopup->Append(IDM_PINSHAPE + i, PinShapeNames[i]);
            Connect(IDM_PINSHAPE + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnSetPinType));
        }
        break;
    case 4: /* position (section) */
        for (int i = 0; i < PinInfo::SectionCount; i++) {
            wxString name = GetPinSectionName(0, i);
            if (!name.IsEmpty()) {
                menuPopup->Append(IDM_PINSECTION + i, name);
                Connect(IDM_PINSECTION + i, wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(libmngrFrame::OnSetPinType));
            }
        }
        break;
    }
    PopupMenu(menuPopup);
}

bool libmngrFrame::RearrangePinNames(int direction)
{
    if (CompareMode || DontRebuildTemplate || PinDataCount[0] == 0)
        return false;
    wxString templatename = GetTemplateName(PartData[0]);
    if (templatename.length() == 0)
        return false;

    int col = m_gridPinNames->GetGridCursorCol();
    int row = m_gridPinNames->GetGridCursorRow();
    int total = m_gridPinNames->GetNumberRows();
    if ((direction > 0 && row >= total - 1) || (direction < 0 && row <= 0))
        return false;

    /* swap the rows, except the sequence numbers */
    int alt = (direction > 0) ? row + 1 : row - 1;
    wxASSERT(alt >= 0 && alt < total);
    wxASSERT(PartData[0].Count() > 0);
    wxASSERT(row < PinDataCount[0] && alt < PinDataCount[0]);
    int seq_row = PinData[0][row].seq;
    int seq_alt = PinData[0][alt].seq;
    PinInfo t = PinData[0][row];
    PinData[0][row] = PinData[0][alt];
    PinData[0][alt] = t;
    PinData[0][row].seq = seq_row;
    PinData[0][alt].seq = seq_alt;

    /* rebuild the template */
    RebuildTemplate();

    /* refresh the data in the grid */
    wxString row_name = PinData[0][row].name;
    if (PinData[0][row].part > 0)
        row_name = wxString::Format(wxT("%d:"), PinData[0][row].part) + row_name;
    wxString alt_name = PinData[0][alt].name;
    if (PinData[0][alt].part > 0)
        alt_name = wxString::Format(wxT("%d:"), PinData[0][alt].part) + alt_name;
    m_gridPinNames->SetCellValue(row, 0, PinData[0][row].number);
    m_gridPinNames->SetCellValue(row, 1, row_name);
    m_gridPinNames->SetCellValue(row, 2, PinTypeNames[PinData[0][row].type]);
    m_gridPinNames->SetCellValue(row, 3, PinShapeNames[PinData[0][row].shape]);
    m_gridPinNames->SetCellValue(row, 4, GetPinSectionName(0, PinData[0][row].section));
    m_gridPinNames->SetCellValue(alt, 0, PinData[0][alt].number);
    m_gridPinNames->SetCellValue(alt, 1, alt_name);
    m_gridPinNames->SetCellValue(alt, 2, PinTypeNames[PinData[0][alt].type]);
    m_gridPinNames->SetCellValue(alt, 3, PinShapeNames[PinData[0][alt].shape]);
    m_gridPinNames->SetCellValue(alt, 4, GetPinSectionName(0, PinData[0][alt].section));

    PartEdited = true;
    m_btnSavePart->Enable(true);
    m_btnRevertPart->Enable(true);
    m_panelView->Refresh();

    m_gridPinNames->SetGridCursor(alt, col);
    return true;
}

void libmngrFrame::OnPinNameRearrange(wxKeyEvent& event)
{
    if (event.m_altDown && (event.m_keyCode == WXK_DOWN || event.m_keyCode == WXK_UP))
        RearrangePinNames((event.m_keyCode == WXK_DOWN) ? 1 : -1);
    else
        event.Skip();
}

void libmngrFrame::OnSwapPins(wxCommandEvent& event)
{
    RearrangePinNames((event.GetId() == IDM_SWAPBELOW) ? 1 : -1);
}

void libmngrFrame::OnPastePinList(wxCommandEvent& /*event*/)
{
    if (!wxTheClipboard->Open())
        return;
    if (!wxTheClipboard->IsSupported(wxDF_TEXT)) {
        wxTheClipboard->Close();
        return;
    }

    wxTextDataObject data;
    wxTheClipboard->GetData(data);
    wxTheClipboard->Close();
    wxArrayString lines = wxStringTokenize(data.GetText(), wxT("\r\n"), wxTOKEN_DEFAULT);
    if (lines.Count() == 0)
        return;

    /* determine separator: tab, comma, semicolon, space */
    bool allow_tab = true, allow_comma = true, allow_semi = true, allow_space = true;
    wxRegEx flt(wxT("[^\\w][1-9]\\d*[^\\w]"), wxRE_ADVANCED);
    wxASSERT(flt.IsValid());
    for (unsigned idx = 0; idx < lines.Count(); idx++) {
        wxString line = lines[idx];
        /* check whether there is a number delimited by whitespace or punctuation */
        if (!flt.Matches(line))
            continue;
        if (allow_tab && line.Find(wxT('\t')) < 0)
            allow_tab = false;
        if (allow_comma && line.Find(wxT(',')) < 0)
            allow_comma = false;
        if (allow_semi && line.Find(wxT(';')) < 0)
            allow_semi = false;
        if (allow_space && line.Find(wxT(' ')) < 0)
            allow_space = false;
    }
    wxChar separator = wxT('\0');
    if (allow_tab)
        separator = wxT('\t');
    else if (allow_comma)
        separator = wxT(',');
    else if (allow_semi)
        separator = wxT(';');
    else if (allow_space)
        separator = wxT(' ');
    else
        return;

    wxStringTokenizerMode mode = (separator == wxT(' ')) ? wxTOKEN_DEFAULT : wxTOKEN_RET_EMPTY_ALL;

    /* determine where the pin number is (first or last) */
    wxString line = lines[0];   //??? should check whether this is a line that matches the regular expression for valid text lines
    bool pinfirst = false;
    bool pinlast = false;
    wxArrayString tokens = wxStringTokenize(line, separator, mode);
    if (tokens.Count() < 2)
        return;
    long pinnr;
    wxString field = tokens[0];
    if (field.Right(1) == wxT(","))
        field = field.Left(field.Length() - 1);
    if (field.ToLong(&pinnr) && pinnr > 0)
        pinfirst = true;
    field = tokens[tokens.Count() - 1];
    if (field.ToLong(&pinnr) && pinnr > 0)
        pinlast = true;
    if ((pinfirst && pinlast) || (!pinfirst && !pinlast))
        return; /* pin number location could not be determined */

    /* check which column to use for the name */
    unsigned name_col;
    if (pinlast) {
        name_col = 0;   /* when the pin number is in the last column, assume the names to be in the first */
    } else {
        wxASSERT(pinfirst);
        name_col = 1;   /* assume that the name follows the number */
    }

    /* run over the clipboard data */
    wxString field_raw;
    for (unsigned idx = 0; idx < lines.Count(); idx++) {
        line = lines[idx];
        wxArrayString tokens = wxStringTokenize(line, separator, mode);
        if (tokens.Count() >= 2) {
            if (pinlast)
                field_raw = tokens[tokens.Count() - 1];
            else
                field_raw = tokens[0];
            if (field_raw.Right(1) == wxT(","))
                field = field_raw.Left(field_raw.Length() - 1);
            else
                field = field_raw;
            if (field.ToLong(&pinnr) && pinnr > 0) {
                /* see whether multiple pin numbers are on the same line */
                wxArrayLong pins;
                pins.Add(pinnr);
                unsigned col = name_col;
                if (pinfirst && field_raw.Right(1) == wxT(",")) {
                    for (col = 1; col < tokens.Count() - 1; col++) {
                        field_raw = tokens[col];
                        if (field_raw.Right(1) == wxT(","))
                            field = field_raw.Left(field_raw.Length() - 1);
                        else
                            field = field_raw;
                        if (!field.ToLong(&pinnr) || pinnr <= 0)
                            break;
                        pins.Add(pinnr);
                        if (field_raw.Right(1) != wxT(","))
                            break;
                    }
                    if (col < tokens.Count() - 1)
                        col += 1;   /* skip last column with pin number */
                }
                for (unsigned pinidx = 0; pinidx < pins.Count(); pinidx++) {
                    pinnr = pins[pinidx];
                    /* find the pin number in the grid */
                    int row, emptyrow = -1;
                    for (row = 0; row < m_gridPinNames->GetNumberRows(); row++) {
                        wxString pinname = m_gridPinNames->GetCellValue(row, 0);
                        long p;
                        if (pinname.ToLong(&p) && p == pinnr)
                            break;  /* found */
                        if (emptyrow < 0 && pinname.length() == 0)
                            emptyrow = row; /* keep reference to first row without a pin number */
                    }
                    /* if not found, use the first empty spot in the grid */
                    if (row >= m_gridPinNames->GetNumberRows() && emptyrow >= 0) {
                        row = emptyrow;
                        wxString pinname = wxString::Format(wxT("%d"), pinnr);
                        m_gridPinNames->SetCellValue(row, 0, pinname);
                    }
                    if (row < m_gridPinNames->GetNumberRows()) {
                        field = tokens[col];
                        if (field.CmpNoCase(wxT("V")) == 0 && (col + 1) < tokens.Count())
                            field += tokens[col + 1];  /* convert "V SS" to "VSS" */
                        m_gridPinNames->SetCellValue(row, 1, field);
                        /* check the other columns for keywords line "input" and "output" */
                        wxString type_name;
                        if (field.CmpNoCase(wxT("VCC")) == 0 || field.CmpNoCase(wxT("VDD")) == 0)
                            type_name = wxT("power_in");
                        else if (field.CmpNoCase(wxT("N.C.")) == 0 || field.CmpNoCase(wxT("N/C")) == 0 || field.CmpNoCase(wxT("NC")) == 0)
                            type_name = wxT("power_in");
                        for (unsigned c = col + 1; c < tokens.Count() && type_name.Length() == 0; c++) {
                            field = tokens[c];
                            field.MakeUpper();
                            if (field.Find(wxT("POWER")) >= 0 || field.Find(wxT("PWR")) >= 0)
                                type_name = wxT("power-in");
                            else if (field.Cmp(wxT("IN")) == 0 || field.Cmp(wxT("INPUT")) == 0 || field.Find(wxT("_IN")) >= 0)
                                type_name = wxT("input");
                            else if (field.Cmp(wxT("OUT")) == 0 || field.Cmp(wxT("OUTPUT")) == 0 || field.Find(wxT("_OUT")) >= 0)
                                type_name = wxT("output");
                            else if (field.Cmp(wxT("I/O")) == 0 || field.Find(wxT("_IO")) >= 0)
                                type_name = wxT("bi-dir");
                            else if (field.Cmp(wxT("N.C.")) == 0 || field.Cmp(wxT("N/C")) == 0 || field.Cmp(wxT("NC")) == 0)
                                type_name = wxT("non-connect");
                        }
                        if (type_name.Length() > 0)
                            m_gridPinNames->SetCellValue(row, 2, type_name);
                    }
                }
            }
        }
    }
}

void libmngrFrame::OnSetPinType(wxCommandEvent& event)
{
    /* get the string to store */
    wxString field;
    int id = event.GetId();
    if (id >= IDM_PINTYPE && id < IDM_PINSHAPE)
        field = PinTypeNames[id - IDM_PINTYPE];
    else if (id >= IDM_PINSHAPE && id < IDM_PINSECTION)
        field = PinShapeNames[id - IDM_PINSHAPE];
    else
        field = GetPinSectionName(0, id - IDM_PINSECTION);

    /* check which column has the selections (with wxWidgets, getting the
       list of selected cells is complicated, but since our grids are small,
       we run over the entire grid until the first selected cell is found */
    int col = -1;
    for (int r = 0; r < m_gridPinNames->GetNumberRows() && col < 0; r++)
        for (int c = 0; c < m_gridPinNames->GetNumberCols() && col < 0; c++)
            if (m_gridPinNames->IsInSelection(r, c))
                col = c;

    /* get array index of the selected string */
    int fieldidx = -1;
    switch (col) {
    case 2:
        for (int i = 0; i < sizearray(PinTypeNames) && fieldidx < 0; i++)
            if (field.CmpNoCase(PinTypeNames[i]) == 0)
                fieldidx = i;
        break;
    case 3:
        for (int i = 0; i < sizearray(PinShapeNames) && fieldidx < 0; i++)
            if (field.CmpNoCase(PinShapeNames[i]) == 0)
                fieldidx = i;
        break;
    case 4:
        for (int i = 0; i < PinInfo::SectionCount && fieldidx < 0; i++) {
            wxString sectionname = GetPinSectionName(0, i);
            if (field.CmpNoCase(sectionname) == 0)
                fieldidx = i;
        }
        break;
    default:
        wxASSERT(0);
    }
    wxASSERT(fieldidx >= 0);

    /* run over the column and act on selected items */
    for (int r = 0; r < m_gridPinNames->GetNumberRows(); r++) {
        if (m_gridPinNames->IsInSelection(r, col)) {
            m_gridPinNames->SetCellValue(r, col, field);
            m_gridPinNames->SetCellBackgroundColour(r, col, CHANGED);
            /* adjust in the structure too */
            switch (col) {
            case 2:
                PinData[0][r].type = fieldidx;
                break;
            case 3:
                PinData[0][r].shape = fieldidx;
                break;
            case 4:
                PinData[0][r].section = fieldidx;
                break;
            }
        }
    }

    #if 0
        m_gridPinNames->AutoSizeColumn(col);
    #endif

    PinNamesEdited = true;
    PartEdited = true;
    m_btnSavePart->Enable(true);
    m_btnRevertPart->Enable(true);

    SetPinNames(PartData[0], PinData[0], PinDataCount[0]);
    if (col == 4)
        RebuildTemplate();
    m_panelView->Refresh();
}

void libmngrFrame::OnEnterPadCount(wxCommandEvent& event)
{
    ChangePadCount((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusPadCount(wxFocusEvent& event)
{
    ChangePadCount((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangePadCount(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        /* check whether the pin count is valid */
        wxString field = m_txtPadCount->GetValue();
        long pins;
        field.ToLong(&pins);
        if (pins < 0)
            return;

        wxASSERT(PartData[0].Count() > 0);
        wxString templatename = GetTemplateName(PartData[0]);
        wxASSERT(templatename.length() > 0);    /* if there is no template, the field for
                                                   the number of pins is read-only */
        if (templatename.length() == 0)
            return;
        bool valid = ValidPinCount(pins, templatename, SymbolMode);
        if (!valid) {
            wxString msg = wxT("The template ") + templatename + wxString::Format(wxT(" does not allow a pin count of %d."), pins);
            /* get the notes for the template and add these to the error message */
            wxString note = GetTemplateHeaderField(templatename, wxT("pins"), SymbolMode);
            if (note.length() > 0)
                msg += wxT("\n\nNote: pin specification = ") + note;
            wxMessageBox(msg);
            return;
        }

        /* first, set all variables to the defaults of the template */
        wxString leftmod = GetSelection(m_listModulesLeft);
        wxString rightmod = GetSelection(m_listModulesRight);
        wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());
        wxString footprintname = (leftmod.length() > 0) ? leftmod : rightmod;
        wxString description = m_txtDescription->GetValue();
        wxString tags = m_txtAlias->GetValue();
        wxString prefix = wxEmptyString;
        if (SymbolMode)
            prefix = GetPrefix(PartData[0]);
        RPNexpression rpn;
        rpn.SetVariable(RPNvariable("PT", pins));   /* other defaults may depend on the correct pin count */
        SetVarDefaults(&rpn, templatename, footprintname, description, prefix, tags, true);
        /* then, update the variables from the fields */
        SetVarsFromFields(&rpn, SymbolMode);
        SetVarsFromTemplate(&rpn, templatename, SymbolMode);

        /* refresh the footprint or symbol from the template (completely) */
        wxArrayString templatemodule;
        LoadTemplate(templatename, &templatemodule, SymbolMode);
        if (SymbolMode) {
            if (pins == 0)
                return;
            /* first rebuild the pininfo structure */
            PinInfo* info = new PinInfo[pins];
            if (info == NULL)
                return;
            int totals[PinInfo::SectionCount];
            int counts[PinInfo::SectionCount];
            for (int s = 0; s < PinInfo::SectionCount; s++) {
                totals[s] = 0;
                counts[s] = 0;
                char str[20];
                sprintf(str, "PT:%d", s);
                rpn.Set(str);
                if (rpn.Parse() == RPN_OK) {
                    RPNvalue v = rpn.Value();
                    totals[s] = (int)(v.Double() + 0.001);
                }
            }
            /* copy the existing pins (but only if they have a label) */
            for (int i = 0; i < pins; i++) {
                info[i].section = -1;
                if (i < PinDataCount[0]
                    && (PinData[0][i].name.Cmp(wxT("~")) != 0
                        || PinData[0][i].type != PinInfo::Passive
                        || PinData[0][i].shape != PinInfo::Line))
                {
                    info[i] = PinData[0][i];
                    int s = info[i].section;
                    counts[s] += 1;
                }
            }
            /* complete with the new pins */
            int cursec = PinInfo::Left;
            for (int i = 0; i < pins; i++) {
                while (cursec < PinInfo::SectionCount && counts[cursec] >= totals[cursec])
                    cursec++;
                if (cursec >= PinInfo::SectionCount)
                    cursec = PinInfo::Left; /* on error, map remaining pins to left side */
                info[i].seq = i;
                if (info[i].section == -1) {
                    info[i].number = wxString::Format(wxT("%d"), i + 1);
                    info[i].name = wxT("~");
                    info[i].type = PinInfo::Passive;
                    info[i].shape = PinInfo::Line;
                    info[i].section = cursec;
                    info[i].part = 0;
                    counts[cursec] += 1;
                }
            }
            /* save the current aliases and footprint list, because these are absent from the templates */
            wxString aliases = GetAliases(PartData[0]);
            wxString footprints = GetFootprints(PartData[0]);
            /* create a symbol from the template, then restore the aliases and footprints */
            SymbolFromTemplate(&PartData[0], templatemodule, rpn, info, pins);
            if (aliases.Length() > 0)
                SetAliases(PartData[0], aliases);
            if (footprints.Length() > 0)
                SetFootprints(PartData[0], footprints);
            /* now swap the PinInfo in the symbol data structure */
            delete[] PinData[0];
            PinDataCount[0] = pins;
            PinData[0] = info;
            GetPinNames(PartData[0], PinData[0], NULL); /* reload pins, for updated information */
            /* grid must be refreshed / resized */
            int currows = m_gridPinNames->GetNumberRows();
            if (pins < currows) {
                m_gridPinNames->DeleteRows(pins, currows - pins);
            } else {
                m_gridPinNames->InsertRows(currows, pins - currows);
                m_gridPinNames->EnableEditing(true);
                for (int idx = currows; idx < pins; idx++) {
                    wxString field = PinData[0][idx].name;
                    if (PinData[0][idx].part > 0)
                        field = wxString::Format(wxT("%d:"), PinData[0][idx].part) + field;
                    m_gridPinNames->SetCellValue((int)idx, 0, PinData[0][idx].number);
                    m_gridPinNames->SetCellValue((int)idx, 1, field);
                    m_gridPinNames->SetCellValue((int)idx, 2, PinTypeNames[PinData[0][idx].type]);
                    m_gridPinNames->SetCellValue((int)idx, 3, PinShapeNames[PinData[0][idx].shape]);
                    m_gridPinNames->SetCellValue((int)idx, 4, GetPinSectionName(0, PinData[0][idx].section));
                    /* no cell needs to be set to read-only and no new cell needs to be coloured */
                    m_gridPinNames->SetCellEditor(idx, 2, new wxGridCellChoiceEditor(sizearray(PinTypeNames), PinTypeNames));
                    m_gridPinNames->SetCellEditor(idx, 3, new wxGridCellChoiceEditor(sizearray(PinShapeNames) - 1, PinShapeNames));
                    wxArrayString list;
                    for (int i = 0; i < PinInfo::SectionCount; i++) {
                        wxString name = GetPinSectionName(0, i);
                        if (name.Length() > 0)
                            list.Add(name);
                    }
                    m_gridPinNames->SetCellEditor(idx, 4, new wxGridCellChoiceEditor(list));
                }
            }
            /* existing pins may have been implicitly assigned to a different section;
                 update all rows of the fields */
            for (int idx = 0; idx < pins; idx++)
                m_gridPinNames->SetCellValue((int)idx, 4, GetPinSectionName(0, PinData[0][idx].section));
        } else {
            FootprintFromTemplate(&PartData[0], templatemodule, rpn, false);
            Footprint[0].Type = VER_MM; /* should already be set to this */
            TranslatePadInfo(&PartData[0], &Footprint[0]);
            Update3DModel(PartData[0]);
        }
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterPadInfo(wxCommandEvent& event)
{
    ChangePadInfo((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusPadInfo(wxFocusEvent& event)
{
    ChangePadInfo((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangePadInfo(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        /* adjust the footprint */
        wxASSERT(PartData[0].Count() > 0);
        FootprintInfo adjusted = Footprint[0];
        long idx;
        wxString field;

        idx = m_choicePadShape->GetSelection();
        if (idx >= 0)
            field = m_choicePadShape->GetString(idx);
        if (field.CmpNoCase(wxT("Round")) == 0)
            adjusted.PadShape = 'C';
        else if (field.CmpNoCase(wxT("Obround")) == 0)
            adjusted.PadShape = 'O';
        else if (field.CmpNoCase(wxT("Rectangular")) == 0)
            adjusted.PadShape = 'R';
        else if (field.CmpNoCase(wxT("Round + square")) == 0)
            adjusted.PadShape = 'S';
        else if (field.CmpNoCase(wxT("Trapezoid")) == 0)
            adjusted.PadShape = 'T';
        else if (field.CmpNoCase(wxT("Rounded rectangle")) == 0)
            adjusted.PadShape = 'D';

        /* some fields are disabled depending on the pad shape */
        SetTextField(m_txtPadLength, m_txtPadLength->GetValue(), (adjusted.PadShape == 'C' || adjusted.PadShape == 'S') ? PROTECTED : ENABLED);
        field = m_txtPadRadius->GetValue();
        if (adjusted.PadShape == 'D' && field.Length() == 0)
            SetTextField(m_txtPadRadius, wxT("25"), CHANGED);
        else
            SetTextField(m_txtPadRadius, field, (adjusted.PadShape == 'D') ? ENABLED : PROTECTED);

        /* for circular and square+round pads, the width & length should be the same */
        if (adjusted.PadShape == 'C' || adjusted.PadShape == 'S') {
            if (ctrl == m_txtPadWidth)
                m_txtPadLength->SetValue(m_txtPadWidth->GetValue());
            else if (ctrl == m_txtPadLength)
                m_txtPadWidth->SetValue(m_txtPadLength->GetValue());
        }

        double dim;
        field = m_txtPadWidth->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.PadSize[0].Set(dim, adjusted.PadSize[0].GetY());
        field = m_txtPadLength->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.PadSize[0].Set(adjusted.PadSize[0].GetX(), dim);
        /* if the pad shape is round or round+square, force the width and height
           of that pad to be equal */
        if (adjusted.PadShape == 'C' || adjusted.PadShape == 'S') {
            if (adjusted.PadSize[0].GetX() < adjusted.PadSize[0].GetY())
                dim = adjusted.PadSize[0].GetX();
            else
                dim = adjusted.PadSize[0].GetY();
            adjusted.PadSize[0].Set(dim, dim);
        }
        /* if the pad is rounded rectangle, read the relevant field; otherwise
           force the rounding to be zero */
        if (adjusted.PadShape == 'D') {
            long percent;
            field = m_txtPadRadius->GetValue();
            if (field.length() > 0 && field.ToLong(&percent))
                adjusted.PadRRatio = percent;
        } else {
            adjusted.PadRRatio = 0;
        }

        field = m_txtAuxPadWidth->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.PadSize[1].Set(dim, adjusted.PadSize[1].GetY());
        field = m_txtAuxPadLength->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.PadSize[1].Set(adjusted.PadSize[1].GetX(), dim);

        field = m_txtDrillSize->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim))
            adjusted.DrillSize = (dim > 0.02) ? dim : 0.0;

        /* force the footprint to use mm (for convenience) */
        if (TranslateUnits(PartData[0], Footprint[0].Type >= VER_MM, true)) {
            Footprint[0].Type = VER_MM;
            adjusted.Type = VER_MM; /* footprint is converted to mm as well */
        }
        AdjustPad(PartData[0], &Footprint[0], adjusted);

        /* if this footprint is based on a template and the template has the "STP"
           variable set (Silk-To-Pad clearance), re-run the template */
        if (CheckTemplateVar(wxT("STP"))) {
            ChangeBodyInfo(NULL);
            TranslatePadInfo(&PartData[0], &Footprint[0]);
        }
        Update3DModel(PartData[0]);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterPitchInfo(wxCommandEvent& event)
{
    ChangePitch((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusPitchInfo(wxFocusEvent& event)
{
    ChangePitch((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangePitch(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        double pitch;
        wxString field = m_txtPitch->GetValue();
        if (field.length() == 0 || !field.ToDouble(&pitch) || pitch <= 0.02)
            return;
        if (Footprint[0].SOT23pitch && Footprint[0].RegPadCount == 3)
            pitch *= 2; /* 3-pin SOT23 or variant */

        /* force the footprint to use mm (for convenience) */
        wxASSERT(PartData[0].Count() > 0);
        if (TranslateUnits(PartData[0], Footprint[0].Type >= VER_MM, true))
            Footprint[0].Type = VER_MM;

        wxASSERT((Footprint[0].MtxWidth == 0 && Footprint[0].MtxHeight == 0)
                 || (Footprint[0].MtxWidth > 0 && Footprint[0].MtxHeight > 0));
        if (Footprint[0].MtxWidth > 0) {
            wxASSERT(!Footprint[0].SOT23pitch);
            AdjustPitchGrid(PartData[0], Footprint[0].Pitch, pitch);
        } else {
            /* check the number of pins in each line */
            wxASSERT(Footprint[0].PadLines > 0);
            int seq = Footprint[0].RegPadCount / Footprint[0].PadLines * Footprint[0].PadSequence;
            if (Footprint[0].SOT23pitch) {
                wxASSERT(Footprint[0].RegPadCount == 3 || Footprint[0].RegPadCount == 5);
                wxASSERT(Footprint[0].PadSequence == 1);
                seq = (Footprint[0].RegPadCount == 3) ? seq - 1 : seq + 1;
            }
            /* find initial direction */
            CoordPair startpin[4];
            int direction;  /* 0=down, 1=right, 2=up, 3=left */
            if (Footprint[0].PitchVertical)
                direction = (Footprint[0].PitchPins[0].GetY() < Footprint[0].PitchPins[1].GetY()) ? 0 : 2;
            else
                direction = (Footprint[0].PitchPins[0].GetX() < Footprint[0].PitchPins[1].GetX()) ? 1 : 3;
            /* find start pins */
            startpin[direction] = Footprint[0].PitchPins[0];
            int opposing = (direction + 2) % 4;
            if (opposing % 2 == 0 && Footprint[0].SpanHor > EPSILON) {
                if (!Equal(startpin[direction].GetX(), Footprint[0].SpanHorPins[0].GetX(), TOLERANCE))
                    startpin[opposing] = Footprint[0].SpanHorPins[0];
                else if (!Equal(startpin[direction].GetX(), Footprint[0].SpanHorPins[1].GetX(), TOLERANCE))
                    startpin[opposing] = Footprint[0].SpanHorPins[1];
            }
            if (opposing % 2 == 1 && Footprint[0].SpanVer > EPSILON) {
                if (!Equal(startpin[direction].GetY(), Footprint[0].SpanVerPins[0].GetY(), TOLERANCE))
                    startpin[opposing] = Footprint[0].SpanVerPins[0];
                else if (!Equal(startpin[direction].GetY(), Footprint[0].SpanVerPins[1].GetY(), TOLERANCE))
                    startpin[opposing] = Footprint[0].SpanVerPins[1];
            }
            if (direction % 2 == 0 && Footprint[0].SpanVer > EPSILON) {
                /* positions 0 and 2 set, now set 1 and 3 */
                if (Footprint[0].SpanVerPins[0].GetY() < Footprint[0].SpanVerPins[1].GetY()) {
                    startpin[3] = Footprint[0].SpanVerPins[0];
                    startpin[1] = Footprint[0].SpanVerPins[1];
                } else {
                    startpin[1] = Footprint[0].SpanVerPins[0];
                    startpin[3] = Footprint[0].SpanVerPins[1];
                }
            }
            if (direction % 2 == 1 && Footprint[0].SpanHor > EPSILON) {
                /* positions 1 and 3 set, now set 0 and 2 */
                if (Footprint[0].SpanHorPins[0].GetX() < Footprint[0].SpanHorPins[1].GetX()) {
                    startpin[0] = Footprint[0].SpanVerPins[0];
                    startpin[2] = Footprint[0].SpanVerPins[1];
                } else {
                    startpin[2] = Footprint[0].SpanVerPins[0];
                    startpin[0] = Footprint[0].SpanVerPins[1];
                }
            }

            /* now modify the footprint */
            int base = 1;
            for (int line = 1; line <= Footprint[0].PadLines; line++) {
                switch (direction) {
                case 0:
                    AdjustPitchVer(PartData[0], base, base + seq - 1, Footprint[0].PadSequence,
                                                 startpin[direction].GetX(), pitch);
                    break;
                case 1:
                    AdjustPitchHor(PartData[0], base, base + seq - 1, Footprint[0].PadSequence,
                                                 startpin[direction].GetY(), pitch);
                    break;
                case 2:
                    AdjustPitchVer(PartData[0], base, base + seq - 1, Footprint[0].PadSequence,
                                                 startpin[direction].GetX(), -pitch);
                    break;
                case 3:
                    AdjustPitchHor(PartData[0], base, base + seq - 1, Footprint[0].PadSequence,
                                                 startpin[direction].GetY(), -pitch);
                    break;
                }
                if (Footprint[0].PadSequence == 1) {
                    base += seq;
                    if (Footprint[0].PadLines == 2)
                        direction = (direction + 2) % 4;
                    else
                        direction = (direction + 1) % 4;
                    /* for 5-pin SOT23 style, double the pitch for the next vertical row */
                    if (direction == 2 && Footprint[0].SOT23pitch && Footprint[0].RegPadCount == 5) {
                        pitch *= 2;
                        seq -= 1;
                        wxASSERT(seq == 2);
                    }
                } else {
                    /* for zig-zag style of dual-row pin headers (or similar) */
                    base += 1;
                    /* keep the direction, but change the X or Y coordinate */
                    int d = (Footprint[0].PadLines == 2) ? (direction + 2) % 4 : (direction + 1) % 4;
                    startpin[direction] = startpin[d];
                }
            }
        } /* if (MtxWidth > 0) */

        /* if this footprint is based on a template and the template has the "STP"
           variable set (Silk-To-Pad clearance), re-run the template */
        if (CheckTemplateVar(wxT("STP")))
            ChangeBodyInfo(NULL);

        TranslatePadInfo(&PartData[0], &Footprint[0]);
        Update3DModel(PartData[0]);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterSpanInfo(wxCommandEvent& event)
{
    ChangeSpan((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusSpanInfo(wxFocusEvent& event)
{
    ChangeSpan((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangeSpan(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        /* force the footprint to use mm (for convenience) */
        wxASSERT(PartData[0].Count() > 0);
        if (TranslateUnits(PartData[0], Footprint[0].Type >= VER_MM, true))
            Footprint[0].Type = VER_MM;

        /* adjust the footprint */
        wxString field;
        double dim, delta;
        CoordPair p1, p2;
        field = m_txtPadSpanX->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02 && !Equal(dim, Footprint[0].SpanHor, 0.01)) {
            if (Footprint[0].SpanHorPins[0].GetX() < Footprint[0].SpanHorPins[1].GetX()) {
                p1 = Footprint[0].SpanHorPins[0];
                p2 = Footprint[0].SpanHorPins[1];
            } else {
                p1 = Footprint[0].SpanHorPins[1];
                p2 = Footprint[0].SpanHorPins[0];
            }
            delta = (dim - Footprint[0].SpanHor) / 2;
            MovePadHor(PartData[0], p1.GetX(), p1.GetX() - delta);
            MovePadHor(PartData[0], p2.GetX(), p2.GetX() + delta);
            Footprint[0].SpanHor = dim;
            Footprint[0].SpanHorPins[0].Set(p1.GetX() - delta, p1.GetY());
            Footprint[0].SpanHorPins[1].Set(p2.GetX() + delta, p2.GetY());
        }
        field = m_txtPadSpanY->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02 && !Equal(dim, Footprint[0].SpanVer, 0.01)) {
            if (Footprint[0].SpanVerPins[0].GetY() < Footprint[0].SpanVerPins[1].GetY()) {
                p1 = Footprint[0].SpanVerPins[0];
                p2 = Footprint[0].SpanVerPins[1];
            } else {
                p1 = Footprint[0].SpanVerPins[1];
                p2 = Footprint[0].SpanVerPins[0];
            }
            delta = (dim - Footprint[0].SpanVer) / 2;
            MovePadVer(PartData[0], p1.GetY(), p1.GetY() - delta);
            MovePadVer(PartData[0], p2.GetY(), p2.GetY() + delta);
            Footprint[0].SpanVer = dim;
            Footprint[0].SpanVerPins[0].Set(p1.GetX(), p1.GetY() - delta);
            Footprint[0].SpanVerPins[1].Set(p2.GetX(), p2.GetY() + delta);
        }

        /* if this footprint is based on a template and the template has the "STP"
             variable set (Silk-To-Pad clearance), re-run the template */
        if (CheckTemplateVar(wxT("STP")))
            ChangeBodyInfo(NULL);
        /* always re-translate the pad information, because the pitch anchor points
           must remain correct */
        TranslatePadInfo(&PartData[0], &Footprint[0]);
        Update3DModel(PartData[0]);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterBodyInfo(wxCommandEvent& event)
{
    ChangeBodyInfo((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusBodyInfo(wxFocusEvent& event)
{
    ChangeBodyInfo((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangeBodyInfo(wxControl* ctrl)
{
    if (FieldEdited || !ctrl) {
        if (ctrl) {
            ctrl->SetBackgroundColour(CHANGED);
            ctrl->Refresh();
        }
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        RebuildTemplate();
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterShapeHeight(wxCommandEvent& event)
{
    ChangeShape((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusShapeHeight(wxFocusEvent& event)
{
    ChangeShape((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::ChangeShape(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        Update3DModel(PartData[0]);
        m_panelView->Refresh();
    }
}

void libmngrFrame::OnEnterLabelField(wxCommandEvent& event)
{
    ChangeLabelInfo((wxControl*)event.GetEventObject());
}

void libmngrFrame::OnKillFocusLabelField(wxFocusEvent& event)
{
    ChangeLabelInfo((wxControl*)event.GetEventObject());
    event.Skip();
}

void libmngrFrame::OnLabelShowHide(wxCommandEvent& event)
{
    FieldEdited = true;
    ChangeLabelInfo((wxControl*)event.GetEventObject());
}

void libmngrFrame::ChangeLabelInfo(wxControl* ctrl)
{
    if (FieldEdited) {
        ctrl->SetBackgroundColour(CHANGED);
        ctrl->Refresh();
        FieldEdited = false;
        PartEdited = true;
        m_btnSavePart->Enable(true);
        m_btnRevertPart->Enable(true);

        /* adjust the symbol or footprint */
        wxASSERT(PartData[0].Count() > 0);
        LabelInfo adjusted = LabelData[0];

        wxString field;
        double dim;

        field = m_txtRefLabel->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.RefLabelSize = dim;
        adjusted.RefLabelVisible = m_chkRefLabelVisible->GetValue();
        field = m_txtValueLabel->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            adjusted.ValueLabelSize = dim;
        adjusted.ValueLabelVisible = m_chkValueLabelVisible->GetValue();

        /* force the footprint to use mm (for convenience) */
        if (TranslateUnits(PartData[0], Footprint[0].Type >= VER_MM, true))
            Footprint[0].Type = VER_MM;
        SetTextLabelSize(PartData[0], adjusted, SymbolMode, true);

        /* if this is a footprint that is based on a template, re-run the
           template because the text position is often a function of the text
           size */
        if (!SymbolMode && GetTemplateName(PartData[0]).Length() > 0) {
            ChangeBodyInfo(NULL);
            TranslatePadInfo(&PartData[0], &Footprint[0]);
        }

        m_panelView->Refresh();
    }
}

bool libmngrFrame::CacheMetadata(const wxString& libname, const wxString& symname, bool force_export, const wxArrayString& module,
                                 const FootprintInfo& footprint)
{
    if (libname.CmpNoCase(LIB_REPOS) == 0)
        return false;   /* repository can serve as its own cache */

    wxString dbpath = libname.AfterLast(wxT(DIRSEP_CHAR));
    dbpath = dbpath.BeforeLast(wxT('.')) + wxT(".db");  /* change extension */
    dbpath = theApp->GetUserDataPath() + wxT(DIRSEP_STR) + dbpath;

    /* get the date/time stamp of the library (because it is stored, and because
       it is used  for checking whether the metadata must be updated */
    time_t stamp;
    if (wxFileName::DirExists(libname)) {
        /* test time/date of footprint file */
        wxFileName fname(libname, symname + wxT(".kicad_mod"));
        stamp = wxFileModificationTime(fname.GetFullPath());
    } else {
        /* test time/date of library file for legacy, or of single footprint for exported kicad_mod file */
        stamp = wxFileModificationTime(libname);
    }

    /* Metadata must be cached if:
        a) the entire database does not exist yet
        b) the symbol does not exist yet in the database
        c) the symbol exists in the database, but with a different timestamp
        d) a (modified) symbol is being saved (i.e. when force_export is true) */
    if (!wxFileExists(dbpath))
        force_export = true;    /* this handles case a) */
    unqlite *pDb;
    int rc = unqlite_open(&pDb, dbpath.c_str(), UNQLITE_OPEN_CREATE);
    if (rc != UNQLITE_OK)
        return false;           /* failure creating the database */
    if (!force_export) {
        /* database exists, see whether the entry already exists */
        unsigned char buffer[100];
        unqlite_int64 size = sizeof buffer;
        rc = unqlite_kv_fetch(pDb, symname.c_str(), -1, buffer, &size);
        if (rc == UNQLITE_OK) {
            /* footprint exists, get its time stamp */
            time_t dbtime = 0;
            if (memcmp(buffer, "stamp=", 6) == 0)
                dbtime = strtoul((char*)buffer + 6, NULL, 10);
            if (dbtime == stamp) {
                /* footprint exists and its date/time stamp matches the stamp of the
                   metadata -> no need to store it again */
                unqlite_close(pDb);
                return true;
            }
        } else if (rc != UNQLITE_NOTFOUND) {
            /* there was an I/O error -> don't try to store (because force_save was not set either) */
            unqlite_close(pDb);
            return true;
        }
    }

    /* collect all data into a string */
    wxString data = wxString::Format(wxT("stamp=%ld\n"), (unsigned long)stamp);

    wxString string = GetDescription(module, SymbolMode);
    if (string.Length() > 0)
        data += wxT("description=") + string + wxT("\n");

    string = GetKeywords(module, SymbolMode);
    if (string.Length() > 0)
        data += wxT("keywords=") + string + wxT("\n");

    //??? store footprint aliases in the FootprintInfo class

    if (footprint.Pitch > EPSILON)
        data += wxString::Format(wxT("pitch=%f\n"), footprint.Pitch);

    double span = 0;
    if (footprint.SpanHor <= EPSILON)
        span = footprint.SpanVer;
    else if (footprint.SpanVer <= EPSILON)
        span = footprint.SpanHor;
    else
        span = footprint.PitchVertical ? footprint.SpanHor : footprint.SpanVer;
    if (span > EPSILON)
        data += wxString::Format(wxT("span=%f\n"), span);

    wxASSERT(footprint.RegPadCount <= footprint.PadCount);
    if (footprint.PadCount == footprint.RegPadCount || footprint.RegPadCount == 0)
        data += wxString::Format(wxT("pads=%d\n"), footprint.PadCount);
    else
        data += wxString::Format(wxT("pads=%d+%d\n"), footprint.RegPadCount, footprint.PadCount - footprint.RegPadCount);
    double xmin, ymin, xmax, ymax;
    xmin = ymin = xmax = ymax = 0;
    for (unsigned pinnr = 0; pinnr < footprint.Pads.Count(); pinnr++) {
        CoordSize pad = footprint.Pads[pinnr];
        if (pinnr == 0) {
            xmin = pad.GetLeft();
            ymin = pad.GetTop();
            xmax = pad.GetRight();
            ymax = pad.GetBottom();
        } else {
            if (xmin > pad.GetLeft())
                xmin = pad.GetLeft();
            if (ymin > pad.GetTop())
                ymin = pad.GetTop();
            if (xmax < pad.GetRight())
                xmax = pad.GetRight();
            if (ymax < pad.GetBottom())
                ymax = pad.GetBottom();
        }
        data += wxString::Format(wxT("pad%d=%f %f\n"), pinnr + 1, pad.GetMidX(), pad.GetMidY());
    }

    CoordPair stdpad = footprint.PadSize[0];
    if (stdpad.GetX() > EPSILON) {
        if (footprint.PadRightAngle[0])
            stdpad.Set(stdpad.GetY(), stdpad.GetX());
        data += wxString::Format(wxT("padsize=%f %f\n"), stdpad.GetX(), stdpad.GetY());
    }

    if (xmax - xmin > EPSILON && ymax - ymin > EPSILON)
        data += wxString::Format(wxT("courtyard=%f %f\n"), xmax - xmin, ymax - ymin);

    //??? export body-size too, but this should be the true body size

    /* store data under symname as key */
    const char* ptr = data.utf8_str();
    rc = unqlite_kv_store(pDb, symname.c_str(), -1, ptr, strlen(ptr));
    unqlite_close(pDb);

    return (rc == UNQLITE_OK);
}

bool libmngrFrame::SavePart(int index, wxListCtrl* list)
{
    if (FromRepository[0])
        return false;
    wxASSERT(index >= 0);

    /* get the name of the symbol and the library it is in */
    wxString symbol = list->GetItemText(index);
    wxListItem info;
    info.SetId(index);
    info.SetColumn(1);
    info.SetMask(wxLIST_MASK_TEXT);
    list->GetItem(info);
    wxString filename = info.GetText();
    if (filename.CmpNoCase(LIB_REPOS) != 0) {
        info.SetColumn(2);
        list->GetItem(info);
        filename = info.GetText();
    }

    wxASSERT(PartData[0].Count() > 0);
    bool result;
    if (SymbolMode) {
        if (PinNamesEdited) {
            /* verify pin numbers */
            wxASSERT(m_gridPinNames->GetNumberRows() == PinDataCount[0]);
            for (int idx = 0; idx < m_gridPinNames->GetNumberRows(); idx++) {
                if (PinData[0][idx].number.length() == 0)
                    wxMessageBox(wxT("No pin number for label: ") + PinData[0][idx].name);
                for (int i = 0; i < idx; i++) {
                    // check duplicate pin number
                    if (PinData[0][i].number.CmpNoCase(PinData[0][idx].number) == 0)
                        wxMessageBox(wxT("Duplicate pin number: ") + PinData[0][idx].number);
                    // check that there is no other pin with the same name on the same part (except N.C.)
                    if (PinData[0][i].part == PinData[0][idx].part
                        && PinData[0][i].type != PinInfo::NC && PinData[0][idx].type != PinInfo::NC
                        && PinData[0][i].name.CmpNoCase(PinData[0][idx].name) == 0
                        && PinData[0][i].name.Cmp(wxT("~")) != 0 && PinData[0][i].name.CmpNoCase(wxT("Vdd")) != 0
                        && PinData[0][i].name.CmpNoCase(wxT("Vss")) != 0 && PinData[0][i].name.CmpNoCase(wxT("GND")) != 0)
                        wxMessageBox(wxT("Duplicate pin name: ") + PinData[0][idx].name
                                     + wxT("\nPins ") + PinData[0][i].number + wxT(" and ") + PinData[0][idx].number);
                }
                if (PinData[0][idx].part > 1) {
                    /* find the pin with the same name in section 1 (it should be found) */
                    int base;
                    for (base = 0; base < PinDataCount[0]; base++)
                        if (PinData[0][base].part == 1 && PinData[0][base].name.CmpNoCase(PinData[0][idx].name) == 0)
                            break;
                    if (base < PinDataCount[0]) {
                        /* copy all other parameters from the pin at section 1 (keep only the pin number and the part */
                        wxString pinnr = PinData[0][idx].number;
                        int part = PinData[0][idx].part;
                        PinData[0][idx] = PinData[0][base];
                        PinData[0][idx].number = pinnr;
                        PinData[0][idx].part = part;
                    } else {
                        wxMessageBox(wxT("Pin number ") + PinData[0][idx].number + wxT(" with name ") + PinData[0][idx].name
                                     + wxT(" has no equivalent in the base unit (unit 1)"));
                    }
                }
            }
        }
        wxASSERT(ExistSymbol(filename, symbol));
        RemoveSymbol(filename, symbol);
        result = InsertSymbol(filename, symbol, PartData[0]);
    } else {
        int targettype = LibraryType(filename);
        wxArrayString module;
        if (Footprint[0].Type == VER_MM && targettype == VER_S_EXPR) {
            /* translate the module to s-expression; this may be needed in the
               case of footprints generated from templates, because the
               templates are in legacy/mm format */
            TranslateToSexpr(&module, PartData[0]);
        } else {
            module = PartData[0];
        }
        //??? check whether all pads are SMD pads; if so, toggle type of the module to SMD
        wxASSERT(ExistFootprint(filename, symbol));
        RemoveFootprint(filename, symbol);
        result = InsertFootprint(filename, symbol, module, Footprint[0].Type >= VER_MM);
        /* cache the metadata (pitch, courtyard, descriptions) */
        CacheMetadata(filename, symbol, true, module, Footprint[0]);
    }
    if (result) {
        PartEdited = false;
    } else {
        wxMessageBox(wxT("Operation failed."));
        return false;
    }

    wxString templatename = GetTemplateName(PartData[0]);
    wxString vrmlpath = GetVRMLPath(filename, PartData[0]);
    if (vrmlpath.Length() > 0 && templatename.length() > 0) {
        /* re-generate 3D model
           first, set all variables to the defaults of the template */
        wxString description = m_txtDescription->GetValue();
        RPNexpression rpn;
        SetVarDefaults(&rpn, templatename, symbol, description);
        SetVarsFromFields(&rpn, false);
        /* get the template to use */
        wxString modelname;
        int idx = m_choiceShape->GetCurrentSelection();
        if (idx >= 0 && idx < (int)m_choiceShape->GetCount()) {
            modelname = m_choiceShape->GetString(idx);
        } else {
            modelname = GetTemplateHeaderField(templatename, wxT("model"), SymbolMode);
            if (modelname.length() == 0)
                modelname = templatename;
            else
                modelname = modelname.BeforeFirst(wxT(' '));
        }
        VRMLFromTemplate(vrmlpath, modelname, rpn);
    }

    return true;
}

bool libmngrFrame::CheckSavePart()
{
    if (!PartEdited)
        return true;    /* nothing changed (so nothing to save) */

    wxString msg;
    if (SymbolMode)
        msg = wxT("Do you want to save the current symbol first?");
    else
        msg = wxT("Do you want to save the footprint symbol first?");
    int reply = wxMessageBox(msg, wxT("Unsaved changes"), wxYES_NO | wxICON_QUESTION);
    if (reply == wxYES) {
        wxASSERT(SelectedPartLeft == -1 || SelectedPartRight == -1);
        long index = (SelectedPartLeft >= 0) ? SelectedPartLeft : SelectedPartRight;
        wxListCtrl* list = (SelectedPartLeft >= 0) ? m_listModulesLeft : m_listModulesRight;
        return SavePart(index, list);
    }
    return false;
}

void libmngrFrame::OnSavePart(wxCommandEvent& /*event*/)
{
    long idxleft = m_listModulesLeft->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    long idxright = m_listModulesRight->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    wxASSERT(idxleft == -1 || idxright == -1);
    long index;
    wxListCtrl* list;
    if (idxleft >= 0) {
        list = m_listModulesLeft;
        index = idxleft;
    } else {
        list = m_listModulesRight;
        index = idxright;
    }
    wxASSERT(index >= 0);
    wxASSERT(!FromRepository[0]);
    SavePart(index, list);

    /* reload the part just saved */
    LoadPart(index, list, 0, 0);
    UpdateDetails(0);
    Update3DModel(PartData[0]);
    m_panelView->Refresh();
    if (SymbolMode)
        m_statusBar->SetStatusText(wxT("Saved modified symbol"));
    else
        m_statusBar->SetStatusText(wxT("Saved modified footprint"));
}

void libmngrFrame::OnRevertPart(wxCommandEvent& /*event*/)
{
    long idxleft = m_listModulesLeft->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    long idxright = m_listModulesRight->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    wxASSERT(idxleft == -1 || idxright == -1);
    if (idxleft >= 0)
        LoadPart(idxleft, m_listModulesLeft, m_choiceModuleLeft, 0);
    else
        LoadPart(idxright, m_listModulesRight, m_choiceModuleRight, 0);
    UpdateDetails(0);
    Update3DModel(PartData[0]);
    m_panelView->Refresh();
    m_statusBar->SetStatusText(wxT("Changes reverted"));
}

void libmngrFrame::SetTextField(wxTextCtrl* ctrl, const wxString& value, const wxColour& status)
{
    wxASSERT(ctrl != NULL);
    ctrl->SetValue(value);
    ctrl->SetEditable(status != PROTECTED);
    ctrl->SetBackgroundColour(status);
}

bool libmngrFrame::CheckTemplateVar(const wxString& varname)
{
    wxASSERT(PartData[0].Count() > 0);
    wxString templatename = GetTemplateName(PartData[0]);
    if (templatename.length() == 0)
        return false;

    if (varname.IsEmpty())
        return true;

    wxString field = GetTemplateHeaderField(templatename, wxT("param"), SymbolMode);
    if (field.length() == 0)
        return false;
    return (field.Find(wxT("@") + varname) >= 0);
}

bool libmngrFrame::SetVarDefaults(RPNexpression *rpn, const wxString& templatename,
                                  const wxString& footprintname, const wxString& description,
                                  const wxString& prefix, const wxString& tags, bool silent)
{
    /* first set the defaults on the parameter line */
    wxASSERT(templatename.length() > 0);
    wxString field = GetTemplateHeaderField(templatename, wxT("param"), SymbolMode);
    if (field.length() > 0) {
        rpn->Set(field.utf8_str());
        RPN_ERROR err = rpn->Parse();
        if (err != RPN_EMPTY && !silent)
            wxMessageBox(wxT("The '#param' line in the template has an error."));
    }
    /* copy PSH and PRR in #param to $PSH and $PRR */
    rpn->Set("PSH");
    const char* shape = (rpn->Parse() == RPN_OK) ? rpn->Value().Text() : "";
    rpn->SetVariable(RPNvariable("$PSH", shape));
    rpn->Set("PRR");
    double rratio = (rpn->Parse() == RPN_OK) ? rpn->Value().Double() : 0.25;
    rpn->SetVariable(RPNvariable("$PRR", rratio));

    /* then set the defaults from the user settings (possibly overriding those
         of the #param line) */
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    config->SetPath(wxT("/template"));
    wxString varname;
    long token;
    bool ok = config->GetFirstEntry(varname, token);
    while (ok) {
        double val;
        if (config->Read(varname, &val))
            rpn->SetVariable(RPNvariable(varname.utf8_str(), val));
        ok = config->GetNextEntry(varname, token);
    }
    delete config;

    rpn->SetVariable(RPNvariable("NAME", footprintname.utf8_str()));
    rpn->SetVariable(RPNvariable("DESCR", description.utf8_str()));
    rpn->SetVariable(RPNvariable("REF", prefix.utf8_str()));
    rpn->SetVariable(RPNvariable("TAGS", tags.utf8_str()));

    /* set any pin section criterions */
    if (SymbolMode) {
        for (int index = 0; index < sizearray(CustomPinSections[0]); index++) {
            if (CustomPinSections[0][index].IsValid()) {
                wxString name = wxString::Format(wxT("PSC:%d"), index + PinInfo::LeftCustom);
                rpn->SetVariable(RPNvariable(name.utf8_str(), CustomPinSections[0][index].Criterion()));
            }
        }
    }

    return true;
}

bool libmngrFrame::SetVarsFromFields(RPNexpression *rpn, bool SymbolMode)
{
    long val;
    double dim;

    wxString field = m_txtPadCount->GetValue();
    if (field.length() > 0 && field.ToLong(&val) && val > 0)
        rpn->SetVariable(RPNvariable("PT", val));
    field = m_txtBodyLength->GetValue();
    if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
        rpn->SetVariable(RPNvariable("BL", dim));
    field = m_txtBodyWidth->GetValue();
    if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
        rpn->SetVariable(RPNvariable("BW", dim));
    field = m_txtRefLabel->GetValue();
    if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02) {
        if (!m_chkRefLabelVisible->GetValue())
            dim = -dim;
        rpn->SetVariable(RPNvariable("TSR", dim));
    }
    field = m_txtValueLabel->GetValue();
    if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02) {
        if (!m_chkValueLabelVisible->GetValue())
            dim = -dim;
        rpn->SetVariable(RPNvariable("TSV", dim));
    }

    if (!SymbolMode) {
        val = m_choicePadShape->GetSelection();
        if (val < 0)
            val = 0;
        wxString shape = m_choicePadShape->GetString(val);
        if (shape.CmpNoCase(wxT("Round")) == 0)
            shape = wxT("circle");
        else if (shape.CmpNoCase(wxT("Obround")) == 0)
            shape = wxT("oval");
        else if (shape.CmpNoCase(wxT("Rectangular")) == 0)
            shape = wxT("rect");
        else if (shape.CmpNoCase(wxT("Round + square")) == 0)
            shape = wxT("sqcircle");
        else if (shape.CmpNoCase(wxT("Trapezoid")) == 0)
            shape = wxT("trapezoid");
        else if (shape.CmpNoCase(wxT("Rounded rectangle")) == 0)
            shape = wxT("roundrect");
        rpn->SetVariable(RPNvariable("$PSH", shape));
        field = m_txtPadRadius->GetValue();
        if (field.length() > 0 && field.ToLong(&val))
            rpn->SetVariable(RPNvariable("$PRR", val / 100.0));
        field = m_txtPadWidth->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("PW", dim));
        if (shape.CmpNoCase(wxT("circle")) == 0 || shape.CmpNoCase(wxT("sqcircle")) == 0) {
            /* for circle and square+circle, set pad length to the same value as the pad width */
            rpn->SetVariable(RPNvariable("PL", dim));
        } else {
            field = m_txtPadLength->GetValue();
            if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
                rpn->SetVariable(RPNvariable("PL", dim));
        }
        field = m_txtAuxPadLength->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("PLA", dim));
        field = m_txtAuxPadWidth->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("PWA", dim));
        field = m_txtPitch->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("PP", dim));
        field = m_txtPadSpanX->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("SH", dim));
        field = m_txtPadSpanY->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("SV", dim));
        field = m_txtDrillSize->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("DS", dim));
        field = m_txtShapeHeight->GetValue();
        if (field.length() > 0 && field.ToDouble(&dim) && dim > 0.02)
            rpn->SetVariable(RPNvariable("BH", dim));
        /* the pitch direction is not in a field; it is set from the parsed info */
        rpn->SetVariable(RPNvariable("PPDIR", Footprint[0].PitchVertical ? 1 : 0));
    }
    return true;
}

void libmngrFrame::SetVarsFromTemplate(RPNexpression *rpn, const wxString& templatename, bool SymbolMode)
{
    wxString field = GetTemplateHeaderField(templatename, wxT("flags"), SymbolMode);
    int pos = field.Find(wxT("aux-pad"));
    if (pos >= 0) {
        wxString params = field.Mid(pos);
        params = params.AfterFirst(wxT('('));
        params = params.BeforeFirst(wxT(')'));
        wxString type = params.BeforeFirst(wxT(','));   /* should be "mechanic", "flag" or "flag-nc" */
        wxString pm = params.AfterFirst(wxT(','));
        long padcount = 0;
        if (pm.Cmp(wxT("*")) == 0)
            padcount = 1;
        else
            pm.ToLong(&padcount);
        if (type.CmpNoCase(wxT("flag")) == 0)
            padcount--; /* for a flag (connected exposed pad), one pad is included in the "normal" pad count */
        rpn->SetVariable(RPNvariable("PTA", padcount));
    }
}

bool libmngrFrame::RebuildTemplate()
{
    wxASSERT(PartData[0].Count() > 0);
    wxString templatename = GetTemplateName(PartData[0]);
    wxASSERT(templatename.length() > 0);    /* if there is no template, the fields for
                                               the body size are read-only */
    if (templatename.length() == 0)
        return false;

    /* first, set all variables to the defaults of the template */
    wxString leftmod = GetSelection(m_listModulesLeft);
    wxString rightmod = GetSelection(m_listModulesRight);
    wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());
    wxString footprintname = (leftmod.length() > 0) ? leftmod : rightmod;
    wxString description = m_txtDescription->GetValue();
    wxString tags = m_txtAlias->GetValue();
    wxString prefix = wxEmptyString;
    if (SymbolMode)
        prefix = GetPrefix(PartData[0]);
    RPNexpression rpn;
    if (SymbolMode)
        rpn.SetVariable(RPNvariable("PT", PinDataCount[0]));    /* other defaults depend on the correct pin count */
    SetVarDefaults(&rpn, templatename, footprintname, description, prefix, tags, true);

    /* then, update the variables from the fields
         update all fields, because the body size may depend on other settings,
         such as pitch, too */
    SetVarsFromFields(&rpn, SymbolMode);
    SetVarsFromTemplate(&rpn, templatename, SymbolMode);

    /* refresh the footprint from the template, but only partially (by default) */
    wxString field = GetTemplateHeaderField(templatename, wxT("flags"), SymbolMode);
    bool bodyonly = (field.Find(wxT("rebuild")) == wxNOT_FOUND);

    wxArrayString templatemodule;
    LoadTemplate(templatename, &templatemodule, SymbolMode);
    bool result;
    if (SymbolMode) {
        /* save the current aliases and footprint list, because these are absent from the templates */
        wxString aliases = GetAliases(PartData[0]);
        wxString footprints = GetFootprints(PartData[0]);
        result = SymbolFromTemplate(&PartData[0], templatemodule, rpn, PinData[0], PinDataCount[0]);
        if (aliases.Length() > 0)
            SetAliases(PartData[0], aliases);
        if (footprints.Length() > 0)
            SetFootprints(PartData[0], footprints);
    } else {
        /* since the footprint is only partially regenerated, the other part must
           be converted to mm (and legacy) */
        if (Footprint[0].Type == VER_MIL) {
            TranslateUnits(PartData[0], false, true);
        } else if (Footprint[0].Type == VER_S_EXPR) {
            wxArrayString partdata;
            TranslateToLegacy(&partdata, PartData[0]);
            PartData[0] = partdata;
        }
        Footprint[0].Type = VER_MM;
        result = FootprintFromTemplate(&PartData[0], templatemodule, rpn, bodyonly);
        Update3DModel(PartData[0]);
    }
    return result;
}

bool libmngrFrame::Update3DModel(const wxArrayString& module)
{
#if defined NO_3DMODEL
    (void)module;
    return false;
#else
    /* a few easy checks */
    if (!ModelMode)
        return false;
    wxASSERT(!SymbolMode && !CompareMode);  /* ModelMode can only be made active for footprints and no comparison */
    wxString templatename = GetTemplateName(module);
    wxString vrmlpath = GetVRMLPath(wxEmptyString, module); /* only to check that a VRML template is defined for this footprint */
    /* if the VRML model is based on a template, regenerate it in a temporary file, so that the model
       is guaranteed to have the correct dimentsions; if there is no template, but there is a VRML
       model, show that model (but disable the edit fields) */
    bool UseTempModel = (vrmlpath.Length() > 0 && templatename.Length() > 0);
    if (vrmlpath.Length() > 0 && !UseTempModel) {
        /* there is an existing VRML module (but not based on a template), check what library the
           part comes from */
        wxASSERT(!CompareMode);
        wxString library, author;
        wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &library, &author);
        wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &library, &author);
        wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());
        /* get the full path to the model */
        vrmlpath = GetVRMLPath(library, module);
    }

    /* re-generate 3D model */
    if (UseTempModel) {
        /* first, set all variables to the defaults of the template */
        RPNexpression rpn;
        SetVarDefaults(&rpn, templatename, wxEmptyString, wxEmptyString);
        SetVarsFromFields(&rpn, false);
        /* get the template to use */
        wxString modelname;
        int idx = m_choiceShape->GetCurrentSelection();
        if (idx >= 0 && idx < (int)m_choiceShape->GetCount()) {
            modelname = m_choiceShape->GetString(idx);
        } else {
            modelname = GetTemplateHeaderField(templatename, wxT("model"), SymbolMode);
            if (modelname.length() == 0)
                modelname = templatename;
            else
                modelname = modelname.BeforeFirst(wxT(' '));
        }

        /* create the VRML file as a temporary file, then load */
        vrmlpath = wxFileName::CreateTempFileName(wxT("kcdlbr"));
        VRMLFromTemplate(vrmlpath, modelname, rpn);
    }

    bool result = sceneGraph.load(vrmlpath.utf8_str());
    if (UseTempModel)
        wxRemoveFile(vrmlpath); /* remove the file, but only if it was generated */
    if (result) {
        sceneGraph.initialize();
        if (sceneGraph.getViewpointNode() == NULL)
            sceneGraph.zoomAllViewpoint();
        sceneGraph.update();
    }
    return result;
#endif
}

void libmngrFrame::UpdateDetails(int fp)
{
    /* reset all fields and colours */
    m_txtDescription->SetToolTip(wxEmptyString);
    SetTextField(m_txtDescription, wxEmptyString, PROTECTED);
    SetTextField(m_txtAlias, wxEmptyString, PROTECTED);
    if (SymbolMode) {
        m_spinUnitSelect->SetRange(1,1);
        m_spinUnitSelect->SetValue(1);
        SetTextField(m_txtFootprintFilter, wxEmptyString, PROTECTED);
        SetTextField(m_txtPadCount, wxEmptyString, PROTECTED);
        m_gridPinNames->ClearGrid();
        m_gridPinNames->SetColLabelSize(0);
        if (m_gridPinNames->GetNumberRows() > 0)
            m_gridPinNames->DeleteRows(0, m_gridPinNames->GetNumberRows());
        wxSizer* sizer = m_gridPinNames->GetContainingSizer();
        SetTextField(m_txtBodyLength, wxEmptyString, PROTECTED);
        SetTextField(m_txtBodyWidth, wxEmptyString, PROTECTED);
        SetTextField(m_txtRefLabel, wxEmptyString, PROTECTED);
        m_chkRefLabelVisible->SetValue(false);
        SetTextField(m_txtValueLabel, wxEmptyString, PROTECTED);
        m_chkValueLabelVisible->SetValue(false);
        wxASSERT(sizer != 0);
        sizer->Layout();

        m_lblUnitSelect->Enable(false);
        m_spinUnitSelect->Enable(false);
        m_gridPinNames->EnableEditing(false);
        m_chkRefLabelVisible->Enable(false);
        m_chkValueLabelVisible->Enable(false);

        m_gridPinNames->SetBackgroundColour(PROTECTED);
        m_chkRefLabelVisible->SetBackgroundColour(wxNullColour);
        m_chkValueLabelVisible->SetBackgroundColour(wxNullColour);
    } else {
        m_choiceShape->Clear();
        SetTextField(m_txtPadCount, wxEmptyString, PROTECTED);
        m_choicePadShape->SetSelection(0);
        SetTextField(m_txtPadWidth, wxEmptyString, PROTECTED);
        SetTextField(m_txtPadLength, wxEmptyString, PROTECTED);
        SetTextField(m_txtPadRadius, wxEmptyString, PROTECTED);
        SetTextField(m_txtPitch, wxEmptyString, PROTECTED);
        SetTextField(m_txtPadSpanX, wxEmptyString, PROTECTED);
        SetTextField(m_txtPadSpanY, wxEmptyString, PROTECTED);
        SetTextField(m_txtDrillSize, wxEmptyString, PROTECTED);
        SetTextField(m_txtAuxPadLength, wxEmptyString, PROTECTED);
        SetTextField(m_txtAuxPadWidth, wxEmptyString, PROTECTED);
        SetTextField(m_txtBodyLength, wxEmptyString, PROTECTED);
        SetTextField(m_txtBodyWidth, wxEmptyString, PROTECTED);
        SetTextField(m_txtRefLabel, wxEmptyString, PROTECTED);
        m_chkRefLabelVisible->SetValue(false);
        SetTextField(m_txtValueLabel, wxEmptyString, PROTECTED);
        m_chkValueLabelVisible->SetValue(false);
        m_choiceShape->SetSelection(0);
        SetTextField(m_txtShapeHeight, wxEmptyString, PROTECTED);

        m_choicePadShape->Enable(false);
        m_chkRefLabelVisible->Enable(false);
        m_chkValueLabelVisible->Enable(false);
        m_choiceShape->Enable(false);

        m_choicePadShape->SetBackgroundColour(wxNullColour);
        m_chkRefLabelVisible->SetBackgroundColour(wxNullColour);
        m_chkValueLabelVisible->SetBackgroundColour(wxNullColour);
        m_choiceShape->SetBackgroundColour(wxNullColour);
    }

    m_btnSavePart->Enable(false);
    m_btnRevertPart->Enable(false);

    if (PartData[fp].Count() == 0)
        return;
    bool DefEnable = !CompareMode && !FromRepository[fp];
    wxString templatename = GetTemplateName(PartData[fp]);

    wxString field = GetDescription(PartData[fp], SymbolMode);
    SetTextField(m_txtDescription, field, DefEnable ? ENABLED : PROTECTED);
    m_txtDescription->SetToolTip(field);

    if (SymbolMode) {
        /* schematic mode */

        field = GetAliases(PartData[fp]);
        SetTextField(m_txtAlias, field, DefEnable ? ENABLED : PROTECTED);

        field = GetFootprints(PartData[fp]);
        SetTextField(m_txtFootprintFilter, field, DefEnable ? ENABLED : PROTECTED);

        bool enable = templatename.length() > 0 && DefEnable;
        if (enable) {
            /* check whether the template allows multiple pin counts (many 2-pin
                 components only allow 2 pins) */
            field = GetTemplateHeaderField(templatename, wxT("pins"), SymbolMode);
            if (field.length() > 0 && field.Find(wxT(' ')) < 0)
                enable = false; /* since leading and trailing white-space was already
                                     trimmed, when more white-space exists, it must be as
                                     a separator */
        }
        SetTextField(m_txtPadCount, wxString::Format(wxT("%d"), PinDataCount[fp]), enable ? ENABLED : PROTECTED);

        int unitcount = GetUnitCount(PartData[fp]);
        enable = unitcount > 1 && DefEnable;
        m_spinUnitSelect->SetRange(1, unitcount);
        m_spinUnitSelect->SetValue(SymbolUnitNumber[fp]);
        m_spinUnitSelect->Enable(enable);
        m_spinUnitSelect->SetBackgroundColour(enable ? ENABLED : PROTECTED);
        m_lblUnitSelect->Enable(enable);

        if (m_gridPinNames->GetNumberRows() > 0)
            m_gridPinNames->DeleteRows(0, m_gridPinNames->GetNumberRows());
        m_gridPinNames->InsertRows(0, PinDataCount[fp]);
        m_gridPinNames->EnableEditing(true);
        for (int idx = 0; idx < PinDataCount[fp]; idx++) {
            wxString field =  PinData[fp][idx].name;
            if ( PinData[fp][idx].part > 0)
                field = wxString::Format(wxT("%d:"),  PinData[fp][idx].part) + field;
            m_gridPinNames->SetCellValue((int)idx, 0, PinData[fp][idx].number);
            m_gridPinNames->SetCellValue((int)idx, 1, field);
            m_gridPinNames->SetCellValue((int)idx, 2, PinTypeNames[PinData[fp][idx].type]);
            m_gridPinNames->SetCellValue((int)idx, 3, PinShapeNames[PinData[fp][idx].shape]);
            m_gridPinNames->SetCellValue((int)idx, 4, GetPinSectionName(0, PinData[fp][idx].section));
            bool enable = templatename.length() > 0 && DefEnable;
            m_gridPinNames->SetReadOnly(idx, 0, !DefEnable);
            m_gridPinNames->SetReadOnly(idx, 1, !DefEnable);
            m_gridPinNames->SetReadOnly(idx, 2, !DefEnable);
            m_gridPinNames->SetReadOnly(idx, 3, !DefEnable);
            m_gridPinNames->SetReadOnly(idx, 4, !enable);
            m_gridPinNames->SetCellBackgroundColour(idx, 0, DefEnable ? ENABLED : PROTECTED);
            m_gridPinNames->SetCellBackgroundColour(idx, 1, DefEnable ? ENABLED : PROTECTED);
            m_gridPinNames->SetCellBackgroundColour(idx, 2, DefEnable ? ENABLED : PROTECTED);
            m_gridPinNames->SetCellBackgroundColour(idx, 3, DefEnable ? ENABLED : PROTECTED);
            m_gridPinNames->SetCellBackgroundColour(idx, 4, enable ? ENABLED : PROTECTED);
            if (DefEnable) {
                m_gridPinNames->SetCellEditor(idx, 2, new wxGridCellChoiceEditor(sizearray(PinTypeNames), PinTypeNames));
                m_gridPinNames->SetCellEditor(idx, 3, new wxGridCellChoiceEditor(sizearray(PinShapeNames) - 1, PinShapeNames));
            }
            if (enable) {
                wxArrayString list;
                for (int i = 0; i < PinInfo::SectionCount; i++) {
                    wxString name = GetPinSectionName(0, i);
                    if (name.Length() > 0)
                        list.Add(name);
                }
                m_gridPinNames->SetCellEditor(idx, 4, new wxGridCellChoiceEditor(list));
            }
        }
        if (PinDataCount[fp] > 0) {
            #if defined _WIN32
                m_gridPinNames->SetColLabelSize(16);    //??? should read this from a user-setting
            #else
                m_gridPinNames->SetColLabelSize(20);
            #endif
        }
        m_gridPinNames->AutoSizeColumn(0);
        m_gridPinNames->AutoSizeColumn(1);
        m_gridPinNames->AutoSizeColumn(2);
        m_gridPinNames->AutoSizeColumn(3);
        m_gridPinNames->AutoSizeColumn(4);
        wxSizer* sizer = m_gridPinNames->GetContainingSizer();
        wxASSERT(sizer != 0);
        sizer->Layout();
        m_panelSettings->FitInside();   /* force recalculation of the panel (calling layout is not enough) */

    } else {
        /* footprint mode */

        field = GetKeywords(PartData[fp], SymbolMode);
        SetTextField(m_txtAlias, field, DefEnable ? ENABLED : PROTECTED);

        bool enable = templatename.length() > 0 && DefEnable;
        if (enable) {
            /* check whether the template allows multiple pin counts (many 2-pin
                 components only allow 2 pins) */
            field = GetTemplateHeaderField(templatename, wxT("pins"), SymbolMode);
            if (field.length() == 0 || field.Find(wxT(' ')) < 0)
                enable = false; /* since leading and trailing white-space was already
                                     trimmed, when more white-space exists, it must be as
                                     a separator */
        }
        SetTextField(m_txtPadCount, wxString::Format(wxT("%d"), Footprint[fp].PadCount), enable ? ENABLED : PROTECTED);

        enable = DefEnable;
        int idx;
        switch (Footprint[fp].PadShape) {
        case 'C':
            idx = m_choicePadShape->FindString(wxT("Round"));
            break;
        case 'O':
            idx = m_choicePadShape->FindString(wxT("Obround"));
            break;
        case 'R':
            idx = m_choicePadShape->FindString(wxT("Rectangular"));
            break;
        case 'S':
            idx = m_choicePadShape->FindString(wxT("Round + square"));
            break;
        case 'T':
            idx = m_choicePadShape->FindString(wxT("Trapezoid"));
            break;
        case 'D':
            idx = m_choicePadShape->FindString(wxT("Rounded rectangle"));
            break;
        default:
            idx = m_choicePadShape->FindString(wxT("(varies)"));
            enable = false;
        }
        wxASSERT(idx >= 0);
        m_choicePadShape->SetSelection(idx);
        m_choicePadShape->Enable(enable);
        m_choicePadShape->SetBackgroundColour(enable ? ENABLED : PROTECTED);

        const CoordPair& padsize = Footprint[fp].PadSize[0];
        if (padsize.GetX() > EPSILON)
            SetTextField(m_txtPadWidth, wxString::Format(wxT("%.3f"), padsize.GetX()), DefEnable ? ENABLED : PROTECTED);
        if (padsize.GetY() > EPSILON)
            SetTextField(m_txtPadLength, wxString::Format(wxT("%.3f"), padsize.GetY()), DefEnable ? ENABLED : PROTECTED);
        int radius = Footprint[fp].PadRRatio;
        if (radius > 0)
            SetTextField(m_txtPadRadius, wxString::Format(wxT("%d"), radius), DefEnable ? ENABLED : PROTECTED);

        const CoordPair& auxpadsize = Footprint[fp].PadSize[1];
        if (auxpadsize.GetX() > EPSILON)
            SetTextField(m_txtAuxPadWidth, wxString::Format(wxT("%.3f"), auxpadsize.GetX()), DefEnable ? ENABLED : PROTECTED);
        if (auxpadsize.GetY() > EPSILON)
            SetTextField(m_txtAuxPadLength, wxString::Format(wxT("%.3f"), auxpadsize.GetY()), DefEnable ? ENABLED : PROTECTED);

        if (Footprint[fp].Pitch > EPSILON) {
            enable = DefEnable && Footprint[fp].RegPadCount > 0
                               && Footprint[fp].PadLines > 0
                               && Footprint[fp].OriginCentred;
            SetTextField(m_txtPitch, wxString::Format(wxT("%.3f"), Footprint[fp].Pitch), enable ? ENABLED : PROTECTED);
        }

        if (Footprint[fp].SpanHor > EPSILON)
            SetTextField(m_txtPadSpanX, wxString::Format(wxT("%.3f"), Footprint[fp].SpanHor), DefEnable ? ENABLED : PROTECTED);
        if (Footprint[fp].SpanVer > EPSILON)
            SetTextField(m_txtPadSpanY, wxString::Format(wxT("%.3f"), Footprint[fp].SpanVer), DefEnable ? ENABLED : PROTECTED);

        if (Footprint[fp].DrillSize > EPSILON)
            SetTextField(m_txtDrillSize, wxString::Format(wxT("%.3f"), Footprint[fp].DrillSize), DefEnable ? ENABLED : PROTECTED);

        if (templatename.length() > 0) {
            /* fill the 3D model list */
            field = GetTemplateHeaderField(templatename, wxT("model"), SymbolMode);
            wxArrayString models = wxSplit(field, wxT(' '));
            m_choiceShape->Set(models);
            /* check which library the part comes from */
            wxString library, author;
            if (fp == 1) {
                wxString modname = GetSelection(m_listModulesRight, m_choiceModuleRight, &library, &author);
                wxASSERT(!modname.IsEmpty());
            } else {
                wxString leftmod = GetSelection(m_listModulesLeft, m_choiceModuleLeft, &library, &author);
                wxString rightmod = GetSelection(m_listModulesRight, m_choiceModuleRight, &library, &author);
                wxASSERT(leftmod.IsEmpty() || rightmod.IsEmpty());
            }
            int idx = 0;
            /* check what model was used */
            wxString vrmlpath = GetVRMLPath(library, PartData[fp]);
            if (vrmlpath.length() > 0) {
                field = wxEmptyString;
                wxString spec = GetFileHeaderField(vrmlpath, wxT("model"));
                wxString modelname = spec.BeforeFirst(wxT('{'));
                modelname.Trim(true);
                idx = m_choiceShape->FindString(modelname);
                spec = spec.AfterFirst(wxT('{')).BeforeFirst(wxT('}'));
                spec.Trim(true);
                if (spec.length() > 0) {
                    spec.Trim(false);
                    wxArrayString params = wxSplit(spec, wxT(' '));
                    if (params.Count() >= 2)
                        field = params[1];
                }
                SetTextField(m_txtShapeHeight, field, DefEnable ? ENABLED : PROTECTED);
            }
            m_choiceShape->SetSelection(idx);
            enable = (DefEnable && models.Count() > 1);
            m_choiceShape->Enable(enable);
            m_choiceShape->SetBackgroundColour(enable ? ENABLED : PROTECTED);
        }
    }

    /* check whether a body size is specified as a default parameter; if not, keep
       the body size disabled (width & length separately) */
    bool setwidth = false;
    bool setlength = false;
    if (templatename.length() > 0) {
        field = GetTemplateHeaderField(templatename, wxT("param"), SymbolMode);
        setwidth = (field.Find(wxT("@BW")) >= 0);
        setlength = (field.Find(wxT("@BL")) >= 0);
    }
    if (BodySize[fp].BodyLength > EPSILON) {
        bool enable = setlength && DefEnable;
        SetTextField(m_txtBodyLength, wxString::Format(wxT("%.3f"), BodySize[fp].BodyLength), enable ? ENABLED : PROTECTED);
    }
    if (BodySize[fp].BodyWidth > EPSILON) {
        bool enable = setwidth && DefEnable;
        SetTextField(m_txtBodyWidth, wxString::Format(wxT("%.3f"), BodySize[fp].BodyWidth), enable ? ENABLED : PROTECTED);
    }

    SetTextField(m_txtRefLabel, wxString::Format(wxT("%.2f"), LabelData[fp].RefLabelSize), DefEnable ? ENABLED : PROTECTED);
    m_chkRefLabelVisible->SetValue(LabelData[fp].RefLabelVisible);
    m_chkRefLabelVisible->Enable(DefEnable);

    SetTextField(m_txtValueLabel, wxString::Format(wxT("%.2f"), LabelData[fp].ValueLabelSize), DefEnable ? ENABLED : PROTECTED);
    m_chkValueLabelVisible->SetValue(LabelData[fp].ValueLabelVisible);
    m_chkValueLabelVisible->Enable(DefEnable);

    FieldEdited = false;    /* clear this flag for all implicit changes */
}
