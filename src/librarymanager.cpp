/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  This file is just the definition of the application.
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
 *  $Id: librarymanager.cpp 5387 2015-10-22 19:31:30Z thiadmer $
 */
#include "librarymanager.h"
#include "libmngr_frame.h"
#include <wx/filefn.h>
#include <wx/stdpaths.h>

IMPLEMENT_APP(LibraryManagerApp)

LibraryManagerApp* theApp;

bool LibraryManagerApp::OnInit()
{
    #if defined _MSC_VER
        _CrtCheckMemory();
    #endif

    theApp = this;

    /* get the ./bin path (this is the easiest one) */
    strBinPath = wxStandardPaths::Get().GetExecutablePath();
    strBinPath = wxPathOnly(strBinPath);

    /* strip off the last bit, to get the root */
    if (strBinPath.Right(4).CmpNoCase(wxT(DIRSEP_STR) wxT("bin")) == 0)
      strRootPath = strBinPath.Left(strBinPath.length() - 4); /* strip off ./bin */
    else
      strRootPath = strBinPath; /* not installed in ./bin, everything must be below the installation directory */

    strTemplates = strRootPath + wxT(DIRSEP_STR) wxT("template");
    strDocumentationPath = strRootPath + wxT(DIRSEP_STR) wxT("doc");
    strFontFile = strRootPath + wxT(DIRSEP_STR) wxT("font") wxT(DIRSEP_STR) wxT("newstroke.cxf");

    /* get the path for storing user data */
    strUserDataPath = wxStandardPaths::Get().GetUserDataDir();
    if (!wxDirExists(strUserDataPath)) {
        #if defined _WIN32
            wxMkDir(strUserDataPath);
        #else
            wxMkDir(strUserDataPath.utf8_str(), 0777);
        #endif
    }

    /* try to locate the INI file in the "root" of KiCad Librarian */
    strINIPath = strRootPath + wxT(DIRSEP_STR) APP_NAME wxT(".ini");
    if (!wxFileName::FileExists(strINIPath))
        strINIPath = strUserDataPath + wxT(DIRSEP_STR) APP_NAME wxT(".ini");

    wxImage::AddHandler(new wxPNGHandler);
    libmngrFrame *frame = new libmngrFrame(NULL);
    frame->Show(true);
    return true;
}
