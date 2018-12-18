/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  Utility functions transferring modules or footprints to/from a repository.
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
 *  $Id: remotelink.cpp 5686 2017-05-24 13:56:46Z thiadmer $
 */

#include "librarymanager.h"
#include "remotelink.h"
#include <wx/ffile.h>
#include <wx/fileconf.h>
#include <wx/filename.h>
#include <curl/curl.h>
#include <curl/easy.h>


enum {
    UNINITIALIZED = 0,
    GLOBAL_INIT,
    EASY_INIT,

    INIT_FAILED = -1,
};
static int Initialized = UNINITIALIZED;
static bool Valid = false;
static CURL* curl = 0;


bool curlInit()
{
    if (Initialized == UNINITIALIZED) {
        Valid = false;
        CURLcode code;
        #if defined _WIN32
            code = curl_global_init(CURL_GLOBAL_SSL | CURL_GLOBAL_WIN32);
        #else
            code = curl_global_init(CURL_GLOBAL_SSL);
        #endif
        Initialized = (code == CURLE_OK) ? GLOBAL_INIT : INIT_FAILED;
    }
    if (Initialized == GLOBAL_INIT) {
        curl = curl_easy_init();
        if (curl)
            Initialized = EASY_INIT;
    }
    if (Initialized == INIT_FAILED) {
        Valid = false;  /* should already be false */
        return false;
    }
    Valid = curlReset();
    return Valid;
}

bool curlReset()
{
    Valid = false;
    if (!curl)
        return false;
    curl_easy_reset(curl);

    curl_easy_setopt(curl, CURLOPT_UPLOAD, 0);
    curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_READDATA, NULL);

    /* set URL, username and password */
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field;
    wxCharBuffer postbuffer;

    field = config->Read(wxT("repository/url"));
    if (field.Length() > 0) {
        postbuffer = field.mb_str();
        curl_easy_setopt(curl, CURLOPT_URL, postbuffer.data());
        Valid = true;
    }

    field = config->Read(wxT("repository/hostuser"));
    if (field.Length() > 0) {
        postbuffer = field.mb_str();
        curl_easy_setopt(curl, CURLOPT_USERNAME, postbuffer.data());
    }

    field = config->Read(wxT("repository/hostpwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        postbuffer = field.mb_str();
        curl_easy_setopt(curl, CURLOPT_PASSWORD, postbuffer.data());
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    }
    long flags;
    config->Read(wxT("repository/hostverify"), &flags, 0x03);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (flags & 0x01) ? 1 : 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (flags & 0x02) ? 2 : 0);

    delete config;

    /* other options */
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    return true;
}

void curlCleanup(bool global)
{
    Valid = false;
    if (curl) {
        wxASSERT(Initialized == EASY_INIT);
        Initialized = GLOBAL_INIT;
        curl_easy_cleanup(curl);
        curl = 0;
    }
    if (global && Initialized >= GLOBAL_INIT) {
        curl_global_cleanup();
        Initialized = UNINITIALIZED;
    }
}

/* Converts a hex character to its integer value */
static inline char from_hex(char ch)
{
    return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static inline char to_hex(char code)
{
    static char hex[] = "0123456789abcdef";
    return hex[code & 15];
}

/* Returns a url-encoded version of the input string */
wxString URLEncode(const wxString& string)
{
    char *buf = (char*)malloc(string.length() * 3 + 1); /* absolute worst case */
    if (!buf)
        return wxEmptyString;
    char *pbuf = buf;
    wxCharBuffer source = string.mb_str(wxConvUTF8);
    for (const char *pstr = source.data(); *pstr; pstr++) {
        if (*pstr >= 0 && *pstr <= 0x7f
            && (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~'))
        {
            *pbuf++ = *pstr;
        } else if (*pstr == ' ') {
                *pbuf++ = '+';
        } else {
            *pbuf++ = '%';
            *pbuf++ = to_hex(*pstr >> 4);
            *pbuf++ = to_hex(*pstr & 15);
        }
    }
    *pbuf = '\0';
    wxString result = wxString::FromAscii(buf);
    free((void*)buf);
    return result;
}

/* Returns a url-decoded version of the input string */
wxString URLDecode(const wxString& string)
{
    wxCharBuffer source = string.mb_str();
    const char *pstr = source.data();
    char *buf = (char*)malloc(strlen(pstr) + 1);
    if (!buf)
        return wxEmptyString;
    char *pbuf = buf;
    while (*pstr) {
        if (*pstr == '%') {
            if (pstr[1] && pstr[2]) {
                *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
                pstr += 2;
            }
        } else if (*pstr == '+') {
            *pbuf++ = ' ';
        } else {
            *pbuf++ = *pstr;
        }
        pstr++;
    }
    *pbuf = '\0';
    wxString result = wxString::FromAscii(buf);
    free((void*)buf);
    return result;
}

static wxString Base64Encode(const unsigned char* buffer, size_t size, bool linefeeds)
{
    const static wxChar Lookup[] = wxT("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
    const static wxChar Padding = wxT('=');

    wxString encoded;
    encoded.Alloc((size / 3 + (size % 3 > 0)) * 4);

    unsigned temp;
    size_t idx;
    for (idx = 0; idx < size / 3; idx++) {
        if (linefeeds && idx > 0 && idx % 16 == 0)
            encoded += wxT("\r\n"); /* add a line break after 16 byte triplets */
        temp  = (unsigned)(*buffer++) << 16;    /* Convert to big endian */
        temp += (unsigned)(*buffer++) << 8;
        temp += (unsigned)(*buffer++);
        encoded += Lookup[(temp & 0x00FC0000) >> 18];
        encoded += Lookup[(temp & 0x0003F000) >> 12];
        encoded += Lookup[(temp & 0x00000FC0) >> 6 ];
        encoded += Lookup[(temp & 0x0000003F)      ];
    }
    if (linefeeds && idx > 0 && idx % 16 == 0)
        encoded += wxT("\r\n");
    switch (size % 3) {
    case 1:
        temp  = (unsigned)(*buffer++) << 16;
        encoded += Lookup[(temp & 0x00FC0000) >> 18];
        encoded += Lookup[(temp & 0x0003F000) >> 12];
        encoded += Padding;
        encoded += Padding;
        break;
    case 2:
        temp  = (unsigned)(*buffer++) << 16;
        temp += (unsigned)(*buffer++) << 8;
        encoded += Lookup[(temp & 0x00FC0000) >> 18];
        encoded += Lookup[(temp & 0x0003F000) >> 12];
        encoded += Lookup[(temp & 0x00000FC0) >> 6 ];
        encoded += Padding;
        break;
    }
    return encoded;
}

wxString Scramble(const wxString& source)
{
    /* this is ROT47 */
    wxString result = source;
    for (size_t idx = 0; idx < source.length(); idx++) {
        if (result[idx] >= wxT('!') && result[idx] <= wxT('O'))
            result[idx] = (((int)result[idx] + 47) % 127);
        else if (result[idx] >= wxT('P') && result[idx] <= wxT('~'))
            result[idx] = (((int)result[idx] - 47) % 127);
    }
    return result;
}

static size_t write_data(void *buffer, size_t size, size_t nmemb, void *userp)
{
    wxString* data = (wxString*)userp;
    wxString buf = wxString::FromUTF8((const char*)buffer, size * nmemb);
    data->Append(buf);
    return size * nmemb;
}

/** The URL and other parameters must be passed in explicitly; this routine is
 *  called before the settings are final.
 *
 *  \return wxEmptyString on success, error message on failure.
 */
wxString curlAddUser(const wxString& url, const wxString& user, const wxString& email,
                     const wxString& hostuser, const wxString& hostpwd, long flags)
{
    wxASSERT(url.length() > 0 && user.length() > 0 && email.length() > 0);
    if (!curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    wxCharBuffer postbuffer;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    postbuffer = url.mb_str();
    curl_easy_setopt(curl, CURLOPT_URL, postbuffer.data());
    if (hostuser.length() > 0) {
        postbuffer = hostuser.mb_str();
        curl_easy_setopt(curl, CURLOPT_USERNAME, postbuffer.data());
    }
    if (hostpwd.Length() > 0) {
        postbuffer = hostpwd.mb_str();
        curl_easy_setopt(curl, CURLOPT_PASSWORD, postbuffer.data());
        curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
    }
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, (flags & 0x01) ? 1 : 0);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, (flags & 0x02) ? 2 : 0);

    wxString post = wxT("cmd=user&user=") + URLEncode(user) + wxT("&email=") + URLEncode(email);
    //??? also set password, for those repositories where the user can choose his/her password
    postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_SSL_CACERT)
        return wxT("Host certificate cannot be authenticated with known CA certificates");
    else if (code != CURLE_OK)
        return wxT("Connection failure");

    data.Trim();
    data.Trim(false);
    if (data.CmpNoCase(wxT("ok")) == 0)
        return wxEmptyString;
    return data;
}

wxString curlList(wxArrayString* list, const wxString& category, const wxString& filter)
{
    if (!Valid && !curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    wxString post = wxT("cmd=list&cat=") + category;
    if (filter.Length() > 0)
        post += wxT("&filter=") + URLEncode(filter);
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field = config->Read(wxT("repository/user"));
    if (field.Length() > 0)
        post += wxT("&user=") + URLEncode(field);
    field = config->Read(wxT("repository/pwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        post += wxT("&pwd=") + URLEncode(field);
    }
    delete config;
    wxCharBuffer postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code == CURLE_SSL_CACERT)
        return wxT("Host certificate cannot be authenticated with known CA certificates");
    else if (code != CURLE_OK)
        return wxT("Connection failure");

    /* parse the returned string (URL-decode fields) */
    data.Trim();
    while (data.Length() > 0) {
        int pos = data.Find(wxT('\n'));
        wxString line;
        if (pos >= 0) {
            line = data.Left(pos);
            data = data.Mid(pos + 1);
        } else {
            line = data;
            data.Clear();
        }
        /* get first field */
        line.Trim(false);
        wxString part;
        if (line[0] == wxT('"')) {
            line = line.Mid(1);         /* skip first '"' */
            pos = line.Find(wxT('"'));  /* find next '"' */
            if (pos < 0)
                break;  /* invalid format */
            part = line.Left(pos);
            line = line.Mid(pos + 1);
            pos = line.Find(wxT(','));  /* find ',' */
            if (pos < 0)
                break;  /* invalid format */
            line = line.Mid(pos + 1);
        } else {
            pos = line.Find(wxT(','));  /* find ',' */
            if (pos < 0)
                break;  /* invalid format */
            part = line.Left(pos);
            line = line.Mid(pos + 1);
            part.Trim();
        }
        /* get second field */
        line.Trim(false);
        wxString name;
        if (line[0] == wxT('"')) {
            line = line.Mid(1);         /* skip first '"' */
            pos = line.Find(wxT('"'));  /* find next '"' */
            if (pos < 0)
                break;  /* invalid format */
            name = line.Left(pos);
        } else {
            name = line;
            name.Trim();
        }
        list->Add(part + wxT("\t") + name);
    }

    if (data.length() > 0)
        return data;    /* likely an error message (instead of a part listing) */
    return wxEmptyString;
}

/** curlGet() retrieves a part from the repository.
 *  If the author name is empty, the current user will be assumed.
 *  If the module parameter is NULL, the function will only return whether the symbol exists.
 */
wxString curlGet(const wxString& partname, const wxString& author, const wxString& category, wxArrayString* module)
{
    if (!Valid && !curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    wxString post = wxT("cmd=get&cat=") + category + wxT("&part=") + URLEncode(partname);
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field = config->Read(wxT("repository/user"));
    if (author.length() == 0)
        post += wxT("&author=") + URLEncode(field);
    else
        post += wxT("&author=") + URLEncode(author);
    post += wxT("&user=") + URLEncode(field);
    field = config->Read(wxT("repository/pwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        post += wxT("&pwd=") + URLEncode(field);
    }
    delete config;

    wxCharBuffer postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        return wxT("Connection failure");

    data.Trim();
    data.Trim(false);
    if (data.length() == 0)
        return wxT("Not found");
    if (data.Left(6).CmpNoCase(wxT("ERROR:")) == 0)
        return data;

    wxString copydata = data;   /* save */
    int count = 0;
    while (data.length() > 0) {
        count++;
        int pos = data.Find(wxT('\n'));
        wxString line;
        if (pos >= 0) {
            line = data.Left(pos);
            data = data.Mid(pos + 1);
        } else {
            line = data;
            data.Clear();
        }
        if (module) {
            line.Trim();
            module->Add(line);
        }
    }
    if (count <= 2)
        return copydata;    /* only 1 or 2 lines in the part definition, this must be an error message */
    return wxEmptyString;
}

/** curlPut() stores a part in the repository.
 *  The author of the part is implicitly set to the user name.
 */
wxString curlPut(const wxString& partname, const wxString& category, const wxArrayString& module)
{
    if (!Valid && !curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    wxString post = wxT("cmd=put&cat=") + category + wxT("&part=") + URLEncode(partname);
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field = config->Read(wxT("repository/user"));
    post += wxT("&author=") + URLEncode(field) + wxT("&user=") + URLEncode(field);
    field = config->Read(wxT("repository/pwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        post += wxT("&pwd=") + URLEncode(field);
    }
    delete config;

    field.Clear();
    for (size_t idx = 0; idx < module.Count(); idx++) {
        if (idx > 0)
            field += wxT("\n");
        field += module[idx];
    }
    post += wxT("&data=") + URLEncode(field);

    wxCharBuffer postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        return wxT("Connection failure");

    data.Trim();
    data.Trim(false);
    if (data.CmpNoCase(wxT("ok")) == 0)
        return wxEmptyString;
    return data;
}

/** curlPutInfo() updates a part in the repository.
 *  The author of the part is implicitly set to the user name.
 */
wxString curlPutInfo(const wxString& partname, const wxString& category, const wxString& fields,
                     const wxString& imagefile)
{
    if (!Valid && !curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    wxString post = wxT("cmd=putinfo&cat=") + category + wxT("&part=") + URLEncode(partname);
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field = config->Read(wxT("repository/user"));
    post += wxT("&author=") + URLEncode(field) + wxT("&user=") + URLEncode(field);
    field = config->Read(wxT("repository/pwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        post += wxT("&pwd=") + URLEncode(field);
    }
    delete config;

    post += wxT("&") + fields;

    if (imagefile.Length() > 0) {
        wxFFile imgdata(imagefile, wxT("rb"));
        if (imgdata.IsOpened()) {
            wxFileOffset length = imgdata.Length();
            unsigned char *buffer = new unsigned char[length];
            imgdata.Read(buffer, length);
            imgdata.Close();
            wxFileName fname(imagefile);
            post += wxT("&thumb=data:image/") + fname.GetExt().MakeLower() + wxT(";base64,")
                    + Base64Encode(buffer, length, false);
            delete[] buffer;
        }
    }

    wxCharBuffer postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        return wxT("Connection failure");

    data.Trim();
    data.Trim(false);
    if (data.CmpNoCase(wxT("ok")) == 0)
        return wxEmptyString;
    return data;
}

/** curlDelete() deletes a part from the repository.
 *  The author of the part is implicitly set to the user name.
 */
wxString curlDelete(const wxString& partname, const wxString& category)
{
    if (!Valid && !curlInit())
        return wxT("Incorrect configuration for network or repository");
    wxASSERT(curl);

    wxString data;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);

    wxString post = wxT("cmd=del&cat=") + category + wxT("&part=") + URLEncode(partname);
    wxFileConfig *config = new wxFileConfig(APP_NAME, VENDOR_NAME, theApp->GetINIPath());
    wxString field = config->Read(wxT("repository/user"));
    post += wxT("&author=") + URLEncode(field) + wxT("&user=") + URLEncode(field);
    field = config->Read(wxT("repository/pwd"));
    if (field.Length() > 0) {
        field = Scramble(field);    /* this un-scrambles the field */
        post += wxT("&pwd=") + URLEncode(field);
    }
    delete config;

    wxCharBuffer postbuffer = post.mb_str();
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postbuffer.data());
    CURLcode code = curl_easy_perform(curl);
    if (code != CURLE_OK)
        return wxT("Connection failure");

    data.Trim();
    data.Trim(false);
    if (data.CmpNoCase(wxT("ok")) == 0)
        return wxEmptyString;
    return data;
}

