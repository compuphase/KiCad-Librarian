/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  Utility functions for parsing and writing libraries.
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
 *  $Id: libraryfunctions.cpp 5692 2017-06-11 19:23:07Z thiadmer $
 */

#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/regex.h>
#include <wx/string.h>
#include <wx/textfile.h>
#include <math.h>
#include "librarymanager.h"
#include "libraryfunctions.h"
#if !defined NO_CURL
    #include "remotelink.h"
#endif

#define EoC     '\x1a'  /* special "end-of-comment" character */
#define MM(d)       (d)
#define MIL10(d)    (long)floor(d / 0.00254 + 0.5)

#if !defined sizearray
    #define sizearray(a)    (sizeof(a) / sizeof((a)[0]))
#endif
#if !defined min
    #define min(a, b)       ((a) < (b) ? (a) : (b))
#endif

#include <wx/arrimpl.cpp>
WX_DEFINE_OBJARRAY(ArrayCoordSize);


/* a syntax changed for version 2.9 and higher; to also (still) support
   wxWidgets 2.8, a helper function is conditionally compiled */
#if wxCHECK_VERSION(2, 9, 0)
    inline void str_replace(wxString& string, size_t start, size_t length, const wxString& sub)
    {
        string.replace(start, length, sub);
    }
#else
    inline void str_replace(wxString& string, size_t start, size_t length, const wxString& sub)
    {
        string.replace(start, length, sub.c_str());
    }
#endif


/* strips trailing zeros from a number; the input string should only contain a number */
static void StripTrailingZeros(wxString* string)
{
    /* if there is no decimal period, we are done quickly */
    if (string->Find(wxT('.'), true) < 0)
        return;
    int l = (int)string->length();
    wxASSERT(l > 0);    /* must be, because we found a period */
    while ((*string)[l - 1] == wxT('0'))
        l -= 1;
    wxASSERT(l > 0);    /* must be, because we would drop in the period eventually */
    if ((*string)[l - 1] == wxT('.'))
        l -= 1;
    if (l > 0)
        *string = string->Left(l);
    else
        *string = wxT("0"); /* this can only occur if the input is ".000" */
}

wxString GetToken(wxString* string)
{
    if (string->length() == 0)
        return wxEmptyString;

    wxString token;
    int index;
    bool stripquotes = false;

    if ((*string)[0] == wxT('"')) {
        *string = string->Mid(1);
        index = string->Find(wxT('"'));
        stripquotes = true;
    } else {
        index = string->Find(wxT(' '));
    }

    if (index < 0) {
        token = *string;
        string->Clear();
    } else {
        token = string->Left(index);
        *string = string->Mid(index);
    }

    if (stripquotes && (*string)[0] == wxT('"'))
        *string = string->Mid(1);
    string->Trim(false);    /* remove leading white-space */

    return token;
}

long GetTokenLong(wxString* string)
{
    wxString token;

    int index = string->Find(wxT(' '));
    if (index < 0) {
        token = *string;
        string->Clear();
    } else {
        token = string->Left(index);
        *string = string->Mid(index);
        string->Trim(false);    /* remove leading white-space */
    }

    long val;
    if (!token.ToLong(&val))
        val = 0;
    return val;
}

/** GetTokenDouble() returns a floating-point value in mm.
 */
double GetTokenDouble(wxString* string)
{
    wxString token;

    int index = string->Find(wxT(' '));
    if (index < 0) {
        token = *string;
        string->Clear();
    } else {
        token = string->Left(index);
        *string = string->Mid(index);
        string->Trim(false);    /* remove leading white-space */
    }

    double val;
    if (!token.ToDouble(&val))
        val = 0;
    return val;
}

/** GetTokenDim() returns a value in mm. If the library is in decimil format,
 *  the value is converted.
 */
double GetTokenDim(wxString* string, bool unit_mm)
{
    wxString token;

    int index = string->Find(wxT(' '));
    if (index < 0) {
        token = *string;
        string->Clear();
    } else {
        token = string->Left(index);
        *string = string->Mid(index);
        string->Trim(false);    /* remove leading white-space */
    }

    double val;
    if (unit_mm) {
        /* the value is in floating point */
        token.ToDouble(&val);
    } else {
        long t;
        token.ToLong(&t);
        val = (double)t * 0.00254;
    }
    return val;
}

/** GetSection() gets a subsection of an s-expression in the string, that
 *  starts with the token. It returns the parameters of the section, without
 *  the parentheses and without the token name.
 *  The token parameter should not include the opening parenthesis.
 */
wxString GetSection(const wxString& string, const wxString& token, int skip)
{
    wxASSERT(token[0] != wxT('('));
    size_t tokenlen = token.length();
    int level = 0;
    bool instring = false;
    unsigned pos;
    for (pos = 0; pos < string.length(); pos++) {
        if (string[pos] == wxT('(') && !instring && level == 0) {
            wxString word = string.Mid(pos + 1, tokenlen);
            if (word.CmpNoCase(token) == 0 && string[pos + tokenlen + 1] == wxT(' ')) {
                if (skip == 0)
                    break;
                skip--;
            }
        }
        if (string[pos] == wxT('(') && !instring)
            level++;
        else if (string[pos] == wxT(')') && !instring)
            level--;
        else if (string[pos] == wxT('"'))
            instring = !instring;
        else if (string[pos] == wxT('\\') && string[pos] == wxT('"'))
            pos++;  /* skip both \ and " */
    }
    if (pos >= (int)string.length())
        return wxEmptyString;
    wxASSERT(string[pos] == wxT('('));
    pos += (int)token.length() + 1; /* +1 for the '(' */
    wxASSERT(string[pos] == wxT(' '));
    level = 0;
    instring = false;
    unsigned idx;
    for (idx = pos; idx < string.length() && level >= 0; idx++) {
        if (string[idx] == wxT('(') && !instring)
            level++;
        else if (string[idx] == wxT(')') && !instring)
            level--;
        else if (string[idx] == wxT('"'))
            instring = !instring;
        else if (string[idx] == wxT('\\') && string[idx] == wxT('"'))
            idx++;  /* skip both \ and " */
    }
    wxString params = string.Mid(pos, idx - pos - 1);
    params.Trim(false);
    params.Trim(true);
    return params;
}

/** SetSection() replaces the parameters in a section of an s-expression.
 *  The token parameter should not include the opening parenthesis.
 */
bool SetSection(wxString& string, const wxString& token, const wxString& params)
{
    wxASSERT(token[0] != wxT('('));
    int pos = string.Find(wxT("(") + token + wxT(" "));
    if (pos < 0)
        return false;
    wxASSERT(pos < (int)string.length());
    wxASSERT(string[pos] == wxT('('));
    pos += (int)token.length() + 1; /* +1 for the '(' */
    wxASSERT(string[pos] == wxT(' '));
    while (pos < (int)string.length() && string[pos] == wxT(' '))
        pos++;
    bool instring = false;
    int level = 0;
    unsigned idx;
    for (idx = pos; idx < string.length() && level >= 0; idx++) {
        if (string[idx] == wxT('(') && !instring)
            level++;
        else if (string[idx] == wxT(')') && !instring)
            level--;
        else if (string[idx] == wxT('"'))
            instring = !instring;
        else if (string[idx] == wxT('\\') && string[idx] == wxT('"'))
            idx++;  /* skip both \ and " */
    }
    string = string.erase(pos, idx - pos - 1);
    string = string.insert(pos, params);
    return true;
}

/** DeleteSection() removes a section in an s-expression.
 *  The token parameter should not include the opening parenthesis.
 */
bool DeleteSection(wxString& string, const wxString& token)
{
    wxASSERT(token[0] != wxT('('));
    int pos = string.Find(wxT("(") + token + wxT(" "));
    if (pos < 0)
        return false;
    wxASSERT(pos < (int)string.length());
    wxASSERT(string[pos] == wxT('('));
    unsigned idx = pos + token.length() + 1;    /* +1 for the '(' */
    wxASSERT(string[idx] == wxT(' '));
    while (idx < (int)string.length() && string[idx] == wxT(' '))
        idx++;
    bool instring = false;
    int level = 0;
    while (idx < string.length() && level >= 0) {
        if (string[idx] == wxT('(') && !instring)
            level++;
        else if (string[idx] == wxT(')') && !instring)
            level--;
        else if (string[idx] == wxT('"'))
            instring = !instring;
        else if (string[idx] == wxT('\\') && string[idx] == wxT('"'))
            idx++;  /* skip both \ and " */
        idx++;
    }
    string = string.erase(pos, idx - pos - 1);
    return true;
}

double NormalizeAngle(double angle)
{
    while (angle < 0.0)
        angle += 360.0;
    while (angle >= 360.0)
        angle -= 360.0;
    return angle;
}

long NormalizeAngle(long angle)
{
    while (angle < 0)
        angle += 3600;
    while (angle >= 3600)
        angle -= 3600;
    return angle;
}

static long GetPadNumber(const wxString& field, int* width, int* height)
{
    if (field.Length() == 0)
        return PIN_ANOMYM;

    wxASSERT(width != NULL && height != NULL);
    long pinnr = 0;
    /* most footprints have numeric pin numbers, but BGA and PGA use a letter
       followed by digits; in this case, the matrix size must be determined
       as well */
    if (isalpha(field[0]) && isdigit(field[1])) {
        long row = toupper(field[0]) - 'A' + 1;
        if (row > 9)
            row--;  /* because 'I' is not used for grid arrays */
        if (row > 14)
            row--;  /* because 'O' is not used for grid arrays */
        if (row > 15)
            row--;  /* because 'Q' is not used for grid arrays */
        if (row > 16)
            row--;  /* because 'S' is not used for grid arrays */
        if (row > 20)
            row--;  /* because 'X' is not used for grid arrays */
        if (row > 21)
            row--;  /* because 'Z' is not used for grid arrays */
        wxString sub = field.Mid(1);
        long col;
        sub.ToLong(&col);
        if (col > *width)
            *width = col;
        if (row > *height)
            *height = row;
        pinnr = *height * (row - 1) + col;
    } else {
        field.ToLong(&pinnr);
    }
    return pinnr;
}

/** TranslateUnits() translates units between mm and 1/10th mil, but only
 *  for the legacy module format.
 */
bool TranslateUnits(wxArrayString& module, bool from_mm, bool to_mm)
{
    if ((from_mm && to_mm) || (!from_mm && !to_mm))
        return false;   /* fom_mm and to_mm are both true or both false -> nothing to do */

    bool inpad = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.length() == 2 && keyword[0] == wxT('T') && isdigit(keyword[1])) {
            double xpos = GetTokenDim(&line, from_mm);
            double ypos = GetTokenDim(&line, from_mm);
            double xsize = GetTokenDim(&line, from_mm);
            double ysize = GetTokenDim(&line, from_mm);
            long rot = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, from_mm);
            wxString mflag = GetToken(&line);
            wxString visflag = GetToken(&line);
            long layer = GetTokenLong(&line);
            /* the "italic" flag may be absent and it may be "glued" to the text field */
            wxString iflag, text;
            if (line[0] == '"' || (line.length() > 1 && line[1] == '"')) {
                /* the italic flag is absent or it is glued to the text (no space between the fields) */
                if (line[0] != '"') {
                    iflag = iflag.Left(1);
                    line = line.Mid(1);
                } else {
                    iflag = wxT("N");       /* assume "not italic", when absent */
                }
                wxASSERT(line[0] == '"');
                text = GetToken(&line);     /* this strips off the double quotes */
            } else {
                iflag = GetToken(&line);
                text = GetToken(&line);
            }
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f %.4f %.4f %.4f %ld %.4f %s %s %ld %s \"%s\""),
                                               keyword.c_str(), MM(xpos), MM(ypos), MM(xsize), MM(ysize),
                                               rot, MM(penwidth), mflag.c_str(), visflag.c_str(),
                                               layer, iflag.c_str(), text.c_str());
            else
                module[idx] = wxString::Format(wxT("%s %ld %ld %ld %ld %ld %ld %s %s %ld %s \"%s\""),
                                               keyword.c_str(), MIL10(xpos), MIL10(ypos), MIL10(xsize), MIL10(ysize),
                                               rot, MIL10(penwidth),  mflag.c_str(), visflag.c_str(),
                                               layer, iflag.c_str(), text.c_str());
        } else if (keyword.CmpNoCase(wxT("DS")) == 0 || keyword.CmpNoCase(wxT("DC")) == 0) {
            double x1 = GetTokenDim(&line, from_mm);
            double y1 = GetTokenDim(&line, from_mm);
            double x2 = GetTokenDim(&line, from_mm);
            double y2 = GetTokenDim(&line, from_mm);
            double penwidth = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f %.4f %.4f %.4f %.4f %s"),
                                               keyword.c_str(), MM(x1), MM(y1), MM(x2), MM(y2),
                                               MM(penwidth), line.c_str());
            else
                module[idx] = wxString::Format(wxT("%s %ld %ld %ld %ld %ld %s"),
                                               keyword.c_str(), MIL10(x1), MIL10(y1), MIL10(x2), MIL10(y2),
                                               MIL10(penwidth), line.c_str());
        } else if (keyword.CmpNoCase(wxT("DA")) == 0) {
            double x1 = GetTokenDim(&line, from_mm);
            double y1 = GetTokenDim(&line, from_mm);
            double x2 = GetTokenDim(&line, from_mm);
            double y2 = GetTokenDim(&line, from_mm);
            long angle = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f %.4f %.4f %.4f %ld %.4f %s"),
                                               keyword.c_str(), MM(x1), MM(y1), MM(x2), MM(y2),
                                               angle, MM(penwidth), line.c_str());
            else
                module[idx] = wxString::Format(wxT("%s %ld %ld %ld %ld %ld %ld %s"),
                                               keyword.c_str(), MIL10(x1), MIL10(y1), MIL10(x2), MIL10(y2),
                                               angle, MIL10(penwidth), line.c_str());
        } else if (keyword.CmpNoCase(wxT("DP")) == 0) {
            long rsv1 = GetTokenLong(&line);
            long rsv2 = GetTokenLong(&line);
            long rsv3 = GetTokenLong(&line);
            long rsv4 = GetTokenLong(&line);
            long count = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %ld %ld %ld %ld %ld %.4f %s"),
                                               keyword.c_str(), rsv1, rsv2, rsv3, rsv4,
                                               count, MM(penwidth), line.c_str());
            else
                module[idx] = wxString::Format(wxT("%s %ld %ld %ld %ld %ld %ld %s"),
                                               keyword.c_str(), rsv1, rsv2, rsv3, rsv4,
                                               count, MIL10(penwidth), line.c_str());
        } else if (keyword.CmpNoCase(wxT("Dl")) == 0) {
            double xpos = GetTokenDim(&line, from_mm);
            double ypos = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(ypos));
            else
                module[idx] = wxString::Format(wxT("%s %ld %ld"), keyword.c_str(), MIL10(xpos), MIL10(ypos));
        } else if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            inpad = false;
        } else if (keyword.CmpNoCase(wxT("Po")) == 0) {
            if (inpad) {
                double xpos = GetTokenDim(&line, from_mm);
                double ypos = GetTokenDim(&line, from_mm);
                if (to_mm)
                    module[idx] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(ypos));
                else
                    module[idx] = wxString::Format(wxT("%s %ld %ld"), keyword.c_str(), MIL10(xpos), MIL10(ypos));
            } else {
                /* the "Po" token must also be translated at the module level, as it
                   may have a non-zero position or angle; we do not care about the
                     position, but the angle must be preserved */
                GetToken(&line);
                GetToken(&line);
                long angle = GetTokenLong(&line);
                module[idx] = wxString::Format(wxT("%s 0 0 %ld %s"), keyword.c_str(), angle, line.c_str());
            }
        } else if (keyword.CmpNoCase(wxT("Sh")) == 0 && inpad) {
            wxString name = GetToken(&line);
            wxString type = GetToken(&line);
            double width = GetTokenDim(&line, from_mm);
            double height = GetTokenDim(&line, from_mm);
            double xdelta = GetTokenDim(&line, from_mm);
            double ydelta = GetTokenDim(&line, from_mm);
            long rot = GetTokenLong(&line);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s \"%s\" %s %.4f %.4f %.4f %.4f %ld"),
                                               keyword.c_str(), name.c_str(), type.c_str(),
                                               MM(width), MM(height), MM(xdelta), MM(ydelta), rot);
            else
                module[idx] = wxString::Format(wxT("%s \"%s\" %s %ld %ld %ld %ld %ld"),
                                               keyword.c_str(), name.c_str(), type.c_str(),
                                               MIL10(width), MIL10(height), MIL10(xdelta), MIL10(ydelta), rot);
        } else if (keyword.CmpNoCase(wxT("Dr")) == 0 && inpad) {
            double size = GetTokenDim(&line, from_mm);
            double xpos = GetTokenDim(&line, from_mm);
            double ypos = GetTokenDim(&line, from_mm);
            if (line.length() > 0) {
                /* slotted hole */
                wxString type = GetToken(&line);
                double xend = GetTokenDim(&line, from_mm);
                double yend = GetTokenDim(&line, from_mm);
                if (to_mm)
                    module[idx] = wxString::Format(wxT("%s %.4f %.4f %.4f %s %.4f %.4f"),
                                                   keyword.c_str(), MM(size), MM(xpos), MM(ypos),
                                                   type.c_str(), MM(xend), MM(yend));
                else
                    module[idx] = wxString::Format(wxT("%s %ld %ld %ld %s %ld %ld"),
                                                   keyword.c_str(), MIL10(size), MIL10(xpos), MIL10(ypos),
                                                   type.c_str(), MIL10(xend), MIL10(yend));
            } else {
                if (to_mm)
                    module[idx] = wxString::Format(wxT("%s %.4f %.4f %.4f"),
                                                   keyword.c_str(), MM(size), MM(xpos), MM(ypos));
                else
                    module[idx] = wxString::Format(wxT("%s %ld %ld %ld"),
                                                   keyword.c_str(), MIL10(size), MIL10(xpos), MIL10(ypos));
            }
        } else if (keyword.CmpNoCase(wxT("Le")) == 0 && inpad) {
            /* length die */
            double size = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f"), keyword.c_str(), MM(size));
            else
                module[idx] = wxString::Format(wxT("%s %ld"), keyword.c_str(), MIL10(size));
        } else if (keyword.CmpNoCase(wxT(".SolderPaste")) == 0
                   || keyword.CmpNoCase(wxT(".SolderMask")) == 0
                   || keyword.CmpNoCase(wxT(".LocalClearance")) == 0
                   || keyword.CmpNoCase(wxT(".ThermalWidth")) == 0
                   || keyword.CmpNoCase(wxT(".ThermalGap")) == 0)
        {
            wxASSERT(inpad);
            double size = GetTokenDim(&line, from_mm);
            if (to_mm)
                module[idx] = wxString::Format(wxT("%s %.4f"), keyword.c_str(), MM(size));
            else
                module[idx] = wxString::Format(wxT("%s %ld"), keyword.c_str(), MIL10(size));
        }
    }
    return true;
}

static struct {
    long number;
    const wxChar* name;
} LayerMap[] = {
    { 0, wxT("B.Cu") },
    { 1, wxT("Inner1.Cu") },
    { 2, wxT("Inner2.Cu") },
    { 3, wxT("Inner3.Cu") },
    { 4, wxT("Inner4.Cu") },
    { 5, wxT("Inner5.Cu") },
    { 6, wxT("Inner6.Cu") },
    { 7, wxT("Inner7.Cu") },
    { 8, wxT("Inner8.Cu") },
    { 9, wxT("Inner9.Cu") },
    { 10, wxT("Inner10.Cu") },
    { 11, wxT("Inner11.Cu") },
    { 12, wxT("Inner12.Cu") },
    { 13, wxT("Inner13.Cu") },
    { 14, wxT("Inner14.Cu") },
    { 15, wxT("F.Cu") },
    { 16, wxT("B.Adhes") },
    { 17, wxT("F.Adhes") },
    { 18, wxT("B.Paste") },
    { 19, wxT("F.Paste") },
    { 20, wxT("B.SilkS") },
    { 21, wxT("F.SilkS") },
    { 22, wxT("B.Mask") },
    { 23, wxT("F.Mask") },
    { 24, wxT("Dwgs.User") },
    { 25, wxT("Cmts.User") },
    { 26, wxT("Eco1.User") },
    { 27, wxT("Eco2.User") },
    { 28, wxT("Edge.Cuts") },
    { 29, wxT("Margin") },
    { 30, wxT("F.CrtYd") },
    { 31, wxT("B.CrtYd") },
    { 32, wxT("F.Fab") },
    { 33, wxT("B.Fab") },
};

static const wxChar* LayerName(int index)
{
    for (unsigned i = 0; i < sizearray(LayerMap); i++) {
        if (LayerMap[i].number == index)
            return LayerMap[i].name;
    }
    wxASSERT(false);
    return wxT("unknown");
}

static long LayerNumber(const wxString& name)
{
    for (unsigned i = 0; i < sizearray(LayerMap); i++) {
        if (name.CmpNoCase(LayerMap[i].name) == 0)
            return LayerMap[i].number;
    }
    wxASSERT(false);
    return -1;
}

/** TranslateToSexpr() translates a legacy module to s-expression. The legacy
 *  module must already use mm.
 */
void TranslateToSexpr(wxArrayString* output, const wxArrayString& module)
{
    wxASSERT(output != 0);
    output->Clear();
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$MODULE")) == 0) {
            wxString name = GetToken(&line);
            /* find extra information to add to this header line */
            int layer = -1, locked = -1, placed = -1;
            unsigned long timestamp = 0;
            for (unsigned i = idx + 1; i < module.Count(); i++) {
                line = module[i];
                keyword = GetToken(&line);
                if (keyword.CmpNoCase(wxT("Po")) == 0) {
                    GetToken(&line);    /* ignore xpos */
                    GetToken(&line);    /* ignore ypos */
                    GetToken(&line);    /* ignore rotation */
                    layer = GetTokenLong(&line);
                    wxString field = GetToken(&line);
                    field.ToULong(&timestamp, 16);
                    GetToken(&line);    /* ignore timestamp 2 */
                    locked = (line[0] == wxT('F'));
                    placed = (line[1] == wxT('P'));
                } else if (keyword[0] == wxT('$')) {
                    break;
                }
            }
            wxString newline;
            newline = wxString::Format(wxT("(module \"%s\""), name.c_str());
            if (layer >= 0)
                newline += wxString::Format(wxT(" (layer %s)"), LayerName(layer));
            if (locked > 0)
                newline += wxT(" locked");
            if (placed > 0)
                newline += wxT(" placed");
            output->Add(newline);
            if (timestamp > 0) {
                newline = wxString::Format(wxT("(tedit %X)"), timestamp);
                output->Add(newline);
            }
        } else if (keyword.CmpNoCase(wxT("$EndMODULE")) == 0) {
            output->Add(wxT(")"));
        } else if (keyword.CmpNoCase(wxT("AR")) == 0 && line.length() > 0) {
            output->Insert(wxT("#template ") + line, 0);
        } else if (keyword.CmpNoCase(wxT("Po")) == 0) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            long angle = GetTokenLong(&line);
            /* the "module position" is redundant for library modules, so we only copy
               it to the output when it is non-zero */
            if (!Equal(xpos, 0.0) || !Equal(ypos, 0.0) || angle != 0) {
                    wxString newline;
                    if (angle == 0)
                        newline = wxString::Format(wxT("(at %.4f %.4f)"), xpos, ypos);
                    else
                        newline = wxString::Format(wxT("(at %.4f %.4f %ld)"), xpos, ypos, angle);
                    output->Add(newline);
            }
        } else if (keyword.CmpNoCase(wxT("At")) == 0) {
            wxString type = GetToken(&line);
            if (type.CmpNoCase(wxT("SMD")) == 0)
                output->Add(wxT("(attr smd)"));
        } else if (toupper(keyword[0]) == 'T' && isdigit(keyword[1])) {
            int type = keyword[1] - '0';
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            double textwidth = GetTokenDim(&line, true);
            double textheight = GetTokenDim(&line, true);
            long angle = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, true);
            GetToken(&line);    /* ignore mirror */
            wxString visible = GetToken(&line);
            long layer = GetTokenLong(&line);
            GetToken(&line);    /* ignore italic */
            wxString text = GetToken(&line);
            wxString newline = wxT("(fp_text ");
            switch (type) {
            case 0:
                newline += wxT("reference");
                break;
            case 1:
                newline += wxT("value");
                break;
            default:
                newline += wxT("user");
            }
            newline += wxT(" \"") + text + wxT("\"");
            if (angle == 0)
                newline += wxString::Format(wxT(" (at %.4f %.4f)"), xpos, ypos);
            else
                newline += wxString::Format(wxT(" (at %.4f %.4f %.1f)"), xpos, ypos, angle / 10.0);
            if (layer >= 0)
                newline += wxString::Format(wxT(" (layer %s)"), LayerName(layer));
            if (visible == wxT('H') || visible == wxT('I'))
                newline += wxT(" hide");
            newline += wxString::Format(wxT(" (effects (font (size %.4f %.4f) (thickness %.4f))))"), textwidth, textheight, penwidth);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("DS")) == 0) {
            double x1 = GetTokenDim(&line, true);
            double y1 = GetTokenDim(&line, true);
            double x2 = GetTokenDim(&line, true);
            double y2 = GetTokenDim(&line, true);
            double penwidth = GetTokenDim(&line, true);
            long layer = GetTokenLong(&line);
            wxString newline = wxString::Format(wxT("(fp_line (start %.4f %.4f) (end %.4f %.4f) (layer %s) (width %.4f))"),
                                                                                    x1, y1, x2, y2, LayerName(layer), penwidth);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("DC")) == 0) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            double xpt = GetTokenDim(&line, true);
            double ypt = GetTokenDim(&line, true);
            double penwidth = GetTokenDim(&line, true);
            long layer = GetTokenLong(&line);
            wxString newline = wxString::Format(wxT("(fp_circle (center %.4f %.4f) (end %.4f %.4f) (layer %s) (width %.4f))"),
                                                                                    xpos, ypos, xpt, ypt, LayerName(layer), penwidth);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("DA")) == 0) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            double xpt = GetTokenDim(&line, true);
            double ypt = GetTokenDim(&line, true);
            long angle = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, true);
            long layer = GetTokenLong(&line);
            wxString newline = wxString::Format(wxT("(fp_arc (start %.4f %.4f) (end %.4f %.4f) (angle %.1f) (layer %s) (width %.4f))"),
                                                                                    xpos, ypos, xpt, ypt, angle / 10.0, LayerName(layer), penwidth);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("DP")) == 0) {
            GetToken(&line);
            GetToken(&line);
            GetToken(&line);
            GetToken(&line);
            long count = GetTokenLong(&line);
            double penwidth = GetTokenDim(&line, true);
            long layer = GetTokenLong(&line);
            wxString newline = wxT("(fp_poly (pts");
            while (count-- > 0) {
                idx++;
                line = module[idx];
                wxString keyword = GetToken(&line);
                wxASSERT(keyword.CmpNoCase(wxT("Dl")) == 0);
                double x = GetTokenDim(&line, true);
                double y = GetTokenDim(&line, true);
                newline += wxString::Format(wxT(" (xy %.4f %.4f)"), x, y);
            }
            newline += wxT(")");
            newline += wxString::Format(wxT(" (layer %s) (width %.4f))"), LayerName(layer), penwidth);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("At")) == 0) {
            wxString type = GetToken(&line);
            if (type.CmpNoCase(wxT("SMD")) == 0)
                output->Add(wxT("(attr smd)"));
        } else if (keyword.CmpNoCase(wxT("Cd")) == 0) {
            output->Add(wxT("(descr \"") + line + wxT("\")"));
        } else if (keyword.CmpNoCase(wxT("Kw")) == 0) {
            output->Add(wxT("(tags \"") + line + wxT("\")"));
        } else if (keyword.CmpNoCase(wxT("Op")) == 0) {
            long pen90 = GetTokenLong(&line);
            long pen180 = GetTokenLong(&line);
            wxString newline;
            if (pen90 != 0) {
                newline = wxString::Format(wxT("(autoplace_cost90 %ld)"), pen90);
                output->Add(newline);
            }
            if (pen180 != 0) {
                newline = wxString::Format(wxT("(autoplace_cost90 %ld)"), pen180);
                output->Add(newline);
            }
        } else if (keyword.CmpNoCase(wxT(".SolderMask")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT("(solder_mask_margin %.4f)"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".SolderPaste")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT("(solder_paste_margin %.4f)"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".SolderPasteRatio")) == 0) {
            double ratio = GetTokenDouble(&line);
            wxString newline = wxString::Format(wxT("(solder_paste_margin_ratio %.2f)"), ratio);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".LocalClearance")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT("(clearance %.4f)"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".ZoneConnection")) == 0) {
            long val = GetTokenLong(&line);
            wxString newline = wxString::Format(wxT("(zone_connect %ld)"), val);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".ThermalWidth")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT("(thermal_width %.4f)"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT(".ThermalGap")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT("(thermal_gap %.4f)"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            wxString name, type, shape;
            double px = 0, py = 0, pw = 0, ph = 0, tx = 0, ty = 0;
            double drillx = 0, drilly = 0, drillwidth = 0, drillheight = 0;
            double die_length = 0, solder_mask_margin = 0, clearance = 0;
            double solder_paste_margin = 0, solder_paste_ratio = 0;
            double thermal_width = 0, thermal_gap = 0;
            long zone_connect = -1;
            long angle = 0;
            long layermask = 0;
            while (idx < module.Count()) {
                idx++;
                line = module[idx];
                line.Trim();
                if (line.CmpNoCase(wxT("$EndPAD")) == 0)
                    break;
                keyword = GetToken(&line);
                if (keyword.CmpNoCase(wxT("Po")) == 0) {
                    px = GetTokenDim(&line, true);
                    py = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT("Sh")) == 0) {
                    name = GetToken(&line);
                    shape = GetToken(&line);
                    pw = GetTokenDim(&line, true);
                    ph = GetTokenDim(&line, true);
                    tx = GetTokenDim(&line, true);
                    ty = GetTokenDim(&line, true);
                    angle = GetTokenLong(&line);
                    /* convert naming conventions */
                    switch (toupper(shape[0])) {
                    case 'C':
                        shape = wxT("circle");
                        break;
                    case 'R':
                        shape = wxT("rect");
                        break;
                    case 'O':
                        shape = wxT("oval");
                        break;
                    case 'T':
                        shape = wxT("trapezoid");
                        break;
                    }
                } else if (keyword.CmpNoCase(wxT("Dr")) == 0) {
                    drillwidth = GetTokenDim(&line, true);
                    drillx = GetTokenDim(&line, true);
                    drilly = GetTokenDim(&line, true);
                    if (line.length() > 0) {
                        GetToken(&line);    /* ignore 'O' */
                        drillwidth = GetTokenDim(&line, true);
                        drillheight = GetTokenDim(&line, true);
                    } else {
                        drillheight = drillwidth;
                    }
                } else if (keyword.CmpNoCase(wxT("At")) == 0) {
                    type = GetToken(&line);
                    GetToken(&line);    /* skip unknown attribute */
                    wxString field = GetToken(&line);
                    field.ToLong(&layermask, 16);
                    /* convert naming conventions */
                    if (type.CmpNoCase(wxT("STD")) == 0)
                        type = wxT("thru_hole");
                    else if (type.CmpNoCase(wxT("SMD")) == 0)
                        type = wxT("smd");
                    else if (type.CmpNoCase(wxT("CONN")) == 0)
                        type = wxT("connect");
                    else if (type.CmpNoCase(wxT("HOLE")) == 0)
                        type = wxT("np_thru_hole");
                } else if (keyword.CmpNoCase(wxT("Le")) == 0) {
                    die_length = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT(".SolderMask")) == 0) {
                    solder_mask_margin = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT(".LocalClearance")) == 0) {
                    clearance = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT(".SolderPaste")) == 0) {
                    solder_paste_margin = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT(".SolderPasteRatio")) == 0) {
                    solder_paste_ratio = GetTokenDouble(&line);
                } else if (keyword.CmpNoCase(wxT(".ZoneConnection")) == 0) {
                    zone_connect = GetTokenLong(&line);
                } else if (keyword.CmpNoCase(wxT(".ThermalWidth")) == 0) {
                    thermal_width = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT(".ThermalGap")) == 0) {
                    thermal_gap = GetTokenDim(&line, true);
                }
            }
            wxString newline = wxString::Format(wxT("(pad \"%s\" %s %s"), name.c_str(), type.c_str(), shape.c_str());
            if (angle != 0)
                newline += wxString::Format(wxT(" (at %.4f %.4f %.1f)"), px, py, angle / 10.0);
            else
                newline += wxString::Format(wxT(" (at %.4f %.4f)"), px, py);
            newline += wxString::Format(wxT(" (size %.4f %.4f)"), pw, ph);
            if (!Equal(tx, 0.0) || !Equal(ty, 0.0))
                newline += wxString::Format(wxT(" (rect_delta %.4f %.4f)"), tx, ty);
            if ((type.CmpNoCase(wxT("thru_hole")) == 0 || type.CmpNoCase(wxT("np_thru_hole")) == 0) && drillwidth > EPSILON) {
                if (Equal(drillwidth, drillheight))
                    newline += wxString::Format(wxT(" (drill %.4f (offset %.4f %.4f))"), drillwidth, drillx, drilly);
                else
                    newline += wxString::Format(wxT(" (drill oval %.4f %.4f (offset %.4f %.4f))"), drillwidth, drillheight, drillx, drilly);
            }
            newline += wxT(" (layers");
            if ((layermask & 0x0000ffff) == 0x0000ffff) {
                newline += wxT(" *.Cu");
                layermask &= ~ 0x0000ffff;
            }
            if ((layermask & 0x00300000) == 0x00300000) {
                newline += wxT(" *.Silk");
                layermask &= ~ 0x00300000;
            }
            if ((layermask & 0x00c00000) == 0x00c00000) {
                newline += wxT(" *.Mask");
                layermask &= ~ 0x00c00000;
            }
            for (long lidx = 0; lidx < 32; lidx++) {
                if ((layermask & (1 << lidx)) != 0) {
                    newline += wxT(" ");
                    newline += LayerName(lidx);
                }
            }
            newline += wxT(")");
            if (die_length > EPSILON)
                newline += wxString::Format(wxT(" (die_length %.4f)"), die_length);
            if (solder_mask_margin > EPSILON)
                newline += wxString::Format(wxT(" (solder_mask_margin %.4f)"), solder_mask_margin);
            if (clearance > EPSILON)
                newline += wxString::Format(wxT(" (clearance %.4f)"), clearance);
            if (solder_paste_margin < -EPSILON || solder_paste_margin > EPSILON)
                newline += wxString::Format(wxT(" (solder_paste_margin %.4f)"), solder_paste_margin);
            if (solder_paste_ratio < -EPSILON || solder_paste_ratio > EPSILON)
                newline += wxString::Format(wxT(" (solder_paste_margin_ratio %.2f)"), solder_paste_ratio);
            if (zone_connect >= 0)
                newline += wxString::Format(wxT(" (zone_connect %ld)"), zone_connect);
            if (thermal_width > EPSILON)
                newline += wxString::Format(wxT(" (thermal_width %.4f)"), thermal_width);
            if (thermal_gap > EPSILON)
                newline += wxString::Format(wxT(" (thermal_gap %.4f)"), thermal_gap);
            newline += wxT(")");
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("$SHAPE3D")) == 0) {
            wxString path = wxEmptyString;
            double ox = 0, oy = 0, oz = 0;
            double sx = 1, sy = 1, sz = 1;
            double rx = 0, ry = 0, rz = 0;
            while (idx < module.Count()) {
                idx++;
                line = module[idx];
                line.Trim();
                if (line.CmpNoCase(wxT("$EndSHAPE3D")) == 0)
                    break;
                keyword = GetToken(&line);
                if (keyword.CmpNoCase(wxT("Na")) == 0) {
                    path = line;
                    if (path[0] != wxT('"'))
                            path = wxT("\"") + path + wxT("\"");
                } else if (keyword.CmpNoCase(wxT("Of")) == 0) {
                    ox = GetTokenDim(&line, true);
                    oy = GetTokenDim(&line, true);
                    oz = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT("Sc")) == 0) {
                    sx = GetTokenDim(&line, true);
                    sy = GetTokenDim(&line, true);
                    sz = GetTokenDim(&line, true);
                } else if (keyword.CmpNoCase(wxT("Ro")) == 0) {
                    rx = GetTokenDim(&line, true);
                    ry = GetTokenDim(&line, true);
                    rz = GetTokenDim(&line, true);
                }
            }
            wxString newline = wxString::Format(wxT("(model %s (at (xyz %.4f %.4f %.4f)) (scale (xyz %.4f %.4f %.4f)) (rotate (xyz %.4f %.4f %.4f)))"),
                                                                                    path.c_str(), ox, oy, oz, sx, sy, sz, rx, ry, rz);
            output->Add(newline);
        }
    }
}

void TranslateToLegacy(wxArrayString* output, const wxArrayString& module)
{
    wxASSERT(output != 0);
    output->Clear();
    wxString symbolname;
    wxString templatename;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        if (line[0] == '#') {
            /* check for a #template comment, to translate this to "AR" */
            if (line.Left(9).CmpNoCase(wxT("#template")) == 0)
                templatename = line.Mid(9);
            continue;
        }
        wxString keyword = GetToken(&line);
        if (keyword[0] != '(')
            continue;
        keyword = keyword.Mid(1);
        if (keyword.CmpNoCase(wxT("module")) == 0) {
            symbolname = GetToken(&line);
            symbolname = symbolname.AfterLast(wxT(':'));
            bool islocked = false;
            if (line.length() > 0) {
                wxString field = GetToken(&line);
                if (field.CmpNoCase(wxT("locked")) == 0)
                    islocked = true;
            }
            double xpos = 0, ypos = 0, angle = 0;
            long layer = -1;
            long penalty90 = 0, penalty180 = 0;
            unsigned long timestamp = 0;
            wxString cmptype = wxT("STD");
            /* get other information needed for the header lines */
            for (unsigned idx2 = idx + 1; idx2 < module.Count(); idx2++) {
                wxString line = module[idx];
                wxString keyword = GetToken(&line);
                if (keyword[0] != '(')
                    continue;
                keyword = keyword.Mid(1);
                if (keyword.CmpNoCase(wxT("layer")) == 0) {
                    wxString field = GetToken(&line);
                    layer = LayerNumber(field);
                } else if (keyword.CmpNoCase(wxT("at")) == 0) {
                    xpos = GetTokenDim(&line, true);
                    ypos = GetTokenDim(&line, true);
                    if (line.length() > 0)
                        angle = GetTokenDouble(&line);
                } else if (keyword.CmpNoCase(wxT("attr")) == 0) {
                    wxString field = GetToken(&line);
                    if (field.CmpNoCase(wxT("smd")) == 0)
                        cmptype = wxT("SMD");
                } else if (keyword.CmpNoCase(wxT("autoplace_cost90")) == 0) {
                    penalty90 = GetTokenLong(&line);
                } else if (keyword.CmpNoCase(wxT("autoplace_cost180")) == 0) {
                    penalty180 = GetTokenLong(&line);
                } else if (keyword.CmpNoCase(wxT("tedit")) == 0) {
                    wxString field = GetToken(&line);
                    field.ToULong(&timestamp, 16);
                }
            }
            output->Add(wxT("$MODULE ") + symbolname);
            wxString newline = wxString::Format(wxT("Po %.4f %.4f %ld %ld %X 0 "),
                                                xpos, ypos, (long)(angle * 10),
                                                layer, timestamp);
            if (islocked)
                newline += wxT("F~");
            else
                newline += wxT("~~");
            output->Add(newline);
            output->Add(wxT("Li ") + symbolname);
            output->Add(wxT("At ") + cmptype);
            if (penalty90 > 0 || penalty180 > 0) {
                newline = wxString::Format(wxT("Op %ld %ld 0"), penalty90, penalty180);
                output->Add(newline);
            }
            if (templatename.Length() > 0)
                output->Add(wxT("AR ") + templatename);
        } else if (keyword.CmpNoCase(wxT("descr")) == 0) {
            wxString field = GetToken(&line);
            output->Add(wxT("Cd ") + field);
        } else if (keyword.CmpNoCase(wxT("tags")) == 0) {
            wxString field = GetToken(&line);
            output->Add(wxT("Kw ") + field);
        } else if (keyword.CmpNoCase(wxT("fp_text")) == 0) {
            wxString field = GetToken(&line);
            long texttype = 2;  /* assume "user" */
            if (field.CmpNoCase(wxT("reference")) == 0)
                texttype = 0;
            else if (field.CmpNoCase(wxT("value")) == 0)
                texttype = 1;
            wxString text = GetToken(&line);
            bool ishidden = line.Find(wxT(" hide ")) > 0;
            double xpos = 0, ypos = 0, angle = 0;
            wxString section = GetSection(line, wxT("at"));
            if (section.length() > 0) {
                xpos = GetTokenDim(&section, true);
                ypos = GetTokenDim(&section, true);
                if (section.length() > 0)
                    angle = GetTokenDouble(&section);
            }
            long layer = 21;    /* assume top silk */
            section = GetSection(line, wxT("layer"));
            if (section.length() > 0) {
                field = GetToken(&section);
                layer = LayerNumber(field);
            }
            double width = 1, height = 1, penwidth = 0.1;
            section = GetSection(line, wxT("effects"));
            if (section.length() > 0) {
                section = GetSection(section, wxT("font"));
                if (section.length() > 0) {
                    wxString subsection = GetSection(section, wxT("size"));
                    if (subsection.length() > 0) {
                        width = GetTokenDim(&subsection, true);
                        height = GetTokenDim(&subsection, true);
                    }
                    subsection = GetSection(section, wxT("thickness"));
                    if (subsection.length() > 0)
                        penwidth = GetTokenDim(&subsection, true);
                }
            }
            wxString newline = wxString::Format(wxT("T%ld %.4f %.4f %.4f %.4f %ld %.4f N %c %ld N \"%s\""),
                                                                                    texttype, xpos, ypos, width, height,
                                                                                    (long)(angle * 10), penwidth,
                                                                                    ishidden ? 'I' : 'V', layer, text.c_str());
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("fp_line")) == 0) {
            double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
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
            double penwidth = 0.1;
            section = GetSection(line, wxT("width"));
            if (section.length() > 0)
                penwidth = GetTokenDim(&section, true);
            long layer = 21;    /* assume top silk */
            section = GetSection(line, wxT("layer"));
            if (section.length() > 0) {
                wxString field = GetToken(&section);
                layer = LayerNumber(field);
            }
            wxString newline = wxString::Format(wxT("DS %.4f %.4f %.4f %.4f %.4f %ld"),
                                                                                    x1, y1, x2, y2, penwidth, layer);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("fp_circle")) == 0) {
            double xpos = 0, ypos = 0, xpt = 0, ypt = 0;
            wxString section = GetSection(line, wxT("center"));
            if (section.length() > 0) {
                xpos = GetTokenDim(&section, true);
                ypos = GetTokenDim(&section, true);
            }
            section = GetSection(line, wxT("end"));
            if (section.length() > 0) {
                xpt = GetTokenDim(&section, true);
                ypt = GetTokenDim(&section, true);
            }
            double penwidth = 0.1;
            section = GetSection(line, wxT("width"));
            if (section.length() > 0)
                penwidth = GetTokenDim(&section, true);
            long layer = 21;    /* assume top silk */
            section = GetSection(line, wxT("layer"));
            if (section.length() > 0) {
                wxString field = GetToken(&section);
                layer = LayerNumber(field);
            }
            wxString newline = wxString::Format(wxT("DC %.4f %.4f %.4f %.4f %.4f %ld"),
                                                                                    xpos, ypos, xpt, ypt, penwidth, layer);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("fp_arc")) == 0) {
            double xpos = 0, ypos = 0, xpt = 0, ypt = 0;
            wxString section = GetSection(line, wxT("start"));
            if (section.length() > 0) {
                xpos = GetTokenDim(&section, true);
                ypos = GetTokenDim(&section, true);
            }
            section = GetSection(line, wxT("end"));
            if (section.length() > 0) {
                xpt = GetTokenDim(&section, true);
                ypt = GetTokenDim(&section, true);
            }
            double angle = 0;
            section = GetSection(line, wxT("angle"));
            if (section.length() > 0)
                angle = GetTokenDim(&section, true);
            double penwidth = 0.1;
            section = GetSection(line, wxT("width"));
            if (section.length() > 0)
                penwidth = GetTokenDim(&section, true);
            long layer = 21;    /* assume top silk */
            section = GetSection(line, wxT("layer"));
            if (section.length() > 0) {
                wxString field = GetToken(&section);
                layer = LayerNumber(field);
            }
            wxString newline = wxString::Format(wxT("DA %.4f %.4f %.4f %.4f %ld %.4f %ld"),
                                                                                    xpos, ypos, xpt, ypt, (long)(10 * angle), penwidth, layer);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("fp_poly")) == 0) {
            long count = 0;
            wxString section = GetSection(line, wxT("pts"));
            if (section.Length() > 0) {
                while (GetSection(section, wxT("xy"), count).length() > 0)
                    count++;
            }
            double penwidth = 0.1;
            section = GetSection(line, wxT("width"));
            if (section.length() > 0)
                penwidth = GetTokenDim(&section, true);
            long layer = 21;    /* assume top silk */
            section = GetSection(line, wxT("layer"));
            if (section.length() > 0) {
                wxString field = GetToken(&section);
                layer = LayerNumber(field);
            }
            wxString newline = wxString::Format(wxT("DP 0 0 0 0 %ld %.4f %ld"),
                                                                                    count, penwidth, layer);
            output->Add(newline);
            section = GetSection(line, wxT("pts"));
            for (long i = 0; i < count; i++) {
                wxString subsect = GetSection(section, wxT("xy"), count);
                double x = GetTokenDim(&subsect, true);
                double y = GetTokenDim(&subsect, true);
                newline = wxString::Format(wxT("Dl %.4f %.4f"), x, y);
                output->Add(newline);
            }
        } else if (keyword.CmpNoCase(wxT("fp_curve")) == 0) {
            wxASSERT(0);    /* fp_curve cannot be supported (need to convert to DP/Dl in legacy format) */
        } else if (keyword.CmpNoCase(wxT("solder_mask_margin")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT(".SolderMask %.4f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("solder_paste_margin")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT(".SolderPaste %.4f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("solder_paste_margin_ratio")) == 0) {
            double dim = GetTokenDouble(&line);
            wxString newline = wxString::Format(wxT(".SolderPasteRatio %.2f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("clearance")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT(".LocalClearance %.4f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("zone_connect")) == 0) {
            long dim = GetTokenLong(&line);
            wxString newline = wxString::Format(wxT(".ZoneConnection %ld"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("thermal_width")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT(".ThermalWidth %.4f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("thermal_gap")) == 0) {
            double dim = GetTokenDim(&line, true);
            wxString newline = wxString::Format(wxT(".ThermalGap %.4f"), dim);
            output->Add(newline);
        } else if (keyword.CmpNoCase(wxT("pad")) == 0) {
            output->Add(wxT("$PAD"));
            wxString name = GetToken(&line);
            wxString type = GetToken(&line);
            wxString shape = GetToken(&line);
            double angle = 0;
            wxString section = GetSection(line, wxT("at"));
            if (section.Length() > 0) {
                double xpos = GetTokenDim(&section, true);
                double ypos = GetTokenDim(&section, true);
                wxString newline = wxString::Format(wxT("Po %.4f %.4f"), xpos, ypos);
                output->Add(newline);
                if (line.length() > 0)
                    angle = GetTokenDouble(&section);
            }
            section = GetSection(line, wxT("size"));
            if (section.Length() > 0) {
                double width = GetTokenDim(&section, true);
                double height = GetTokenDim(&section, true);
                char lshape = 'C';
                if (shape.CmpNoCase(wxT("rect")) == 0)
                    lshape = 'R';
                else if (shape.CmpNoCase(wxT("oval")) == 0)
                    lshape = 'O';
                else if (shape.CmpNoCase(wxT("trapezoid")) == 0)
                    lshape = 'T';
                double dx = 0, dy = 0;
                section = GetSection(line, wxT("rect_delta"));
                if (section.Length() > 0) {
                    dx = GetTokenDim(&section, true);
                    dy = GetTokenDim(&section, true);
                }
                wxString newline = wxString::Format(wxT("Sh \"%s\" %c %.4f %.4f %.4f %.4f %ld"),
                                                                                        name.c_str(), lshape, width, height, dx, dy,
                                                                                        (long)(angle * 10));
                output->Add(newline);
            }
            long mask = 0;
            section = GetSection(line, wxT("layers"));
            if (section.Length() > 0) {
                while (section.length() > 0) {
                    wxString field = GetToken(&section);
                    if (field.CmpNoCase(wxT("*.Cu")) == 0)
                        mask |= 0x0000ffff;
                    else if (field.CmpNoCase(wxT("*.Mask")) == 0)
                        mask |= 0x00c00000;
                    else if (field.CmpNoCase(wxT("*.Silk")) == 0)
                        mask |= 0x00300000;
                    else
                        mask |= LayerNumber(field);
                }
            }
            if (type.CmpNoCase(wxT("thru_hole")) == 0)
                type = wxT("STD");
            else if (type.CmpNoCase(wxT("smd")) == 0)
                type = wxT("SMD");
            else if (type.CmpNoCase(wxT("connect")) == 0)
                type = wxT("CONN");
            else
                type = wxT("HOLE");
            wxString newline = wxString::Format(wxT("At %s N %X"), type.c_str(), mask);
            output->Add(newline);
            section = GetSection(line, wxT("drill"));
            if (section.Length() > 0) {
                wxString field = GetToken(&section);
                double width, height;
                if (field.CmpNoCase(wxT("oval")) == 0) {
                    width = GetTokenDim(&section, true);
                    height = GetTokenDim(&section, true);
                } else {
                    field.ToDouble(&width); /* already in mm, so no conversion needed */
                    height = width;
                }
                double xpos = 0, ypos = 0;
                section = GetSection(section, wxT("offset"));
                if (section.Length() > 0) {
                    xpos = GetTokenDim(&section, true);
                    ypos = GetTokenDim(&section, true);
                }
                if (Equal(width, height))
                    newline = wxString::Format(wxT("Dr %.4f %.4f %.4f"), width, xpos, ypos);
                else
                    newline = wxString::Format(wxT("Dr %.4f %.4f %.4f O %.4f %.4f"),
                                                min(width, height), xpos, ypos, width, height);
                output->Add(newline);
            }
            section = GetSection(line, wxT("die_length"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT("Le %.4f"), dim);
                output->Add(newline);
            }
            section = GetSection(line, wxT("solder_mask_margin"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT(".SolderMask %.4f"), dim);
                output->Add(newline);
            }
            section = GetSection(line, wxT("clearance"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT(".LocalClearance %.4f"), dim);
                output->Add(newline);
            }
            section = GetSection(line, wxT("solder_paste_margin"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT(".SolderPaste %.4f"), dim);
                output->Add(newline);
            }
            section = GetSection(line, wxT("solder_paste_margin_ratio"));
            if (section.Length() > 0) {
                double val = GetTokenDouble(&section);
                newline = wxString::Format(wxT(".SolderPasteRatio %.2f"), val);
                output->Add(newline);
            }
            section = GetSection(line, wxT("zone_connect"));
            if (section.Length() > 0) {
                long val = GetTokenLong(&section);
                newline = wxString::Format(wxT(".ZoneConnection %ld"), val);
                output->Add(newline);
            }
            section = GetSection(line, wxT("thermal_width"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT(".ThermalWidth %.4f"), dim);
                output->Add(newline);
            }
            section = GetSection(line, wxT("thermal_gap"));
            if (section.Length() > 0) {
                double dim = GetTokenDim(&section, true);
                newline = wxString::Format(wxT(".ThermalGap %.4f"), dim);
                output->Add(newline);
            }
            output->Add(wxT("$EndPAD"));
        } else if (keyword.CmpNoCase(wxT("model")) == 0) {
            output->Add(wxT("$SHAPE3D"));
            wxString name = GetToken(&line);
            output->Add(wxT("Na ") + name);
            wxString section = GetSection(line, wxT("at"));
            if (section.Length() > 0) {
                section = GetSection(section, wxT("xyz"));
                if (section.Length() > 0) {
                    double x = GetTokenDim(&section, true);
                    double y = GetTokenDim(&section, true);
                    double z = GetTokenDim(&section, true);
                    wxString newline = wxString::Format(wxT("Of %.4f %.4f %.4f"), x, y, z);
                    output->Add(newline);
                }
            }
            section = GetSection(line, wxT("scale"));
            if (section.Length() > 0) {
                section = GetSection(section, wxT("xyz"));
                if (section.Length() > 0) {
                    double x = GetTokenDim(&section, true);
                    double y = GetTokenDim(&section, true);
                    double z = GetTokenDim(&section, true);
                    wxString newline = wxString::Format(wxT("Sc %.4f %.4f %.4f"), x, y, z);
                    output->Add(newline);
                }
            }
            section = GetSection(line, wxT("rotate"));
            if (section.Length() > 0) {
                section = GetSection(section, wxT("xyz"));
                if (section.Length() > 0) {
                    double x = GetTokenDim(&section, true);
                    double y = GetTokenDim(&section, true);
                    double z = GetTokenDim(&section, true);
                    wxString newline = wxString::Format(wxT("Ro %.4f %.4f %.4f"), x, y, z);
                    output->Add(newline);
                }
            }
            output->Add(wxT("$EndSHAPE3D"));
        }
    }
    if (output->Count() > 0)
        output->Add(wxT("$EndMODULE ") + symbolname);
}

int AdjustPad(wxArrayString& module, FootprintInfo* current, const FootprintInfo& adjusted)
{
    wxASSERT(current != NULL);

    wxString padshape_adj = wxEmptyString;  /* new pad shape name (s-expressions) */
    switch (adjusted.PadShape) {
    case 'C':
    case 'S':
        padshape_adj = wxT("circle");
        break;
    case 'O':
        padshape_adj = wxT("oval");
        break;
    case 'R':
        padshape_adj = wxT("rect");
        break;
    case 'T':
        padshape_adj = wxT("trapezoid");
        break;
    }

    CoordPair padsize;
    char padshape = '\0';                   /* pad shape letter for legacy modules */
    wxString padshape_s = wxEmptyString;    /* pad shape name for the s-expressions */
    bool all_pads_smd = true;               /* if all pads are SMD, the module is marked SMD too */
    bool footprint_smd = false;
    unsigned footprint_attr_line = -1;
    double drillsize = 0;
    long pinnr = 0;
    unsigned Start = 0;

    int Matches = 0;
    bool inpad = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
            Start = idx;    /* save the position where this pad starts, because
                               on a match, it must be reparsed */
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            wxASSERT(Start > 0);
            wxASSERT(inpad);
            if (pinnr > 0 && padshape == current->PadShape && padsize.Equal(current->PadSize[0], TOLERANCE)
                && (drillsize > current->DrillSize - TOLERANCE && drillsize < current->DrillSize + TOLERANCE))
            {
                /* this pad matches the primary pad, go through it again, to adjust it */
                for (unsigned i = Start; i < module.Count(); i++) {
                    wxString line = module[i];
                    wxString keyword = GetToken(&line);
                    if (keyword.CmpNoCase(wxT("$EndPAD")) == 0)
                        break;
                    if (keyword.CmpNoCase(wxT("Sh")) == 0) {
                        wxString name = GetToken(&line);
                        GetToken(&line);    /* ignore shape */
                        GetToken(&line);    /* ignore width */
                        GetToken(&line);    /* ignore height */
                        double xdelta = GetTokenDim(&line, true);
                        double ydelta = GetTokenDim(&line, true);
                        long rot = GetTokenLong(&line);
                        padshape = adjusted.PadShape;
                        if (padshape == 'S')
                            padshape = (pinnr == 1) ? 'R' : 'C';
                        padsize.Set(adjusted.PadSize[0].GetX(), adjusted.PadSize[0].GetY());
                        module[i] = wxString::Format(wxT("%s \"%s\" %c %.4f %.4f %.4f %.4f %ld"),
                                                     keyword.c_str(), name.c_str(), padshape,
                                                     MM(padsize.GetX()), MM(padsize.GetY()),
                                                     MM(xdelta), MM(ydelta), rot);
                    } else if (keyword.CmpNoCase(wxT("Dr")) == 0) {
                        GetToken(&line);    /* ignore drill size */
                        double xpos = GetTokenDim(&line, true);
                        double ypos = GetTokenDim(&line, true);
                        drillsize = adjusted.DrillSize;
                        module[i] = wxString::Format(wxT("%s %.4f %.4f %.4f"),
                                                     keyword.c_str(), MM(drillsize),
                                                     MM(xpos), MM(ypos));
                    } else if (keyword.CmpNoCase(wxT("At")) == 0) {
                        wxString type = GetToken(&line);
                        if (type.CmpNoCase(wxT("STD")) == 0 && adjusted.DrillSize < EPSILON)
                            module[i] = wxT("At SMD N 00888000");
                        else if (type.CmpNoCase(wxT("SMD")) == 0 && adjusted.DrillSize > EPSILON)
                            module[i] = wxT("At STD N 00E0FFFF");
                    }
                }
                all_pads_smd = all_pads_smd && (drillsize > EPSILON);
                Matches++;
            } else if (pinnr > 0 && padshape == 'R' && drillsize < EPSILON
                       && padsize.Equal(current->PadSize[1], TOLERANCE))
            {
                /* this pad matches the secondary pad, go through it again, to adjust it */
                for (unsigned i = Start; i < module.Count(); i++) {
                    wxString line = module[i];
                    wxString keyword = GetToken(&line);
                    if (keyword.CmpNoCase(wxT("$EndPAD")) == 0)
                        break;
                    if (keyword.CmpNoCase(wxT("Sh")) == 0) {
                        wxString name = GetToken(&line);
                        wxString shape = GetToken(&line);
                        GetToken(&line);    /* ignore width */
                        GetToken(&line);    /* ignore height */
                        double xdelta = GetTokenDim(&line, true);
                        double ydelta = GetTokenDim(&line, true);
                        long rot = GetTokenLong(&line);
                        if (padsize.Equal(current->PadSize[1], 0.01))
                            padsize.Set(adjusted.PadSize[1].GetX(), adjusted.PadSize[1].GetY());
                        else
                            padsize.Set(adjusted.PadSize[1].GetY(), adjusted.PadSize[1].GetX());
                        module[i] = wxString::Format(wxT("%s \"%s\" %c %.4f %.4f %.4f %.4f %ld"),
                                                     keyword.c_str(), name.c_str(), shape[0],
                                                     MM(padsize.GetX()), MM(padsize.GetY()),
                                                     MM(xdelta), MM(ydelta), rot);
                    }
                }
                Matches++;
            }
            padsize.Set(0, 0);
            padshape = '\0';
            drillsize = 0;
            pinnr = 0;
            inpad = false;
        } else if (keyword.CmpNoCase(wxT("Sh")) == 0 && inpad) {
            wxString field = GetToken(&line);
            pinnr = GetPadNumber(field, &current->MtxWidth, &current->MtxHeight);
            field = GetToken(&line);
            padshape = field[0];
            if (current->PadShape == 'S'
                && ((padshape == 'R' && pinnr == 1)
                    || (padshape == 'C' && pinnr > 1)))
                padshape = 'S';
            double width = GetTokenDim(&line, true);
            double height = GetTokenDim(&line, true);
            padsize.Set(width, height);
        } else if (keyword.CmpNoCase(wxT("Dr")) == 0 && inpad) {
            drillsize = GetTokenDim(&line, true);
        } else if (keyword.CmpNoCase(wxT("At")) == 0 && !inpad) {
            wxString type = GetToken(&line);
            footprint_smd = (type.CmpNoCase(wxT("SMD")) == 0);
            footprint_attr_line = idx;
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            /* first parse the pad information */
            wxString field = GetToken(&line);
            pinnr = GetPadNumber(field, &current->MtxWidth, &current->MtxHeight);
            GetToken(&line);    /* ignore smd/thru_hole/np_thru_hole type */
            padshape_s = GetToken(&line);
            if (padshape_s.Cmp(wxT("circle")) == 0)
                padshape = (current->PadShape == 'S' && pinnr > 1) ? 'S' : 'C';
            else if (padshape_s.Cmp(wxT("rect")) == 0)
                padshape = (current->PadShape == 'S' && pinnr == 1) ? 'S' : 'R';
            else if (padshape_s.Cmp(wxT("oval")) == 0)
                padshape = 'O';
            else if (padshape_s.Cmp(wxT("trapezoid")) == 0)
                padshape = 'T';
            wxString section = GetSection(line, wxT("size"));
            if (section.length() > 0) {
                double width = GetTokenDim(&section, true);
                double height = GetTokenDim(&section, true);
                padsize.Set(width, height);
            }
            section = GetSection(line, wxT("drill"));
            if (section.length() > 0) {
                if (section.Left(4).Cmp(wxT("oval"))==0)
                    pinnr = 0;  /* oval holes are not supported for editing */
                else
                    drillsize = GetTokenDim(&section, true);
            }
            /* adjust the information */
            wxString line = module[idx];    /* get the unmodified line again */
            if (pinnr > 0 && padshape == current->PadShape && padsize.Equal(current->PadSize[0], TOLERANCE)
                && (drillsize > current->DrillSize - TOLERANCE && drillsize < current->DrillSize + TOLERANCE))
            {
                /* this pad matches the primary pad, adjust it */
                section = wxString::Format(wxT("%.4f %.4f"), MM(adjusted.PadSize[0].GetX()), MM(adjusted.PadSize[0].GetY()));
                SetSection(line, wxT("size"), section);
                if (adjusted.DrillSize > EPSILON) {
                    section = wxString::Format(wxT("%.4f"), MM(adjusted.DrillSize));
                    SetSection(line, wxT("drill"), section);    //??? this fails if there is no drill section yet (a new section must be inserted)
                } else {
                    DeleteSection(line, wxT("drill"));
                }
                if (adjusted.PadShape == 'S' && pinnr == 1)
                    line.Replace(padshape_s, wxT("rect"), false);
                else
                    line.Replace(padshape_s, padshape_adj, false);
                module[idx] = line;
                all_pads_smd = all_pads_smd && (drillsize > EPSILON);
                Matches++;
            } else if (pinnr > 0 && padshape == 'R' && drillsize < EPSILON
                       && padsize.Equal(current->PadSize[1], TOLERANCE))
            {
                /* this pad matches the secondary pad, adjust it */
                if (padsize.Equal(current->PadSize[1], 0.01))
                    padsize.Set(adjusted.PadSize[1].GetX(), adjusted.PadSize[1].GetY());
                else
                    padsize.Set(adjusted.PadSize[1].GetY(), adjusted.PadSize[1].GetX());
                section = wxString::Format(wxT("%.4f %.4f"), MM(padsize.GetX()), MM(padsize.GetY()));
                SetSection(line, wxT("size"), section);
                module[idx] = line;
                Matches++;
            }
            padsize.Set(0, 0);
            padshape = '\0';
            padshape_s = wxEmptyString;
            drillsize = 0;
            pinnr = 0;
        } else if (keyword.CmpNoCase(wxT("(attr")) == 0) {
            wxString type = GetToken(&line);
            footprint_smd = (type.CmpNoCase(wxT("smd")) == 0);
            footprint_attr_line = idx;
        }
    }

    if (all_pads_smd && !footprint_smd) {
        /* set the module attribute to SMD, first find out the type */
        int type = VER_INVALID;
        unsigned idx;
        for (idx = 0; type == VER_INVALID && idx < module.Count(); idx++) {
            wxString line = module[0];  /* check whether this is s-exprssion or legacy */
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("(module")) == 0)
                type = VER_S_EXPR;
            else if (keyword.CmpNoCase(wxT("$MODULE")) == 0)
                type = VER_MM;          /* for the attribute, it does not matter whether the type is VER_MM or VER_MIL */
        }
        /* the At or (attr ) line may be absent, in which case it must be inserted */
        if (footprint_attr_line < 0 && type != VER_INVALID) {
            /* insert the attribute after the header line */
            footprint_attr_line = idx + 1;
            module.Insert(wxT(""), footprint_attr_line);
        }
        wxASSERT(footprint_attr_line >= 0 && footprint_attr_line < module.Count());
        if (type == VER_S_EXPR)
            module[footprint_attr_line] = wxT("(attr smd)");
        else
            module[footprint_attr_line] = wxT("At SMD");
    }

    /* the module data was adjusted in-place, now also copy the pad/shape
       information structure back */
    *current = adjusted;

    return Matches;
}

/** AdjustPitchHor() changes the horizontal pitch of a range of pins.
 *  \param module       The footprint data.
 *  \param firstpin     The pin number of the first pin to adjust.
 *  \param lastpin      The pin number of the last pin to adjust.
 *  \param sequence     The value to add to get to each next pin (usually 1).
 *  \param ymatch       The Y-coordinate for the horizontal row that the pads
 *                      should be on.
 *  \param pitch        The new pitch.
 */
int AdjustPitchHor(wxArrayString& module, int firstpin, int lastpin, int sequence, double ymatch, double pitch)
{
    int Matches = 0;
    unsigned Start = 0;
    long pinnr = 0;
    bool inpad = false;
    wxASSERT(sequence >= 1);
    long range = (lastpin - firstpin) / sequence;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
            Start = idx;    /* save the position where this pad starts, because
                               on a match, it must be reparsed */
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            wxASSERT(Start > 0);
            wxASSERT(inpad);
            bool matchpin = (pinnr >= firstpin && pinnr <= lastpin);
            if (matchpin && sequence > 1)
                matchpin = matchpin && ((pinnr - firstpin) % sequence) == 0;
            if (matchpin) {
                /* this pad matches, go through it again, to adjust it */
                for (unsigned i = Start; i < module.Count(); i++) {
                    wxString line = module[i];
                    wxString keyword = GetToken(&line);
                    if (keyword.CmpNoCase(wxT("$EndPAD")) == 0)
                        break;
                    if (keyword.CmpNoCase(wxT("Po")) == 0) {
                        double xpos = GetTokenDim(&line, true);
                        double ypos = GetTokenDim(&line, true);
                        if (Equal(ypos, ymatch, 0.02)) {
                            xpos = pitch * ((pinnr - firstpin) / sequence - range / 2.0);
                            module[i] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(ypos));
                            Matches++;
                        }
                    }
                }
            }
            inpad = false;
            pinnr = 0;
            Start = 0;
        } else if (keyword.CmpNoCase(wxT("Sh")) == 0 && inpad) {
            wxString field = GetToken(&line);
            field.ToLong(&pinnr);
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            /* first check the pin number */
            wxString field = GetToken(&line);
            field.ToLong(&pinnr);
            bool matchpin = (pinnr >= firstpin && pinnr <= lastpin);
            if (matchpin && sequence > 1)
                matchpin = matchpin && ((pinnr - firstpin) % sequence) == 0;
            if (matchpin) {
                /* this pad matches, adjust it */
                wxString section = GetSection(line, wxT("at"));
                if (section.length() > 0) {
                    double xpos = GetTokenDim(&section, true);
                    double ypos = GetTokenDim(&section, true);
                    if (Equal(ypos, ymatch, 0.02)) {
                        xpos = pitch * ((pinnr - firstpin) / sequence - range / 2.0);
                        if (section.length() > 0) {
                            double rot = GetTokenDouble(&section);
                            section = wxString::Format(wxT("%.4f %.4f %.1f"), MM(xpos), MM(ypos), rot);
                        } else {
                            section = wxString::Format(wxT("%.4f %.4f"), MM(xpos), MM(ypos));
                        }
                        line = module[idx];
                        SetSection(line, wxT("at"), section);
                        module[idx] = line;
                        Matches++;
                    }
                }
            }
            pinnr = 0;
        }
    }
    return Matches;
}

/** AdjustPitchVer() changes the vertical pitch of a range of pins.
 *  \param module       The footprint data.
 *  \param firstpin     The pin number of the first pin to adjust.
 *  \param lastpin      The pin number of the last pin to adjust.
 *  \param sequence     The value to add to get to each next pin (usually 1).
 *  \param xmatch       The X-coordinate for the verticalal row that the pads
 *                      should be on.
 *  \param pitch        The new pitch.
 */
int AdjustPitchVer(wxArrayString& module, int firstpin, int lastpin, int sequence, double xmatch, double pitch)
{
    int Matches = 0;
    unsigned Start = 0;
    long pinnr = 0;
    bool inpad = false;
    wxASSERT(sequence >= 1);
    long range = (lastpin - firstpin) / sequence;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
            Start = idx;    /* save the position where this pad starts, because
                               on a match, it must be reparsed */
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            wxASSERT(Start > 0);
            wxASSERT(inpad);
            bool matchpin = (pinnr >= firstpin && pinnr <= lastpin);
            if (matchpin && sequence > 1)
                matchpin = matchpin && ((pinnr - firstpin) % sequence) == 0;
            if (matchpin) {
                /* this pad matches, go through it again, to adjust it */
                for (unsigned i = Start; i < module.Count(); i++) {
                    wxString line = module[i];
                    wxString keyword = GetToken(&line);
                    if (keyword.CmpNoCase(wxT("$EndPAD")) == 0)
                        break;
                    if (keyword.CmpNoCase(wxT("Po")) == 0) {
                        double xpos = GetTokenDim(&line, true);
                        double ypos = GetTokenDim(&line, true);
                        if (Equal(xpos, xmatch, 0.02)) {
                            ypos = pitch * ((pinnr - firstpin) / sequence - range / 2.0);
                            module[i] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(ypos));
                            Matches++;
                        }
                    }
                }
            }
            inpad = false;
            pinnr = 0;
            Start = 0;
        } else if (keyword.CmpNoCase(wxT("Sh")) == 0 && inpad) {
            wxString field = GetToken(&line);
            field.ToLong(&pinnr);
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            /* first check the pin number */
            wxString field = GetToken(&line);
            field.ToLong(&pinnr);
            bool matchpin = (pinnr >= firstpin && pinnr <= lastpin);
            if (matchpin && sequence > 1)
                matchpin = matchpin && ((pinnr - firstpin) % sequence) == 0;
            if (matchpin) {
                /* this pad matches, adjust it */
                wxString section = GetSection(line, wxT("at"));
                if (section.length() > 0) {
                    double xpos = GetTokenDim(&section, true);
                    double ypos = GetTokenDim(&section, true);
                    if (Equal(xpos, xmatch, 0.02)) {
                        ypos = pitch * ((pinnr - firstpin) / sequence - range / 2.0);
                        if (section.length() > 0) {
                            double rot = GetTokenDouble(&section);
                            section = wxString::Format(wxT("%.4f %.4f %.1f"), MM(xpos), MM(ypos), rot);
                        } else {
                            section = wxString::Format(wxT("%.4f %.4f"), MM(xpos), MM(ypos));
                        }
                        line = module[idx];
                        SetSection(line, wxT("at"), section);
                        module[idx] = line;
                        Matches++;
                    }
                }
            }
            pinnr = 0;
        }
    }
    return Matches;
}

/** AdjustPitchGrid() changes the pitch of all pins in a grid.
 *  \param module       The footprint data.
 *  \param curpitch     The current pitch.
 *  \param newpitch     The new pitch.
 */
int AdjustPitchGrid(wxArrayString& module, double curpitch, double newpitch)
{
    int Matches = 0;
    bool inpad = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            wxASSERT(inpad);
            inpad = false;
        } else if (keyword.CmpNoCase(wxT("Po")) == 0 && inpad) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            /* scale the coordinates with the factor between the current and new pitch */
            xpos *= newpitch / curpitch;
            ypos *= newpitch / curpitch;
            module[idx] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(ypos));
            Matches++;
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            wxString section = GetSection(line, wxT("at"));
            if (section.length() > 0) {
                double xpos = GetTokenDim(&section, true);
                double ypos = GetTokenDim(&section, true);
                /* scale the coordinates with the factor between the current and new pitch */
                xpos *= newpitch / curpitch;
                ypos *= newpitch / curpitch;
                line = module[idx];
                SetSection(line, wxT("at"), section);
                module[idx] = line;
                Matches++;
            }

        }
    }
    return Matches;
}

int MovePadHor(wxArrayString& module, double from_x, double to_x)
{
    int Matches = 0;
    bool inpad = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            inpad = false;
        } else if (keyword.CmpNoCase(wxT("Po")) == 0 && inpad) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            if (Equal(xpos, from_x, 0.02)) {
                module[idx] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(to_x), MM(ypos));
                Matches++;
            }
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            wxString section = GetSection(line, wxT("at"));
            if (section.length() > 0) {
                double xpos = GetTokenDim(&section, true);
                double ypos = GetTokenDim(&section, true);
                if (Equal(xpos, from_x, 0.02)) {
                    if (section.length() > 0) {
                        double rot = GetTokenDouble(&section);
                        section = wxString::Format(wxT("%.4f %.4f %.1f"), MM(to_x), MM(ypos), rot);
                    } else {
                        section = wxString::Format(wxT("%.4f %.4f"), MM(to_x), MM(ypos));
                    }
                    line = module[idx];
                    SetSection(line, wxT("at"), section);
                    module[idx] = line;
                    Matches++;
                }
            }
        }
    }
    return Matches;
}

int MovePadVer(wxArrayString& module, double from_y, double to_y)
{
    int Matches = 0;
    bool inpad = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$PAD")) == 0) {
            inpad = true;
        } else if (keyword.CmpNoCase(wxT("$EndPAD")) == 0) {
            inpad = false;
        } else if (keyword.CmpNoCase(wxT("Po")) == 0 && inpad) {
            double xpos = GetTokenDim(&line, true);
            double ypos = GetTokenDim(&line, true);
            if (Equal(ypos, from_y, 0.02)) {
                module[idx] = wxString::Format(wxT("%s %.4f %.4f"), keyword.c_str(), MM(xpos), MM(to_y));
                Matches++;
            }
        } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
            wxString section = GetSection(line, wxT("at"));
            if (section.length() > 0) {
                double xpos = GetTokenDim(&section, true);
                double ypos = GetTokenDim(&section, true);
                if (Equal(ypos, from_y, 0.02)) {
                    if (section.length() > 0) {
                        double rot = GetTokenDouble(&section);
                        section = wxString::Format(wxT("%.4f %.4f %.1f"), MM(xpos), MM(to_y), rot);
                    } else {
                        section = wxString::Format(wxT("%.4f %.4f"), MM(xpos), MM(to_y));
                    }
                    line = module[idx];
                    SetSection(line, wxT("at"), section);
                    module[idx] = line;
                    Matches++;
                }
            }
        }
    }
    return Matches;
}

/** LibraryType() returns one of VER_MIL, VER_MM or VER_S_EXPR.
 *  For footprint libraries.
 */
int LibraryType(const wxString& filename)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0)
        return VER_MM;  //??? this would presumably be modified to s-expression in a future version

    /* check for s-expression first (i.e. check whether "filename" is a
       directory name) */
    if (wxFileName::DirExists(filename) || filename.Right(10).Cmp(wxT(".kicad_mod")) == 0)
        return VER_S_EXPR;

    wxTextFile file;
    if (!file.Open(filename))
        return VER_INVALID;

    wxString line = file.GetLine(0);
    line = line.Left(19);
    if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
        file.Close();
        return VER_INVALID;
    }

    /* browse through the file until the first "command" (which should be the index) */
    int result = VER_MIL;
    for (long idx = 1; idx < (long)file.GetLineCount(); idx++) {
        line = file.GetLine(idx);
        if (line[0] == wxT('$'))
            break;
        if (line.CmpNoCase(wxT("Units mm")) == 0)
            result = VER_MM;
    }
    file.Close();

    return result;
}

/** ExistFootprint() returns whether a footprint with the given name exists in
 *  the library.
 *  \param filename     The name of the library file (a directory in the case of
 *                              s-expression library)
 *  \param name         The name of the footprint
 *  \param author       The name of the author, only used for the repository
 */
bool ExistFootprint(const wxString& filename, const wxString& name, const wxString& author)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlGet(name, author, wxT("footprints"), 0);
            return msg.length() == 0;
        #endif
    } else {
        if (wxFileName::DirExists(filename)) {
            wxFileName fname(filename, name + wxT(".kicad_mod"));
            return fname.FileExists();
        } else if (wxFileName::FileExists(filename) && filename.Right(10).CmpNoCase(".kicad_mod") == 0) {
            int pos = filename.Find(wxT(DIRSEP_CHAR), true);
            pos = (pos < 0) ? 0 : pos + 1;
            wxString modname = filename.Mid(pos);
            modname = modname.Left(modname.Length() - 10);
            return modname.CmpNoCase(name) == 0;
        } else {
            wxTextFile file;
            if (!file.Open(filename))
                return false;

            /* verify the header */
            wxString line = file.GetLine(0);
            line = line.Left(19);
            if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
                file.Close();
                return false;
            }

            long idx = 1;
            /* find the index */
            while (idx < (long)file.GetLineCount()) {
                line = file.GetLine(idx);
                idx++;
                if (line.CmpNoCase(wxT("$INDEX")) == 0)
                    break;
            }
            /* find the module in the index */
            int result = -1;
            while (idx < (long)file.GetLineCount() && result < 0) {
                line = file.GetLine(idx);
                if (line.CmpNoCase(name) == 0)
                    result = 1;     /* match found */
                else if (line[0] == wxT('$'))
                    result = 0;     /* end-of-index reached, no match */
                idx++;
            }
            file.Close();
            return (result == 1);
        }
    }
}

/** InsertFootprint() adds a footprint to a library.
 *  \param  filename    The target library name; for s-expressions, this is a
 *                      directory name.
 *  \param  name        The name of the footprint; this becomes a filename for
 *                      an s-expression library.
 *  \param  module      The complete contents of the footprint; this may be in
 *                      either legacy or s-expression format (and it must
 *                      already match the format of the library)
 *  \param  unit_mm     Whether the source in \c module uses mm as a unit (as
 *                      opposed to 0.1 mil).
 */
bool InsertFootprint(const wxString& filename, const wxString& name, const wxArrayString& module, bool unit_mm)
{
    int type = LibraryType(filename);
    if (type == VER_INVALID)
        return false;

    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            /* make a copy of the module, so that it can be translated */
            wxArrayString mod = module;
            TranslateUnits(mod, unit_mm, true);
            wxString msg = curlPut(name, wxT("footprints"), mod);
            return msg.length() == 0;
        #endif
    } else {
        wxTextFile file;
        if (type == VER_S_EXPR) {
            wxFileName fname(filename, name + wxT(".kicad_mod"));
            if (filename.Right(10).CmpNoCase(wxT(".kicad_mod")) == 0)
                fname = filename;
            if (wxFileExists(fname.GetFullPath()))
                wxRemoveFile(fname.GetFullPath());
            if (!file.Create(fname.GetFullPath()))
                return false;
            /* insert all lines of the module, indent all except the first and the last */
            bool inheader = true;
            wxString line;
            for (unsigned idx = 0; idx < module.Count(); idx++) {
                if (inheader || idx == module.Count() - 1)
                    line = module[idx];
                else
                    line = wxT("  ") + module[idx];
                if (line[0] != wxT('#'))
                    inheader = false;
                file.InsertLine(line, idx);
            }
        } else {
            /* check whether "filename" uses 1/10 mil or mm dimensions */
            bool tgt_mm = (type == VER_MM);

            if (!file.Open(filename))
                return false;
            /* no need to verify the header, function LibraryType() did that already */

            /* insert name in the index */
            wxString line;
            long idx = 1;
            /* find the start of the index */
            while (idx < (long)file.GetLineCount()) {
                line = file.GetLine(idx);
                idx++;
                if (line.CmpNoCase(wxT("$INDEX")) == 0)
                    break;
            }
            /* find the position to store the name */
            while (idx < (long)file.GetLineCount()) {
                line = file.GetLine(idx);
                int cmp = line.CmpNoCase(name);
                if (line[0] == wxT('$') || cmp > 0) {
                    file.InsertLine(name, idx);
                    break;
                } else if (cmp == 0) {
                    return false;   /* symbol already present, insertion fails */
                }
                idx++;
            }
            /* skip to the end of the index */
            while (idx < (long)file.GetLineCount()) {
                line = file.GetLine(idx);
                idx++;
                if (line.CmpNoCase(wxT("$EndINDEX")) == 0)
                    break;
            }

            /* make a copy of the module, so that it can be translated */
            wxArrayString mod = module;
            TranslateUnits(mod, unit_mm, tgt_mm);

            /* now find the insertion position for the module itself (this probably
                 does not have to stay sorted, but we do it anyway) */
            wxString keyword;
            while (idx < (long)file.GetLineCount()) {
                line = file.GetLine(idx);
                if (line[0] == '$' && line.Left(7).CmpNoCase(wxT("$MODULE")) == 0) {
                    GetToken(&line);
                    keyword = GetToken(&line);
                    if (keyword.CmpNoCase(name) > 0)
                        break;
                } else if (line.CmpNoCase(wxT("$EndLIBRARY")) == 0) {
                    break;
                }
                idx++;
            }
            /* insert all lines of the module */
            for (long l = 0; l < (long)mod.Count(); l++) {
                file.InsertLine(mod[l], idx);
                idx++;
            }
        }

        file.Write();
        file.Close();
        return true;
    }
}

/* For footprints */
bool RemoveFootprint(const wxString& filename, const wxString& name)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlDelete(name, wxT("footprints"));
            return msg.length() == 0;
        #endif
    } else if (wxFileName::DirExists(filename)) {
        wxFileName fname(filename, name + wxT(".kicad_mod"));
        return wxFileExists(fname.GetFullPath()) && wxRemoveFile(fname.GetFullPath());
    } else if (wxFileName::FileExists(filename) && filename.Right(10).CmpNoCase(".kicad_mod") == 0) {
        int pos = filename.Find(wxT(DIRSEP_CHAR), true);
        pos = (pos < 0) ? 0 : pos + 1;
        wxString modname = filename.Mid(pos);
        modname = modname.Left(modname.Length() - 10);
        if (modname.CmpNoCase(name) != 0)
            return false;
        return wxRemoveFile(filename);
    } else {
        wxTextFile file;
        if (!file.Open(filename))
            return false;

        /* verify the header */
        wxString line = file.GetLine(0);
        line = line.Left(19);
        if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
            file.Close();
            return false;
        }

        long idx = 1;
        /* find the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            idx++;
            if (line.CmpNoCase(wxT("$INDEX")) == 0)
                break;
        }
        /* find the module in the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            if (line.CmpNoCase(name) == 0) {
                file.RemoveLine(idx);
                break;
            } else if (line[0] == wxT('$')) {
                file.Close();
                return false;   /* module not found in the index, skip other operations */
            }
            idx++;
        }
        /* skip to the end of the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            idx++;
            if (line.CmpNoCase(wxT("$EndINDEX")) == 0)
                break;
        }
        /* now find the module itself */
        wxString keyword;
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            if (line[0] == '$' && line.Left(7).CmpNoCase(wxT("$MODULE")) == 0) {
                keyword = GetToken(&line);
                if (line.CmpNoCase(name) == 0)
                    break;
            }
            idx++;
        }
        /* delete all lines of the module */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            file.RemoveLine(idx);
            if (line.Left(10).CmpNoCase(wxT("$EndMODULE")) == 0)
                break;
        }

        file.Write();
        file.Close();
    }
    return true;
}

/* For footprints */
bool RenameFootprint(wxArrayString* module, const wxString& oldname, const wxString& newname)
{
    for (int idx = 0; idx < (int)module->Count(); idx++) {
        wxString line = (*module)[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("$MODULE")) == 0
                || keyword.CmpNoCase(wxT("$EndMODULE")) == 0
                || keyword.CmpNoCase(wxT("Li")) == 0)
        {
            (*module)[idx] = keyword + wxT(" ") + newname;
        } else if (keyword.CmpNoCase(wxT("T0")) == 0
                             || keyword.Cmp(wxT("Na")) == 0
                             || keyword.Cmp(wxT("(module")) == 0
                             || keyword.Cmp(wxT("(model")) == 0
                             || keyword.Cmp(wxT("(fp_text")) == 0
                             )
        {
            (*module)[idx].Replace(oldname, newname);
        }
    }
    return true;
}

/* For footprints */
bool RenameFootprint(const wxString& filename, const wxString& oldname, const wxString& newname)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxArrayString module;
            wxString msg = curlGet(oldname, wxEmptyString, wxT("footprints"), &module);
            if (msg.length() > 0)
                return false;
            RenameFootprint(&module, oldname, newname);
            msg = curlPut(newname, wxT("footprints"), module);
            if (msg.length() > 0)
                return false;
            msg = curlDelete(oldname, wxT("footprints"));
            return msg.length() == 0;
        #endif
    } else if (wxFileName::DirExists(filename)) {
        wxFileName old_fname(filename, oldname + wxT(".kicad_mod"));
        wxFileName new_fname(filename, newname + wxT(".kicad_mod"));
        if (!wxRenameFile(old_fname.GetFullPath(), new_fname.GetFullPath(), true))
            return false;
        wxTextFile file;
        if (!file.Open(new_fname.GetFullPath()))
            return false;
        wxArrayString module;
        /* find the index */
        for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
            wxString line = file.GetLine(idx);
            module.Add(line);
        }
        RenameFootprint(&module, oldname, newname);
        file.Clear();
        for (unsigned idx = 0; idx < module.Count(); idx++)
            file.AddLine(module[idx]);
        file.Write();
        file.Close();
    } else {
        wxTextFile file;
        if (!file.Open(filename))
            return false;

        /* verify the header */
        wxString line = file.GetLine(0);
        line = line.Left(19);
        if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
            file.Close();
            return false;
        }

        long idx = 1;
        /* find the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            idx++;
            if (line.CmpNoCase(wxT("$INDEX")) == 0)
                break;
        }
        /* find the module in the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            if (line.CmpNoCase(oldname) == 0) {
                file.RemoveLine(idx);
                file.InsertLine(newname, idx);
                break;
            } else if (line[0] == wxT('$')) {
                file.Close();
                return false;   /* module not found in the index, skip other operations */
            }
            idx++;
        }
        /* skip to the end of the index */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            idx++;
            if (line.CmpNoCase(wxT("$EndINDEX")) == 0)
                break;
        }
        /* now find the module itself */
        wxString keyword;
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            if (line[0] == '$' && line.Left(7).CmpNoCase(wxT("$MODULE")) == 0) {
                keyword = GetToken(&line);
                if (line.CmpNoCase(oldname) == 0)
                    break;
            }
            idx++;
        }
        /* go through the module, translate a few strings */
        while (idx < (long)file.GetLineCount()) {
            line = file.GetLine(idx);
            keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("$MODULE")) == 0
                || keyword.CmpNoCase(wxT("$EndMODULE")) == 0
                || keyword.CmpNoCase(wxT("Li")) == 0)
            {
                file.RemoveLine(idx);
                line = keyword + wxT(" ") + newname;
                file.InsertLine(line, idx);
            } else if (keyword.CmpNoCase(wxT("T0")) == 0 || keyword.Cmp(wxT("Na")) == 0) {
                line.Replace(oldname, newname);
                file.RemoveLine(idx);
                line = keyword + wxT(" ") + line;
                file.InsertLine(line, idx);
            }
            if (keyword.CmpNoCase(wxT("$EndMODULE")) == 0)
                break;
            idx++;
        }

        file.Write();
        file.Close();
    }
    return true;
}

/** LoadFootprint() loads a footprint.
 *  \param filename     The name of the library (which is a directory for
 *                      s-expression libraries), or the repository string
 *  \param name         The footprint name
 *  \param author       The name of the author of the footprint; only used for the
 *                      repository
 *  \param striplink    If true, the link with the template is stripped from the
 *                      module
 *  \param module       The array that will hold the footprint data on output
 *  \param version      The library file format version
 */
bool LoadFootprint(const wxString& filename, const wxString& name, const wxString& author,
                   bool striplink, wxArrayString* module, int* version)
{
    wxASSERT(module != NULL);
    module->Clear();
    wxASSERT(version != NULL);
    *version = VER_INVALID;

    bool result = false;

    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlGet(name, author, wxT("footprints"), module);
            if (msg.length() == 0)
                *version = VER_MM;  //??? this will presumably change to VER_S_EXPR in a future version
            result = (msg.length() == 0 && module->Count() > 0);
        #endif
    } else {
        wxTextFile file;
        /* check whether this is an s-expression library */
        if (wxFileName::DirExists(filename) || filename.Right(10).Cmp(wxT(".kicad_mod")) == 0) {
            wxString mod_name;
            if (wxFileName::DirExists(filename)) {
                /* the symbol name may contain periods, and the part after the last period may then be
                   misdetected as an extension, if we use fname.SetExt() -> so we concatenate the
                   extension rather than use SetExt() */
                wxFileName fname(filename, name + wxT(".kicad_mod"));
                mod_name = fname.GetFullPath();
            } else {
                mod_name = filename;
            }
            if (!file.Open(mod_name))
                return false;
            *version = VER_S_EXPR;
            /* s-expression is a free-format; for ease of parsing, the contents are
                 reformatted; the first step is to gather all data in a single (long)
                 string */
            wxString total = wxEmptyString;
            wxString line;
            for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
                line = file.GetLine(idx);
                line.Trim(false);           /* remove leading and trailing white-space */
                line.Trim(true);
                if (total.length() > 0 && line.length() > 0 && line[0] != wxT(')'))
                    total += wxT(" ");  /* put one space between sections/keywords */
                total += line;
                if (line.Find(wxT('#')) >= 0)
                    total += wxT(EoC);  /* if a comment appears in the string, append a special "end-of-comment" token */
            }
            file.Close();
            /* remove spaces after a '(' (KiCad does not generate these, so it is merely
                 to be extra sure) */
            total.Replace(wxT("( "), wxT("("));
            /* handle any leading comments */
            while (total[0] == wxT('#')) {
                int pos = total.Find(wxT(EoC));
                wxASSERT(pos > 0);
                line = total.Left(pos);
                module->Add(line);
                total = total.Mid(pos + 1);
            }
            /* reformat with only a single "indentation level" (although no indentation
                 is added) */
            total.Trim(true);   /* trim trailing */
            total.Trim(false);  /* trim leading */
            wxASSERT(total[0] == wxT('('));
            unsigned start = 1;
            bool instring = false;
            while (start < total.length() && (!(total[start] == wxT('(') || total[start] == wxT(')')) || instring)) {
                if (total[start] == wxT('"'))
                    instring = !instring;
                start++;
            }
            line = total.Left(start);
            line.Trim(true);
            module->Add(line);
            total = total.Mid(start);
            total.Trim(false);
            while (total[0] != wxT(')')) {
                wxASSERT(total[0] == wxT('('));
                int level = 0;
                instring = false;
                for (start = 1; start < total.length() && level >= 0; start++) {
                    if (total[start] == wxT('"'))
                        instring = !instring;
                    if (total[start] == wxT('(') && !instring)
                        level++;
                    else if (total[start] == wxT(')') && !instring)
                        level--;
                }
                line = total.Left(start);
                /* remove comments (right now, we only support header comments) */
                int pos;
                while ((pos = line.Find(wxT('#'))) >= 0) {
                    int pos2 = line.Find(wxT(EoC));
                    wxASSERT(pos2 > pos);
                    line.Remove(pos, pos2 - pos + 1);
                }
                module->Add(line);
                total = total.Mid(start);
                total.Trim(false);  /* remove the space after the closing ')' */
            }
            module->Add(total); /* this is the final ')' that closes the module definition */
            result = (module->Count() > 0);
        } else {
            /* legacy library, open the library */
            if (!file.Open(filename))
                return false;

            /* verify the header */
            wxString line = file.GetLine(0);
            line = line.Left(19);
            if (line.CmpNoCase(wxT("PCBNEW-LibModule-V1")) != 0) {
                file.Close();
                return false;
            }

            /* search for the unit (this may be absent) */
            wxASSERT(version != NULL);
            *version = VER_MIL;
            unsigned idx = 1;
            while (idx < file.GetLineCount()) {
                line = file.GetLine(idx);
                idx++;
                if (line[0] == '$')
                    break;  /* the unit must appear before the index, and before any module */
                wxString keyword = GetToken(&line);
                if (keyword.CmpNoCase(wxT("Units")) == 0 && line.CmpNoCase(wxT("mm")) == 0)
                    *version = VER_MM;
            }

            /* search the module */
            while (idx < file.GetLineCount()) {
                line = file.GetLine(idx);
                idx++;
                if (line[0] != '$')
                    continue;
                wxString keyword = line.Left(7);
                if (keyword.CmpNoCase(wxT("$MODULE")) != 0)
                    continue;
                line.Trim();                    /* remove trailing white-space */
                wxString modname = line.Mid(8);
                modname.Trim(false);    /* remove leading white-space (trailing white-space was already removed) */
                if (modname.CmpNoCase(name) == 0) {
                    module->Add(line);  /* already add the header line to the saved data */
                    break;
                }
            }

            /* read the module */
            while (idx < file.GetLineCount()) {
                line = file.GetLine(idx);
                module->Add(line);
                line.Trim();
                line.Trim(false);
                if (line[0] == '$' && line.Left(10).CmpNoCase(wxT("$EndMODULE")) == 0)
                    break;
                idx++;
            }

            file.Close();
            result = (module->Count() > 0);
        }
    }

    if (result && striplink) {
        for (unsigned idx = 0; idx < module->Count(); idx++) {
            wxString line = (*module)[idx];
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("AR")) == 0)
                (*module)[idx] = keyword;
            else if (keyword.Cmp(wxT("#template")) == 0)
                (*module)[idx] = wxT("#");
        }
    }
    return result;
}

/* For templates (footprints & symbols) */
bool LoadTemplate(const wxString& templatename, wxArrayString* module, bool SymbolMode)
{
    wxASSERT(module);
    module->Clear();

    wxString ext = SymbolMode ? wxT(".st") : wxT(".mt");
    wxString path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + templatename + ext;
    wxTextFile file;
    if (file.Open(path)) {
        wxString line;
        for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
            wxString segm = file.GetLine(idx);
            /* strip trailing comments */
            int pos = segm.Find(wxT('#'));
            if (pos >= 0)
                segm = segm.Left(pos);
            segm.Trim();    /* remove leading and trailing white space */
            segm.Trim(false);
            line += segm;
            /* check for line continuation before adding the line to the list */
            size_t len = line.length();
            if (len > 0) {
                if (line[len - 1] == wxT('\\')) {
                    line[len - 1] = wxT(' ');
                } else {
                    module->Add(line);
                    line.Clear();
                }
            }
        }
        file.Close();
    }
    return module->Count() > 0;
}

/* For templates (sub-function) and for generated VRML files
   if a key occurs multiple times, the "sequence" parameter allows to run through them */
wxString GetFileHeaderField(const wxString& path, const wxString& key, int sequence)
{
    if (!wxFileExists(path))
        return wxEmptyString;
    wxTextFile file;
    wxString result = wxEmptyString;
    if (file.Open(path)) {
        wxString line;
        for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
            line = file.GetLine(idx);
            if (line[0] != wxT('#'))
                break;  /* end of the header found, return an empty string */
            wxString token = GetToken(&line);
            token = token.Mid(1);   /* strip of '#' */
            if (token.Cmp(key) == 0) {
                if (sequence-- == 0) {
                    line.Trim(true);
                    result = line;
                    /* test for line continuation */
                    size_t len = result.length();
                    while (len > 0 && result[len - 1] == wxT('\\')) {
                        result[len - 1] = wxT(' '); /* replace \ by space */
                        if (++idx >= file.GetLineCount())
                            break;          /* no more lines in the file, cannot continue (this is an error in the template) */
                        line = file.GetLine(idx);
                        if (line[0] != wxT('#'))
                            break;  /* end of the header found (this is also an error in the template) */
                        line = line.Mid(1); /* erase # */
                        line.Trim(false);   /* remove leading and trailing whitespace */
                        line.Trim(true);
                        result += line;
                        len = result.length();
                    }
                    result.Trim(true);
                    break;  /* token found, no need to search further */
                }
            }
        }
        file.Close();
    }
    return result;
}

/* For templates (footprints & symbols) */
wxString GetTemplateHeaderField(const wxString& templatename, const wxString& key, bool SymbolMode, int sequence)
{
    wxString ext = SymbolMode ? wxT(".st") : wxT(".mt");
    wxString path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + templatename + ext;
    return GetFileHeaderField(path, key, sequence);
}

/* get sections for pins (currently for symbols only) */
void GetTemplateSections(const wxString& templatename, PinSection sections[], size_t max)
{
    wxASSERT(sections);
    int idx = 0;
    if (templatename.Length() > 0) {
        while (idx < (int)max) {
            wxString sect = GetTemplateHeaderField(templatename, wxT("section"), true, idx);
            if (sect.Length() == 0)
                break;
            /* extract the name, the side and the criterion */
            wxString name = GetToken(&sect);
            wxString side = GetToken(&sect);
            double crit;
            sect.ToDouble(&crit);
            sections[idx].SetCriterion(name, side, crit);
            idx++;
        }
    }
    /* clear any remaining sections */
    while (idx < (int)max) {
        sections[idx].Clear();
        idx++;
    }
}

bool ValidPinCount(long pins, const wxString& templatename, bool symbolmode)
{
    wxASSERT(templatename.length() > 0);
    if (templatename.length() == 0 || pins == 0)
        return false;
    wxString field = GetTemplateHeaderField(templatename, wxT("pins"), symbolmode);
    wxASSERT(symbolmode || field.length() > 0); /* if this header is absent, the field for
                                                   the number of pins is read-only for a footprint */
    if (field.length() == 0)
        return symbolmode;      /* for symbols, accept all pin counts if this field is absent */

    long cur = 0, prev = 0;
    int count = 0;
    while (field.length() > 0) {
        wxString item;
        item = GetToken(&field);
        if (item.Cmp(wxT("...")) == 0) {
            if (count < 2)
                return false;   /* this means that the template definition is incorrect */
            long dif = cur - prev;
            if (dif <= 0)
                return false;   /* again, template definition is incorrect */
            if (cur % dif == pins % dif)
                return true;
        } else {
            prev = cur;
            item.ToLong(&cur);
        }
        if (cur == pins)
            return true;
        if (cur > pins)
            return false;       /* no point in looking further */
        count++;
    }
    return false;
}

static const wxChar* rpn_errors[] = { wxT("(none)"), wxT("empty stack"),
    wxT("multiple results"), wxT("underflow"), wxT("overflow"),
    wxT("invalid variable"), wxT("invalid function"), wxT("invalid operator") };

/* For footprints */
bool FootprintFromTemplate(wxArrayString* module, const wxArrayString& templat,
                            RPNexpression& rpn, bool bodyonly)
{
    int errorcount = 0;

    wxASSERT(module);
    if (module->Count() < 3)
        bodyonly = false;   /* there cannot possibly be a valid module in so few lines */
    if (bodyonly) {
        /* erase all lines up to the first pad */
        while (module->Item(0).CmpNoCase(wxT("$PAD")) != 0)
            module->RemoveAt(0);
    } else {
        module->Clear();
    }

    /* the regular expression matches "{...}" where "..." is anything; this must
       be a non-greedy match, because multiple expressions between braces can
       exist on a single line */
    wxRegEx flt(wxT("\\{([^\\}]+?)\\}"), wxRE_ADVANCED);
    wxASSERT(flt.IsValid());

    wxString endlabel = wxEmptyString;

    /* copy the shape (body) */
    unsigned tidx = 0;
    unsigned padstart = 0;
    unsigned bodyline = 0;
    while (tidx < templat.Count()) {
        wxString line = templat[tidx];
        line.Trim();
        /* check for the start of a pad (pads are handled separately) */
        if (line.CmpNoCase(wxT("$PAD")) == 0 || line.Left(4).Cmp(wxT("(pad")) == 0) {
            padstart = tidx;
            break;
        }
        /* handle skipping conditional blocks */
        if (endlabel.length() > 0 || line[0] == wxT(':')) {
            if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                endlabel = wxEmptyString;   /* label matched, stop skipping */
            tidx++;
            continue;
        }
        /* handle conditional expression */
        if (line[0] == wxT('{') && line[1] == wxT('?')) {
            flt.Matches(line);
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(start == 0 && len > start && len <= line.length());
            wxASSERT(line[start + 1] == wxT('?'));
            wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
            if (expr[0] == wxT(':')) {
                /* there is an "end label" */
                for (start = 1; start < expr.length() && expr[start] > wxT(' '); start++)
                    /* nothing, skip the label name */;
                endlabel = expr.Left(start);
                expr = expr.Mid(start);
            }
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                line = wxEmptyString;
            } else {
                line.replace(0, len, wxT(""));
                endlabel = wxEmptyString;
            }
        }
        for (int matchindex = 0; flt.Matches(line); matchindex++) {
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(len > 0 && start + len <= line.length());
            wxString expr = line.Mid(start + 1, len - 2);
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK) {
                RPNvalue val = rpn.Value();
                if (val.Alpha()) {
                    expr = wxString::FromUTF8(val.Text());
                } else {
                    expr = wxString::Format(wxT("%.4f"), val.Double());
                    StripTrailingZeros(&expr);
                }
            } else if (err == RPN_EMPTY) {
                /* this means that the expression ended with an assignment */
                expr = wxEmptyString;
            } else {
                wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression \"%s\""),
                                                rpn_errors[err], tidx + 1, expr.c_str());
                wxMessageBox(msg);
                expr = wxEmptyString;
                errorcount++;
            }
            str_replace(line, start, len, expr);
        }
        line.Trim();
        if (line.length() > 0)
            module->Insert(line, bodyline++);
        tidx++;
    }
    if (endlabel.length() > 0) {
        wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
        errorcount++;
    }

    wxASSERT(padstart > 0); /* if it isn't, no pads are detected in the template, which is highly likely wrong */
    int tailstart = 0;
    if (padstart > 0 && !bodyonly) {
        /* get the number of pads defined for this shape */
        rpn.Set("PT");
        int pads = (rpn.Parse() == RPN_OK) ? (int)(rpn.Value().Double() + 0.1) : 0; /* + 0.1 to avoid rounding errors */
        rpn.Set("PTA");
        int pads_aux = (rpn.Parse() == RPN_OK) ? (int)(rpn.Value().Double() + 0.1) : 0; /* + 0.1 to avoid rounding errors */
        for (int pad = 1; pad <= pads + pads_aux; pad++) {
            endlabel = wxEmptyString;
            tidx = padstart;
            rpn.SetVariable(RPNvariable("PN", pad));
            while (tidx < templat.Count()) {
                wxString line = templat[tidx];
                line.Trim();
                /* handle skipping conditional blocks */
                if (endlabel.length() > 0 || line[0] == wxT(':')) {
                    if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                        endlabel = wxEmptyString;   /* label matched, stop skipping */
                    tidx++;
                    continue;
                }
                /* handle conditional expression */
                if (line[0] == wxT('{') && line[1] == wxT('?')) {
                    flt.Matches(line);
                    size_t start, len;
                    flt.GetMatch(&start, &len, 0);
                    wxASSERT(start == 0 && len > start && len <= line.length());
                    wxASSERT(line[start + 1] == wxT('?'));
                    wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
                    if (expr[0] == wxT(':')) {
                        /* there is an "end label" */
                        for (start = 1; start < expr.length() && expr[start] > wxT(' '); start++)
                            /* nothing, skip the label name */;
                        endlabel = expr.Left(start);
                        expr = expr.Mid(start);
                    }
                    rpn.Set(expr.utf8_str());
                    RPN_ERROR err = rpn.Parse();
                    if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                        line = wxEmptyString;
                    } else {
                        line.replace(0, len, wxT(""));
                        endlabel = wxEmptyString;
                    }
                }
                for (int matchindex = 0; flt.Matches(line); matchindex++) {
                    size_t start, len;
                    flt.GetMatch(&start, &len, 0);
                    wxASSERT(len > 0 && start + len <= line.length());
                    wxString expr = line.Mid(start + 1, len - 2);
                    /* there should not be any non-numeric expressions in a pad definition;
                       we do not handle them */
                    rpn.Set(expr.utf8_str());
                    RPN_ERROR err = rpn.Parse();
                    if (err == RPN_OK) {
                        RPNvalue val = rpn.Value();
                        if (val.Alpha()) {
                            expr = wxString::FromUTF8(val.Text());
                        } else {
                            expr = wxString::Format(wxT("%.4f"), val.Double());
                            StripTrailingZeros(&expr);
                        }
                    } else if (err == RPN_EMPTY) {
                        /* this means that the expression ended with an assignment */
                        expr = wxEmptyString;
                    } else {
                        wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression \"%s\""),
                                                        rpn_errors[err], tidx + 1, expr.c_str());
                        wxMessageBox(msg);
                        expr = wxEmptyString;
                        errorcount++;
                    }
                    str_replace(line, start, len, expr);
                }
                line.Trim();
                if (line.length() > 0)
                    module->Add(line);
                /* check for the end of a pad (templates in the s-expresssion format
                   must have the terminating ')' on a line of its own, and a parenthesis
                   on its own should never occur elsewhere within a pad definition) */
                line.Trim(false);
                if (line.CmpNoCase(wxT("$EndPAD")) == 0 || line.Cmp(wxT(")")) == 0) {
                    tailstart = tidx + 1;
                    break;
                }
                tidx++;
            }
            if (endlabel.length() > 0) {
                wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
                errorcount++;
            }
            /* after running the first aux-pad, check the PTA variable again,
               but note that the first aux-pad may be the last numbered pad (so
               the first aux-pad can be pads or pads + 1) */
            if (pad >= pads) {
                rpn.Set("PTA");
                pads_aux = (rpn.Parse() == RPN_OK) ? (int)(rpn.Value().Double() + 0.1) : 0; /* + 0.1 to avoid rounding errors */
            }
        }
    }

    /* finish the template */
    endlabel = wxEmptyString;
    wxASSERT(tailstart > 0 || bodyonly);    /* there should be at least an $EndMODULE definition following the pads */
    if (tailstart > 0 && !bodyonly) {
        tidx = tailstart;
        while (tidx < templat.Count()) {
            wxString line = templat[tidx];
            line.Trim();
            /* handle skipping conditional blocks */
            if (endlabel.length() > 0 || line[0] == wxT(':')) {
                if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                    endlabel = wxEmptyString;   /* label matched, stop skipping */
                tidx++;
                continue;
            }
            /* handle conditional expression */
            if (line[0] == wxT('{') && line[1] == wxT('?')) {
                flt.Matches(line);
                size_t start, len;
                flt.GetMatch(&start, &len, 0);
                wxASSERT(start == 0 && len > start && len <= line.length());
                wxASSERT(line[start + 1] == wxT('?'));
                wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
                if (expr[0] == wxT(':')) {
                    /* there is an "end label" */
                    for (start = 1; start < expr.length() && expr[start] > wxT(' '); start++)
                        /* nothing, skip the label name */;
                    endlabel = expr.Left(start);
                    expr = expr.Mid(start);
                }
                rpn.Set(expr.utf8_str());
                RPN_ERROR err = rpn.Parse();
                if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                    line = wxEmptyString;
                } else {
                    line.replace(0, len, wxT(""));
                    endlabel = wxEmptyString;
                }
            }
            for (int matchindex = 0; flt.Matches(line); matchindex++) {
                size_t start, len;
                flt.GetMatch(&start, &len, 0);
                wxASSERT(len > 0 && start + len <= line.length());
                wxString expr = line.Mid(start + 1, len - 2);
                rpn.Set(expr.utf8_str());
                RPN_ERROR err = rpn.Parse();
                if (err == RPN_OK) {
                    RPNvalue val = rpn.Value();
                    if (val.Alpha()) {
                        expr = wxString::FromUTF8(val.Text());
                    } else {
                        expr = wxString::Format(wxT("%.4f"), val.Double());
                        StripTrailingZeros(&expr);
                    }
                } else if (err == RPN_EMPTY) {
                    /* this means that the expression ended with an assignment */
                    expr = wxEmptyString;
                } else {
                    wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression %s"),
                                                    rpn_errors[err], tidx + 1, expr.c_str());
                    wxMessageBox(msg);
                    expr = wxEmptyString;
                    errorcount++;
                }
                str_replace(line, start, len, expr);
            }
            line.Trim();
            if (line.length() > 0)
                module->Add(line);
            tidx++;
        }
        if (endlabel.length() > 0) {
            wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
            errorcount++;
        }
    }

    return errorcount == 0;
}

/* returns a full path to a VRML file, given a module (that contains a relative
   path) and a library path; for the repository, only the base name is returned
   For footprints and 3D models */
wxString GetVRMLPath(const wxString& library, const wxArrayString& module)
{
    wxString rpath = wxEmptyString;

    /* check that there is a VRML file in the module */
    bool in3dshape = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString token = GetToken(&line);
        if (in3dshape && token.CmpNoCase(wxT("Na")) == 0)
            rpath = GetToken(&line);
        else if (token.Cmp(wxT("$SHAPE3D")) == 0)
            in3dshape = true;
        else if (token.Cmp(wxT("(module")) == 0)
            rpath = GetToken(&line);
    }
    if (rpath.length() == 0)
        return wxEmptyString;

    if (library.CmpNoCase(LIB_REPOS) == 0) {
        /* for the repository, use only the base name of the library (ignore any
             relative path in the module */
        int idx = rpath.Find('/', true);
        if (idx < 0)
            idx = rpath.Find('\\', true);
        if (idx >= 0)
            rpath = rpath.Mid(idx + 1);
    } else {
        /* for local libraries, first check whether the "relative" path actually is
           an absolute path (there is nothing to do in that case) */
        if (rpath[0] != '/' && (rpath.length() < 3 || rpath[1] != ':' || (rpath[2] != '\\' && rpath[2] != '/'))) {
            /* so it is a true relative path; strip the filename from the local
               library path, add the "packages3d" path and the relative path */
            int idx = library.Find('/', true);
            if (idx < 0)
                idx = library.Find('\\', true);
            if (idx >= 0)
                rpath = library.Left(idx + 1) + wxT("packages3d/") + rpath;
        }
        /* adhere to the proper path separations */
        rpath.Replace(wxT("\\\\"), wxT("\\"));
        #if DIRSEP_CHAR != '/'
            rpath.Replace(wxT("/"), wxT(DIRSEP_STR));
        #endif
        #if DIRSEP_CHAR != '\\'
            rpath.Replace(wxT("\\"), wxT(DIRSEP_STR));
        #endif
        /* add the extension, if not present */
        if (rpath.Length() < 4 || rpath.Right(4).CmpNoCase(wxT(".wrl")) != 0)
            rpath = rpath + wxT(".wrl");
    }

    return rpath;
}

/* For footprintts and 3D models */
bool CopyVRMLfile(const wxString& source, const wxString& target,
                            const wxString& author, const wxArrayString& module)
{
    wxString sourcepath = GetVRMLPath(source, module);
    if (sourcepath.Length() == 0)
        return false;
    wxString targetpath = GetVRMLPath(target, module);
    wxASSERT(targetpath.length() > 0);  /* if there is a source path, there must be a target path */
    if (sourcepath.Cmp(targetpath) == 0)
        return false;       /* source and target point to the same filename (or repository entry) */
    if (!wxFileExists(sourcepath))
        return false;

    bool result = false;
    if (source.CmpNoCase(LIB_REPOS) == 0) {
        #if !defined NO_CURL
            /* get the model */
            wxArrayString model;
            wxString msg = curlGet(sourcepath, author, wxT("models"), &model);
            if (msg.length() == 0) {
                /* write the model to file */
                wxTextFile file;
                if (file.Open(sourcepath) || file.Create(sourcepath)) {
                    file.Clear();   /* delete any existing contents */
                    for (unsigned idx = 0; idx < model.Count(); idx++)
                        file.AddLine(model[idx]);
                    file.Write();
                    file.Close();
                    result = true;
                }
            }
        #endif
    } else if (target.CmpNoCase(LIB_REPOS) == 0) {
        #if !defined NO_CURL
            /* load the model */
            wxArrayString model;
            wxTextFile file;
            if (file.Open(sourcepath)) {
                for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
                    wxString line = file.GetLine(idx);
                    line.Trim();
                    line.Trim(false);
                    model.Add(line);
                }
                file.Close();
                /* upload the model */
                wxString msg = curlPut(targetpath, wxT("models"), model);
                if (msg.length() == 0)
                    result = true;
            }
        #endif
    } else {
        /* check whether to create the path */
        wxFileName fname(targetpath);
        if (!wxDirExists(fname.GetPath()))
            wxFileName::Mkdir(fname.GetPath(), 0777, wxPATH_MKDIR_FULL);
        result = ::wxCopyFile(sourcepath, targetpath);
    }
    return result;
}

/* VRMLFromTemplate() creates the 3d model from a template and a fully
   initialized RPN engine --the routine gets the pin count from the "PT"
   variable */
bool VRMLFromTemplate(const wxString vrmlpath, const wxString& templatename, RPNexpression& rpn)
{
    /* create full path to the template */
    wxString templatepath = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + templatename + wxT(".vt");

    /* load the template */
    wxTextFile file;
    if (!file.Open(templatepath))
        return false;
    wxArrayString templat;
    for (unsigned idx = 0; idx < file.GetLineCount(); idx++) {
        wxString line = file.GetLine(idx);
        line.Trim();
        templat.Add(line);
    }
    file.Close();

    /* run over the template, create the output */

    /* the regular expression matches "{...}" where "..." is anything; this must
         be a non-greedy match, because multiple expressions between braces can
         exist on a single line */
    wxRegEx flt(wxT("\\{([^ ][^\\}]+?)\\}"), wxRE_ADVANCED);
    wxASSERT(flt.IsValid());

    wxString endlabel = wxEmptyString;
    int errorcount = 0;
    wxArrayString model;

    /* copy the body */
    bool inside_pins = false;
    unsigned tidx = 0;
    while (tidx < templat.Count()) {
        wxString line = templat[tidx];
        if (line.Length() == 0) {
            tidx++;
            continue;
        }
        /* copy up to the line "Shape {" inside the "DEF pins Transform {" section,
           which means that the "DEF pins" must be found first */
        if (line.Left(8).CmpNoCase(wxT("DEF pins")) == 0)
            inside_pins = true;
        if (inside_pins) {
            wxString trimmed = line;
            trimmed.Trim(false);
            if (trimmed.CmpNoCase(wxT("Shape {")) == 0)
                break;
        }
        /* handle skipping conditional blocks */
        if (endlabel.length() > 0 || line[0] == wxT(':')) {
            if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                endlabel = wxEmptyString;   /* label matched, stop skipping */
            tidx++;
            continue;
        }
        /* handle conditional expression */
        if (line[0] == wxT('{') && line[1] == wxT('?')) {
            flt.Matches(line);
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(start == 0 && len > start && len <= line.length());
            wxASSERT(line[start + 1] == wxT('?'));
            wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
            if (expr[0] == wxT(':')) {
                /* there is an "end label" */
                for (start = 1; start < expr.length() && expr[start] > wxT(' '); start++)
                    /* nothing, skip the label name */;
                endlabel = expr.Left(start);
                expr = expr.Mid(start);
            }
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                line = wxEmptyString;
            } else {
                line.replace(0, len, wxT(""));
                endlabel = wxEmptyString;
            }
        }
        for (int matchindex = 0; flt.Matches(line); matchindex++) {
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(len > 0 && start + len <= line.length());
            wxString expr = line.Mid(start + 1, len - 2);
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK) {
                RPNvalue val = rpn.Value();
                if (val.Alpha()) {
                    expr = wxString::FromUTF8(val.Text());
                } else {
                    expr = wxString::Format(wxT("%.4f"), val.Double());
                    StripTrailingZeros(&expr);
                }
            } else if (err == RPN_EMPTY) {
                /* this means that the expression ended with an assignment */
                expr = wxEmptyString;
            } else {
                wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression \"%s\""),
                                                rpn_errors[err], tidx + 1, expr.c_str());
                wxMessageBox(msg);
                expr = wxEmptyString;
                errorcount++;
            }
            str_replace(line, start, len, expr);
        }
        line.Trim();
        if (line.length() > 0)
            model.Add(line);
        tidx++;
    }
    if (endlabel.length() > 0) {
        wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
        errorcount++;
    }

    /* update the "model" line in the header, to specify options (height) */
    if (rpn.ExistVariable("BH")) {
        rpn.Set("BH");
        if (rpn.Parse() == RPN_OK) {
            for (unsigned idx = 0; idx < model.Count(); idx++) {
                wxASSERT(model[idx].Length() > 0);  /* zero-length lines are not added to the output */
                if (model[idx].Left(6).Cmp(wxT("#model")) == 0) {
                    model[idx] += wxString::Format(wxT(" {height %.1f}"), rpn.Value().Double());
                    break;  /* no need to search further */
                }
            }
        }
    }

    /* repeat for every pin */
    long pins = 0;
    if (rpn.ExistVariable("PT")) {
        rpn.Set("PT");
        if (rpn.Parse() == RPN_OK)
            pins = (long)(rpn.Value().Double() + 0.1);
    }
    unsigned pins_start = tidx;
    unsigned pins_end = 0;
    for (long pin = 1; pin <= pins; pin++) {
        tidx = pins_start;
        rpn.SetVariable(RPNvariable("PN", (double)pin));
        while (tidx < templat.Count()) {
            wxString line = templat[tidx];
            /* copy up to a line with only a "]" */
            wxString trimmed = line;
            trimmed.Trim(false);
            if (trimmed.Cmp(wxT("]")) == 0) {
                pins_end = tidx;
                break;
            }
            /* handle skipping conditional blocks */
            if (endlabel.length() > 0 || line[0] == wxT(':')) {
                if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                    endlabel = wxEmptyString;   /* label matched, stop skipping */
                tidx++;
                continue;
            }
            /* handle conditional expression */
            if (trimmed[0] == wxT('{') && trimmed[1] == wxT('?') && flt.Matches(line)) {
                size_t start, len;
                flt.GetMatch(&start, &len, 0);
                wxASSERT(start + len <= line.length());
                wxASSERT(line[start + 1] == wxT('?'));
                wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
                if (expr[0] == wxT(':')) {
                    /* there is an "end label" */
                    unsigned idx;
                    for (idx = 1; idx < expr.length() && expr[idx] > wxT(' '); idx++)
                        /* nothing, skip the label name */;
                    endlabel = expr.Left(idx);
                    expr = expr.Mid(idx);
                }
                rpn.Set(expr.utf8_str());
                RPN_ERROR err = rpn.Parse();
                if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                    line = wxEmptyString;
                } else {
                    line.replace(start, len, wxT(""));
                    endlabel = wxEmptyString;
                }
            }
            for (int matchindex = 0; flt.Matches(line); matchindex++) {
                size_t start, len;
                flt.GetMatch(&start, &len, 0);
                wxASSERT(len > 0 && start + len <= line.length());
                wxString expr = line.Mid(start + 1, len - 2);
                rpn.Set(expr.utf8_str());
                RPN_ERROR err = rpn.Parse();
                if (err == RPN_OK) {
                    RPNvalue val = rpn.Value();
                    if (val.Alpha()) {
                        expr = wxString::FromUTF8(val.Text());
                    } else {
                        expr = wxString::Format(wxT("%.4f"), val.Double());
                        StripTrailingZeros(&expr);
                    }
                } else if (err == RPN_EMPTY) {
                    /* this means that the expression ended with an assignment */
                    expr = wxEmptyString;
                } else {
                    wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression \"%s\""),
                                                    rpn_errors[err], tidx + 1, expr.c_str());
                    wxMessageBox(msg);
                    expr = wxEmptyString;
                    errorcount++;
                }
                str_replace(line, start, len, expr);
            }
            line.Trim();
            if (line.length() > 0)
                model.Add(line);
            tidx++;
        }
        if (endlabel.length() > 0) {
            wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
            errorcount++;
        }
    }

    /* copy the trailer, a simple loop that does not consider expressions */
    for (tidx = pins_end; tidx < templat.Count(); tidx++) {
        wxString line = templat[tidx];
        line.Trim();
        if (line.length() > 0)
            model.Add(line);
    }

    if (errorcount>0)
        return false;   /* message already given */

    /* check whether to create the path */
    wxFileName fname(vrmlpath);
    if (!wxDirExists(fname.GetPath()))
        wxFileName::Mkdir(fname.GetPath(), 0777, wxPATH_MKDIR_FULL);

    /* save the output */
    if (wxFileExists(vrmlpath))
        wxRemoveFile(vrmlpath);
    if (!file.Create(vrmlpath)) {
        wxMessageBox(wxString::Format(wxT("Failed to create file \"%s\""), vrmlpath.c_str()));
        return false;
    }
    for (unsigned tidx = 0; tidx < model.Count(); tidx++)
        file.AddLine(model[tidx]);
    file.Write();
    file.Close();

    return true;
}

/* For symbols */
bool SymbolFromTemplate(wxArrayString* symbol, const wxArrayString& templat,
                                            RPNexpression& rpn, const PinInfo* pininfo, int pincount)
{
    int errorcount = 0;

    wxASSERT(symbol != NULL);
    symbol->Clear();

    /* update variables on the count of pins per section (when the pins are
         re-assigned to other sections, the counts may no longer be correct) */
    wxASSERT(pininfo != NULL);
    int pcnt[PinInfo::SectionCount];
    for (int s = 0; s < PinInfo::SectionCount; s++) {
        int count = 0;
        for (int p = 0; p < pincount; p++)
            if (pininfo[p].section == s && pininfo[p].type != PinInfo::NC)
                count++;
        char varname[20];
        sprintf(varname, "PT:%d", s);
        rpn.SetVariable(RPNvariable(varname, count));
        pcnt[s] = count;
    }

    /* the regular expression matches "{...}" where "..." is anything; this must
       be a non-greedy match, because multiple expressions between braces can
       exist on a single line */
    wxRegEx flt(wxT("\\{([^\\}]+?)\\}"), wxRE_ADVANCED);
    wxASSERT(flt.IsValid());

    wxString endlabel = wxEmptyString;

    /* copy the shape */
    unsigned tidx = 0;
    unsigned drawstart = 0;
    unsigned drawend = 0;
    int pinidx = 0;
    while (tidx < templat.Count()) {
        wxString line = templat[tidx];
        line.Trim();
        /* handle skipping conditional blocks */
        if (endlabel.length() > 0 || line[0] == wxT(':')) {
            if (endlabel.length() > 0 && line.Cmp(endlabel) == 0)
                endlabel = wxEmptyString;   /* label matched, stop skipping */
            tidx++;
            continue;
        }
        /* handle conditional expression */
        if (line[0] == wxT('{') && line[1] == wxT('?')) {
            flt.Matches(line);
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(start == 0 && len > start && len <= line.length());
            wxASSERT(line[start + 1] == wxT('?'));
            wxString expr = line.Mid(start + 2, len - 3);   /* strip off {? and } */
            if (expr[0] == wxT(':')) {
                /* there is an "end label" */
                for (start = 1; start < expr.length() && expr[start] > wxT(' '); start++)
                    /* nothing, skip the label name */;
                endlabel = expr.Left(start);
                expr = expr.Mid(start);
            }
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK && rpn.Value().Double() < EPSILON) {
                line = wxEmptyString;
            } else {
                line.replace(0, len, wxT(""));
                endlabel = wxEmptyString;
            }
        }
        /* check for the start of a drawing section or whether we are inside a
             drawing section (because the pins need to be repeated) */
        if (line.Cmp(wxT("DRAW")) == 0) {
            drawstart = tidx;
            wxASSERT(pininfo != NULL);
            wxASSERT(pincount > 0 && pinidx < pincount);
            rpn.SetVariable(RPNvariable("PS", pininfo[pinidx].section));    /* side/section */
            rpn.SetVariable(RPNvariable("PTS", pcnt[pininfo[pinidx].section]));
            int pinseq = 0;
            for (int i = 0; i < pinidx; i++)
                if (pininfo[i].section == pininfo[pinidx].section && pininfo[i].type != PinInfo::NC)
                    pinseq++;
            rpn.SetVariable(RPNvariable("PNS", pinseq + 1));    /* sequence number in the section */
            rpn.SetVariable(RPNvariable("PLB", pininfo[pinidx].name.utf8_str()));
            rpn.SetVariable(RPNvariable("PN", pininfo[pinidx].number.utf8_str()));
            static const char *type[] = { "I", "O", "B", "T", "P", "C", "E", "N", "U", "W", "w" };
            rpn.SetVariable(RPNvariable("PTY", type[pininfo[pinidx].type]));
            static const char *shape[] = { "", "I", "C", "CI", "" };
            rpn.SetVariable(RPNvariable("PSH", shape[pininfo[pinidx].shape]));
            if (pinidx > 0) {   /* output the line "DRAW" only once */
                tidx++;
                continue;
            }
        } else if (line.Cmp(wxT("ENDDRAW")) == 0) {
            drawend = tidx;
            pinidx++;
            /* check whether we have handled all pins; if not, repeat */
            if (pinidx < pincount) {
                tidx = drawstart;   /* drop back to read "DRAW" on the next iteration, which
                                                         will adjust the variables */
                continue;
            }
        } else if (tidx >= drawstart && tidx < drawend && pinidx > 0) {
            /* on the second, third, ... passes through the DRAW section, ignore all
                 commands except X and calculations */
            if (line.Length() == 0 || (line.Left(2).Cmp(wxT("X ")) != 0 && line[0] != wxT('{'))) {
                tidx++;
                continue;
            }
        }
        for (int matchindex = 0; flt.Matches(line); matchindex++) {
            size_t start, len;
            flt.GetMatch(&start, &len, 0);
            wxASSERT(len > 0 && start + len <= line.length());
            wxString expr = line.Mid(start + 1, len - 2);
            rpn.Set(expr.utf8_str());
            RPN_ERROR err = rpn.Parse();
            if (err == RPN_OK) {
                RPNvalue val = rpn.Value();
                if (val.Alpha()) {
                    expr = wxString::FromUTF8(val.Text());
                } else {
                    expr = wxString::Format(wxT("%.4f"), val.Double());
                    StripTrailingZeros(&expr);
                }
            } else if (err == RPN_EMPTY) {
                /* this means that the expression ended with an assignment */
                expr = wxEmptyString;
            } else {
                wxString msg = wxString::Format(wxT("RPN expression error \"%s\" on line %u, expression \"%s\""),
                                                rpn_errors[err], tidx + 1, expr.c_str());
                wxMessageBox(msg);
                expr = wxEmptyString;
                errorcount++;
            }
            str_replace(line, start, len, expr);
        }
        line.Trim();
        if (line.length() > 0)
            symbol->Add(line);
        tidx++;
    }
    if (endlabel.length() > 0) {
        wxMessageBox(wxString::Format(wxT("RPN expression error: no match for label \"%s\""), endlabel.c_str()));
        errorcount++;
    }

    return errorcount == 0;
}

/* For symbols and footprints; also checkes whether the template exists */
wxString GetTemplateName(const wxArrayString& module)
{
    bool symbolmode = false;
    wxString name = wxEmptyString;
    for (unsigned idx = 0; name.length() == 0 && idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("AR")) == 0 || keyword.Cmp(wxT("#template")) == 0) {
            name = line;
        } else if (keyword.CmpNoCase(wxT("F4")) == 0) {
            name = GetToken(&line);
            symbolmode = true;
        }
    }

    /* strip off parameters */
    if (name.Find(wxT('[')) > 0) {
        //??? wxString parameters = name.AfterFirst(wxT('[')).BeforeLast(wxT(']'));
        name = name.BeforeFirst(wxT('['));
    }

    wxString ext = symbolmode ? wxT(".st") : wxT(".mt");
    wxString path = theApp->GetTemplatePath() + wxT(DIRSEP_STR) + name + ext;
    if (!wxFileExists(path))
        return wxEmptyString;

    return name;
}

/* For symbols and footprints */
wxString GetDescription(const wxArrayString& module, bool symbolmode)
{
    /* although it is possible to detect whether the "module" contains a footprint
       or a symbol, it is probably best not to try */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (!symbolmode && keyword.CmpNoCase(wxT("Cd")) == 0) {
            return line;
        } else if (!symbolmode && keyword.Cmp(wxT("(descr")) == 0) {
            line = line.Left(line.length() - 1);    /* strip trailing quote */
            return GetToken(&line); /* to strip the double quotes (if present) */
        } else if (symbolmode && keyword.CmpNoCase(wxT("D")) == 0) {
            return line;
        }
    }
    return wxEmptyString;
}

/* For symbols and footprints */
bool SetDescription(wxArrayString& module, const wxString& description, bool symbolmode)
{
    /* although it is possible to detect whether the "module" contains a footprint
         or a symbol, it is probably best not to try */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (!symbolmode && keyword.CmpNoCase(wxT("Cd")) == 0) {
            if (description.length() == 0)
                module.RemoveAt(idx);
            else
                module[idx] = wxT("Cd ") + description;
            return true;
        } else if (!symbolmode && keyword.Cmp(wxT("(descr")) == 0) {
            module[idx] = wxT("(descr \"") + description + wxT("\")");
            return true;
        } else if (symbolmode && keyword.CmpNoCase(wxT("D")) == 0) {
            if (description.length() == 0)
                module.RemoveAt(idx);
            else
                module[idx] = wxT("D ") + description;
            return true;
        }
    }
    /* if a component did not already have a description, one should be inserted */
    if (symbolmode) {
        wxString name = wxEmptyString;
        for (unsigned idx = 0; idx < module.Count(); idx++) {
            wxString line = module[idx];
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("$CMP")) == 0) {
                module.Insert(wxT("D ") + description, idx + 1);
                return true;
            } else if (keyword.CmpNoCase(wxT("DEF")) == 0) {
                name = GetToken(&line);
            }
        }
        /* if arrived here, the symbol does not have any documentation or keywords
             yet, so create the complete section */
        wxASSERT(name.length() > 0);    /* if the name is not set, this is not a valid symbol record */
        module.Add(wxT("#"));
        module.Add(wxT("$CMP ") + name);
        module.Add(wxT("D ") + description);
        module.Add(wxT("$ENDCMP"));
    } else {
        for (unsigned idx = 0; idx < module.Count(); idx++) {
            wxString line = module[idx];
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("$MODULE")) == 0) {
                module.Insert(wxT("Cd ") + description, idx + 1);
                return true;
            } else if (keyword.Cmp(wxT("(module")) == 0) {
                module.Insert(wxT("(descr \"") + description + wxT("\")"), idx + 1);
                return true;
            }
        }
    }
    return false;
}

/* For symbols and footprints */
wxString GetKeywords(const wxArrayString& module, bool symbolmode)
{
    /* although it is possible to detect whether the "module" contains a footprint
       or a symbol, it is probably best not to try */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (!symbolmode && keyword.CmpNoCase(wxT("Kw")) == 0) {
            return line;
        } else if (!symbolmode && keyword.Cmp(wxT("(tags")) == 0) {
            line = line.Left(line.length() - 1);    /* strip trailing quote */
            return GetToken(&line); /* to strip the double quotes (if present) */
        } else if (symbolmode && keyword.CmpNoCase(wxT("K")) == 0) {
            return line;
        }
    }
    return wxEmptyString;
}

/* For symbols and footprints */
bool SetKeywords(wxArrayString& module, const wxString& keywords, bool symbolmode)
{
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (!symbolmode && keyword.CmpNoCase(wxT("Kw")) == 0) {
            module[idx] = wxT("Kw ") + keywords;
            return true;
        } else if (!symbolmode && keyword.Cmp(wxT("(tags")) == 0) {
            module[idx] = wxT("(tags \"") + keywords + wxT("\")");
            return true;
        } else if (symbolmode && keyword.CmpNoCase(wxT("K")) == 0) {
            if (keywords.length() == 0)
                module.RemoveAt(idx);
            else
                module[idx] = wxT("K ") + keywords;
            return true;
        }
    }
    /* if a symbol did not already have aliases, they should be inserted */
    if (keywords.length() == 0)
        return true;    /* nothing to insert */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (!symbolmode && keyword.CmpNoCase(wxT("$MODULE")) == 0) {
            module.Insert(wxT("Kw ") + keywords, idx + 1);
            return true;
        } else if (!symbolmode && keyword.Cmp(wxT("(module")) == 0) {
            module.Insert(wxT("(tags \"") + keywords + wxT("\")"), idx + 1);
            return true;
        } else if (symbolmode && keyword.CmpNoCase(wxT("$ENDCMP")) == 0) {
            module.Insert(wxT("K ") + keywords, idx);
            return true;
        }
    }
    return false;
}

/* For symbols */
wxString GetPrefix(const wxArrayString& symbol)
{
    for (unsigned idx = 0; idx < symbol.Count(); idx++) {
        wxString line = symbol[idx];
        wxString keyword = GetToken(&line);
        if (keyword.Cmp(wxT("DEF")) == 0) {
            GetToken(&line);
            keyword = GetToken(&line);
            return keyword;
        }
    }
    return wxEmptyString;
}

/* For symbols */
wxString GetAliases(const wxArrayString& symbol)
{
    for (unsigned idx = 0; idx < symbol.Count(); idx++) {
        wxString line = symbol[idx];
        wxString keyword = GetToken(&line);
        if (keyword.Cmp(wxT("ALIAS")) == 0)
            return line;
    }
    return wxEmptyString;
}

/* For symbols */
bool SetAliases(wxArrayString& module, const wxString& aliases)
{
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("ALIAS")) == 0) {
            if (aliases.length() == 0)
                module.RemoveAt(idx);
            else
                module[idx] = wxT("ALIAS ") + aliases;
            return true;
        }
    }
    /* if a symbol did not already have aliases, they should be inserted */
    if (aliases.length() == 0)
        return true;    /* nothing to insert */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString keyword = GetToken(&line);
        if (keyword.CmpNoCase(wxT("DEF")) == 0) {
            module.Insert(wxT("ALIAS ") + aliases, idx + 1);
            return true;
        }
    }
    return false;
}

/* For symbols */
wxString GetFootprints(const wxArrayString& module)
{
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        if (line.CmpNoCase(wxT("$FPLIST")) == 0) {
            wxString total = wxEmptyString;
            while (++idx < module.Count()) {
                wxString line = module[idx];
                if (line.CmpNoCase(wxT("$ENDFPLIST")) == 0)
                    break;
                if (total.length() > 0)
                    total += wxT(" ");
                total += line;
            }
            return total;
        }
    }
    return wxEmptyString;
}

/* For symbols */
bool SetFootprints(wxArrayString& module, const wxString& footprints)
{
    unsigned pos = 0;
    /* find the section, erase it completely */
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        if (line.CmpNoCase(wxT("$FPLIST")) == 0) {
            pos = idx;  /* keep the insertion position */
            do {
                line = module[pos];
                module.RemoveAt(pos);
            } while (pos < module.Count() && line.CmpNoCase(wxT("$ENDFPLIST")) != 0);
        }
    }
    if (footprints.length() == 0)
        return true;    /* no new footprints to insert */

    /* no footprint filter was found, insert it near the top */
    if (pos == 0) {
        for (unsigned idx = 0; pos == 0 && idx < module.Count(); idx++) {
            wxString line = module[idx];
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("DEF")) == 0)
                pos = idx + 1;
        }
    }

    /* create a new section */
    wxString list = footprints;
    list.Trim();
    list.Trim(false);
    if (list.length() > 0) {
        module.Insert(wxT("$FPLIST"), pos++);
        while (list.length() > 0) {
            wxString pattern = GetToken(&list);
            module.Insert(pattern, pos++);
        }
        module.Insert(wxT("$ENDFPLIST"), pos);
    }

    return true;
}

/** GetPinNames() retrieves the names and numbers of the pins in an array.
 *  \param module       the symbol data structure
 *  \param info         if not NULL, the extracted information is stored here
 *  \param count        if not NULL, the total count of pins is stored here
 *
 *  \note "info" must be allocated with sufficient size; to query how many
 *              blocks to allocate, set "info" to NULL and "count" to a variable
 *              (non-NULL).
 *
 * For symbols.
 */
bool GetPinNames(const wxArrayString& module, PinInfo* info, int* count)
{
    int pinidx = 0;
    bool indraw = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        if (indraw) {
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("ENDDRAW")) == 0) {
                indraw = false;
                break;
            } if (keyword.CmpNoCase(wxT("X")) == 0) {
                wxString name = GetToken(&line);
                wxString number = GetToken(&line);
                double x = GetTokenLong(&line) * 0.0254;
                double y = GetTokenLong(&line) * 0.0254;
                GetToken(&line);    /* ignore length */
                wxString orient = GetToken(&line);
                GetToken(&line);    /* ignore text and pin number size */
                GetToken(&line);
                long part = GetTokenLong(&line);
                if (GetTokenLong(&line) > 1)
                    continue;       /* ignore De Morgan converted shape */
                wxString type = GetToken(&line);
                wxString shape = GetToken(&line);
                /* add the name the the description to the output array */
                if (info != NULL) {
                    info[pinidx].seq = pinidx;
                    info[pinidx].number = number;
                    info[pinidx].name = name;
                    info[pinidx].part = part;
                    info[pinidx].x = x;
                    info[pinidx].y = y;
                    switch ((int)orient[0]) {
                    case 'R':
                        info[pinidx].section = PinInfo::Left;
                        break;
                    case 'L':
                        info[pinidx].section = PinInfo::Right;
                        break;
                    case 'D':
                        info[pinidx].section = PinInfo::Top;
                        break;
                    case 'U':
                        info[pinidx].section = PinInfo::Bottom;
                        break;
                    }
                    switch ((int)type[0]) {
                    case 'I':
                        info[pinidx].type = PinInfo::Input;
                        break;
                    case 'O':
                        info[pinidx].type = PinInfo::Output;
                        break;
                    case 'B':
                        info[pinidx].type = PinInfo::BiDir;
                        break;
                    case 'T':
                        info[pinidx].type = PinInfo::Tristate;
                        break;
                    case 'P':
                        info[pinidx].type = PinInfo::Passive;
                        break;
                    case 'C':
                        info[pinidx].type = PinInfo::OpenCol;
                        break;
                    case 'E':
                        info[pinidx].type = PinInfo::OpenEmit;
                        break;
                    case 'N':
                        info[pinidx].type = PinInfo::NC;
                        break;
                    case 'U':
                        info[pinidx].type = PinInfo::Unspecified;
                        break;
                    case 'W':
                        info[pinidx].type = PinInfo::PowerIn;
                        break;
                    case 'w':
                        info[pinidx].type = PinInfo::PowerOut;
                        break;
                    }
                    info[pinidx].shape = PinInfo::Other;
                    if (shape.length() == 0)
                        info[pinidx].shape = PinInfo::Line;
                    else if (shape.CmpNoCase(wxT("I")) == 0)
                        info[pinidx].shape = PinInfo::Inverted;
                    else if (shape.CmpNoCase(wxT("C")) == 0)
                        info[pinidx].shape = PinInfo::Clock;
                    else if (shape.CmpNoCase(wxT("CI")) == 0)
                        info[pinidx].shape = PinInfo::InvClock;
                }
                pinidx += 1;
            }
        } else if (line.CmpNoCase(wxT("DRAW")) == 0) {
            indraw = true;
        }
    }
    if (count != NULL)
        *count = pinidx;

    if (info != NULL) {
        /* sort the array on pin numbers (simple insertion sort) */
        for (int i = 1; i < pinidx; i++) {
            PinInfo t = info[i];
            /* check whether the pin number of "t" is a number (it usually is) */
            long tn;
            if (!t.number.ToLong(&tn))
                tn = LONG_MIN;
            int j = i;
            while (j > 0) {
                /* check whether the value compared to is a number (it usually is) */
                int result;
                long vn;
                if (tn != LONG_MIN && info[j - 1].number.ToLong(&vn))
                    result = tn - vn;
                else
                    result = t.number.CmpNoCase(info[j - 1].number);
                if (result >= 0)
                    break;
                info[j] = info[j - 1];
                j--;
            }
          info[j] = t;
        }
        /* if all pins are of part 1 (or general), there is only a single part */
        bool pin1found = false;
        bool pin2found = false;
        for (int i = 0; i < pinidx; i++) {
            if (info[i].part == 1)
                pin1found = true;
            if (info[i].part >= 2)
                pin2found = true;
        }
        if (pin1found && !pin2found) {
            for (int i = 0; i < pinidx; i++)
                info[i].part = 0;
        }
    }

    return true;
}

/** SetPinNames() can only adjust existing pins; it cannot add new pins, or
 *  remove pins.
 *  For symbols.
 */
bool SetPinNames(wxArrayString& module, const PinInfo* info, int count)
{
    static const char type[] = { 'I', 'O', 'B', 'T', 'P', 'C', 'E', 'N', 'U', 'W', 'w' };
    static const char orientation[] = { 'R', 'L', 'D', 'U' };
    static const wxString shape[] = { wxT(""), wxT("I"), wxT("C"), wxT("CI"), wxEmptyString };

    wxASSERT(info != NULL);
    int sequence = 0;
    bool indraw = false;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        if (indraw) {
            wxString keyword = GetToken(&line);
            if (keyword.CmpNoCase(wxT("ENDDRAW")) == 0) {
                indraw = false;
                break;
            } if (keyword.CmpNoCase(wxT("X")) == 0) {
                /* first look through the pin definition whether this pin must be handled */
                GetToken(&line);                    /* ignore the pin name */
                GetToken(&line);                    /* ignore the pin number */
                long xpos = GetTokenLong(&line);    /* save position and length */
                long ypos = GetTokenLong(&line);
                long len = GetTokenLong(&line);
                GetToken(&line);                    /* ignore orientation */
                long sznum = GetTokenLong(&line);   /* save pin label sizes */
                long szname = GetTokenLong(&line);
                long part = GetTokenLong(&line);    /* save part */
                long dmg = GetTokenLong(&line);
                if (dmg > 1)
                    continue;                       /* ignore De Morgan converted shape */
                GetToken(&line);                    /* ignore type */
                wxString curshape = GetToken(&line);
                /* find the index for the sequence number */
                int pinidx;
                for (pinidx = 0; pinidx < count && info[pinidx].seq != sequence; pinidx++)
                    /* nothing */;
                wxASSERT(pinidx < count);           /* sequence number should be found */
                sequence++;                         /* next time, search for the sequentially next pin */
                /* create the new line */
                wxString name = info[pinidx].name;
                if (name.length() == 0)
                    name = wxT("~");
                if (info[pinidx].shape != PinInfo::Other)
                    curshape = shape[info[pinidx].shape];
                else
                    curshape = wxT("");
                if (info[pinidx].type == PinInfo::NC)
                    curshape = wxT("N") + curshape;
                int section = info[pinidx].section;
                if (section >= PinInfo::LeftCustom)
                    section -= PinInfo::LeftCustom;
                line = wxString::Format(wxT("X %s %s %ld %ld %ld %c %ld %ld %ld %ld %c %s"),
                                        name.c_str(), info[pinidx].number.c_str(),
                                        xpos, ypos, len, orientation[section],
                                        sznum, szname, part, dmg,
                                        type[info[pinidx].type], curshape.c_str());
                line.Trim();
                module[idx] = line;
            }
        } else if (line.CmpNoCase(wxT("DRAW")) == 0) {
            indraw = true;
        }
    }
    return true;
}

/** ReAssignPins() changes the standard sections of pins to custom sections, if
 *  these pins fall into the range for the custom section.
 */
void ReAssignPins(PinInfo* info, int count, const BodyInfo& body, const PinSection custsections[], size_t maxsections)
{
    wxASSERT(info);
    for (size_t s = 0; s < maxsections; s++) {
        if (!custsections[s].IsValid())
            continue;
        for (int p = 0; p < count; p++) {
            if ((custsections[s].Section() == PinInfo::LeftCustom && info[p].section != PinInfo::Left)
                || (custsections[s].Section() == PinInfo::RightCustom && info[p].section != PinInfo::Right))
                continue;
            double rel = (double)info[p].y / body.BodyLength + 0.5;
            double crit = custsections[s].Criterion();
            wxASSERT(crit < -EPSILON || crit > EPSILON);
            if ((crit > EPSILON && rel <= crit)         /* the "custom" section is in the bottom half & the pin is in the bottom half */
                || (crit < EPSILON && rel >= 1 - crit)) /* the "custom" section is in the top half & the pin is in the top half */
                info[p].section = custsections[s].Section();
        }
    }
}

/* Retrieves the extents of the drawings, for footprints and symbols */
bool GetBodySize(const wxArrayString& module, BodyInfo* info, bool symbolmode, bool unit_mm)
{
    wxASSERT(info != 0);
    info->Clear();

    double x1 = 0, y1 = 0, x2 = 0, y2 = 0;
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString token = GetToken(&line);
        if (!symbolmode && token.CmpNoCase(wxT("DS")) == 0) {
            double x = GetTokenDim(&line, unit_mm);
            double y = GetTokenDim(&line, unit_mm);
            if (x < x1)
                x1 = x;
            else if (x > x2)
                x2 = x;
            if (y < y1)
                y1 = y;
            else if (y > y2)
                y2 = y;
            x = GetTokenDim(&line, unit_mm);
            y = GetTokenDim(&line, unit_mm);
            if (x < x1)
                x1 = x;
            else if (x > x2)
                x2 = x;
            if (y < y1)
                y1 = y;
            else if (y > y2)
                y2 = y;
        } else if (!symbolmode && (token.CmpNoCase(wxT("DC")) == 0 || token.CmpNoCase(wxT("DA")) == 0)) {
            double xc = GetTokenDim(&line, unit_mm);
            double yc = GetTokenDim(&line, unit_mm);
            double xp = GetTokenDim(&line, unit_mm);
            double yp = GetTokenDim(&line, unit_mm);
            double radius = sqrt((xp - xc) * (xp - xc) + (yp - yc) * (yp - yc));
            if (xc - radius < x1)
                x1 = xc - radius;
            if (xc + radius > x2)
                x2 = xc + radius;
            if (yc - radius < y1)
                y1 = yc - radius;
            if (yc + radius > y2)
                y2 = yc + radius;
        } else if (token.Cmp(wxT("(fp_line")) == 0) {
            wxASSERT(!symbolmode);
            double x=0, y=0;
            wxString section = GetSection(line, wxT("start"));
            if (section.length() > 0) {
                x = GetTokenDim(&section, true);
                y = GetTokenDim(&section, true);
                if (x < x1)
                    x1 = x;
                else if (x > x2)
                    x2 = x;
                if (y < y1)
                    y1 = y;
                else if (y > y2)
                    y2 = y;
            }
            section = GetSection(line, wxT("end"));
            if (section.length() > 0) {
                x = GetTokenDim(&section, true);
                y = GetTokenDim(&section, true);
                if (x < x1)
                    x1 = x;
                else if (x > x2)
                    x2 = x;
                if (y < y1)
                    y1 = y;
                else if (y > y2)
                    y2 = y;
            }
        } else if (token.Cmp(wxT("(fp_circle")) == 0) {
            wxASSERT(!symbolmode);
            double xc=0, yc=0, xp=0, yp=0;
            wxString section = GetSection(line, wxT("center"));
            if (section.length() > 0) {
                xc = GetTokenDim(&section, true);
                yc = GetTokenDim(&section, true);
            }
            section = GetSection(line, wxT("end"));
            if (section.length() > 0) {
                xp = GetTokenDim(&section, true);
                yp = GetTokenDim(&section, true);
            }
            double radius = sqrt((xp - xc) * (xp - xc) + (yp - yc) * (yp - yc));
            if (xc - radius < x1)
                x1 = xc - radius;
            if (xc + radius > x2)
                x2 = xc + radius;
            if (yc - radius < y1)
                y1 = yc - radius;
            if (yc + radius > y2)
                y2 = yc + radius;
        } else if (token.Cmp(wxT("(fp_arc")) == 0) {
            wxASSERT(!symbolmode);
            double xc=0, yc=0, xp=0, yp=0;
            wxString section = GetSection(line, wxT("start"));
            if (section.length() > 0) {
                xc = GetTokenDim(&section, true);
                yc = GetTokenDim(&section, true);
            }
            section = GetSection(line, wxT("end"));
            if (section.length() > 0) {
                xp = GetTokenDim(&section, true);
                yp = GetTokenDim(&section, true);
            }
            double radius = sqrt((xp - xc) * (xp - xc) + (yp - yc) * (yp - yc));
            if (xc - radius < x1)
                x1 = xc - radius;
            if (xc + radius > x2)
                x2 = xc + radius;
            if (yc - radius < y1)
                y1 = yc - radius;
            if (yc + radius > y2)
                y2 = yc + radius;
        } else if (symbolmode && token.Cmp(wxT("C")) == 0) {
            double xc = GetTokenLong(&line) * 0.0254;
            double yc = GetTokenLong(&line) * 0.0254;
            double radius = GetTokenLong(&line) * 0.0254;
            if (xc - radius < x1)
                x1 = xc - radius;
            if (xc + radius > x2)
                x2 = xc + radius;
            if (yc - radius < y1)
                y1 = yc - radius;
            if (yc + radius > y2)
                y2 = yc + radius;
        } else if (symbolmode && token.Cmp(wxT("S")) == 0) {
            double px1 = GetTokenLong(&line) * 0.0254;
            double py1 = GetTokenLong(&line) * 0.0254;
            double px2 = GetTokenLong(&line) * 0.0254;
            double py2 = GetTokenLong(&line) * 0.0254;
            if (px1 > px2) {
                double t = px1;
                px1 = px2;
                px2 = t;
            }
            if (py1 > py2) {
                double t = py1;
                py1 = py2;
                py2 = t;
            }
            if (px1 < x1)
                x1 = px1;
            if (px2 > x2)
                x2 = px2;
            if (py1 < y1)
                y1 = py1;
            if (py2 > y2)
                y2 = py2;
        } else if (symbolmode && token.Cmp(wxT("P")) == 0) {
            long count = GetTokenLong(&line);
            GetToken(&line);    /* ignore part */
            GetToken(&line);    /* ignore dmg */
            GetToken(&line);    /* ignore pen */
            while (count-- > 0) {
                double x = GetTokenLong(&line) * 0.0254;
                double y = GetTokenLong(&line) * 0.0254;
                if (x < x1)
                    x1 = x;
                else if (x > x2)
                    x2 = x;
                if (y < y1)
                    y1 = y;
                if (y > y2)
                    y2 = y;
            }
        }
    }

    info->BodyLength = y2 - y1;
    info->BodyWidth = x2 - x1;
    return true;
}

/* Retrieves the sizes of the reference and text labels, as well as whether
   they are visible, for symbols and footprints */
bool GetTextLabelSize(const wxArrayString& module, LabelInfo* info, bool symbolmode, bool unit_mm)
{
    wxASSERT(info != 0);
    info->RefLabelSize = info->ValueLabelSize = 0;
    info->RefLabelVisible = info->ValueLabelVisible = false;

    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString token = GetToken(&line);
        if (!symbolmode && token.length() == 2 && token[0] == wxT('T') && isdigit(token[1])) {
            GetToken(&line);    /* ignore x position */
            GetToken(&line);    /* ignore y position */
            double xsize = GetTokenDim(&line, unit_mm);
            double ysize = GetTokenDim(&line, unit_mm);
            double size = (xsize + ysize) / 2;
            GetToken(&line);    /* ignore text rotation */
            GetToken(&line);    /* ignore pen width */
            GetToken(&line);    /* ignore mirror */
            wxString visible = GetToken(&line);
            if (token[1] == wxT('0')) {
                info->RefLabelSize = size;
                info->RefLabelVisible = (visible[0] == wxT('V'));
            } else if (token[1] == wxT('1')) {
                info->ValueLabelSize = size;
                info->ValueLabelVisible = (visible[0] == wxT('V'));
            }
        } else if (token.Cmp(wxT("(fp_text")) == 0) {
            wxASSERT(!symbolmode);
            double xsize = 0, ysize = 0;
            wxString type = GetToken(&line);
            bool ishidden = line.Find(wxT(" hide ")) > 0;
            wxString section = GetSection(line, wxT("effects"));
            if (section.length() > 0) {
                section = GetSection(section, wxT("font"));
                if (section.length() > 0) {
                    wxString subsection = GetSection(section, wxT("size"));
                    if (subsection.length() > 0) {
                        xsize = GetTokenDim(&subsection, true);
                        ysize = GetTokenDim(&subsection, true);
                    }
                }
            }
            double size = (xsize + ysize) / 2;
            if (type.Cmp(wxT("reference")) == 0) {
                info->RefLabelSize = size;
                info->RefLabelVisible = !ishidden;
            } else if (type.Cmp(wxT("value")) == 0) {
                info->ValueLabelSize = size;
                info->ValueLabelVisible = !ishidden;
            }
        } else if (symbolmode && token.length() == 2 && token[0] == wxT('F') && isdigit(token[1])) {
            GetToken(&line);    /* ignore text */
            GetToken(&line);    /* ignore x position */
            GetToken(&line);    /* ignore y position */
            double size = GetTokenLong(&line) * 0.0254;
            GetToken(&line);    /* ignore horz./vert. orientation */
            wxString visflag = GetToken(&line);
            bool ishidden = (visflag == wxT('I'));
            if (token[1] == wxT('0')) {
                info->RefLabelSize = size;
                info->RefLabelVisible = !ishidden;
            } else if (token[1] == wxT('1')) {
                info->ValueLabelSize = size;
                info->ValueLabelVisible = !ishidden;
            }
        }
    }

    return true;
}

/* Sets the sizes of the reference and text labels, as well as whether
   they are visible, for symbols and footprints
   Note that unit_mm is ignored when symbolmode is true */
void SetTextLabelSize(wxArrayString& module, const LabelInfo& info, bool symbolmode, bool unit_mm)
{
    for (unsigned idx = 0; idx < module.Count(); idx++) {
        wxString line = module[idx];
        wxString token = GetToken(&line);
        if (!symbolmode && token.length() == 2 && token[0] == wxT('T') && (token[1] == '0' || token[1] == '1')) {
            double xpos = GetTokenDim(&line, unit_mm);
            double ypos = GetTokenDim(&line, unit_mm);
            double xsize = GetTokenDim(&line, unit_mm);
            double ysize = GetTokenDim(&line, unit_mm);
            long rot = GetTokenLong(&line);
            double pen = GetTokenDim(&line, unit_mm);
            wxString mirror = GetToken(&line);
            GetToken(&line);    /* ignore visible flag */
            /* get new size; calculate new pen based on new size (keeping the same weight) */
            double size = (token[1] == '0') ? info.RefLabelSize : info.ValueLabelSize;
            bool visible = (token[1] == '0') ? info.RefLabelVisible : info.ValueLabelVisible;
            double weight = pen / ((xsize + ysize) / 2);
            pen = size * weight;
            module[idx] = wxString::Format(wxT("%s %.4f %.4f %.2f %.2f %ld %.2f %s %s %s"),
                                           token.c_str(), MM(xpos), MM(ypos), MM(size), MM(size),
                                           rot, MM(pen), mirror.c_str(), visible ? wxT("V") : wxT("I"),
                                           line.c_str());
        } else if (token.Cmp(wxT("(fp_text")) == 0) {
            /* verify that it is only reference or value (not user) */
            wxString type = GetToken(&line);
            if (type.Cmp(wxT("reference")) == 0 || type.Cmp(wxT("value")) == 0) {
                double xsize = 0, ysize = 0, pen = 0;
                wxString text = GetToken(&line);
                wxString at = GetSection(line, wxT("at"));
                wxString layer = GetSection(line, wxT("layer"));
                wxString section = GetSection(line, wxT("effects"));
                if (section.length() > 0) {
                    section = GetSection(section, wxT("font"));
                    if (section.length() > 0) {
                        wxString subsection = GetSection(section, wxT("size"));
                        if (subsection.length() > 0) {
                            xsize = GetTokenDim(&subsection, true);
                            ysize = GetTokenDim(&subsection, true);
                        }
                        subsection = GetSection(section, wxT("thickness"));
                        if (subsection.length() > 0)
                            pen = GetTokenDim(&subsection, true);
                    }
                }
                double weight = pen / ((xsize + ysize) / 2);
                double size;
                bool visible;
                if (type.Cmp(wxT("reference")) == 0) {
                    size = info.RefLabelSize;
                    visible = info.RefLabelVisible;
                } else {
                    wxASSERT(type.Cmp(wxT("value")) == 0);
                    size = info.ValueLabelSize;
                    visible = info.ValueLabelVisible;
                }
                line = wxT("(fp_text ") + type + wxT(" \"") + text + wxT("\"");
                if (at.length() > 0)
                    line += wxT(" (at ") + at + wxT(")");
                if (layer.length() > 0)
                    line += wxT(" (layer ") + layer + wxT(")");
                if (!visible)
                    line += wxT(" hide");
                line += wxString::Format(wxT(" (effects (font (size %.4f %.4f) (thickness %.4f))))"),
                                         size, size, size * weight);
                module[idx] = line;
            }
        } else if (symbolmode && token.length() == 2 && token[0] == wxT('F') && (token[1] == '0' || token[1] == '1')) {
            wxString text = GetToken(&line);
            wxString xpos = GetToken(&line);
            wxString ypos = GetToken(&line);
            GetToken(&line);    /* skip size */
            wxString orient = GetToken(&line);
            GetToken(&line);    /* skip visible/hidden */
            double size = (token[1] == '0') ? info.RefLabelSize : info.ValueLabelSize;
            bool visible = (token[1] == '0') ? info.RefLabelVisible : info.ValueLabelVisible;
            module[idx] = wxString::Format(wxT("%s \"%s\" %s %s %ld %s %c %s"),
                                           token.c_str(), text.c_str(), xpos.c_str(), ypos.c_str(),
                                           (long)(size / 0.0254 + 0.5), orient.c_str(),
                                           (visible ? 'V' : 'I'), line.c_str());
        }
    }
}

bool TranslatePadInfo(wxArrayString* module, FootprintInfo* info)
{
    wxASSERT(module != 0);
    wxASSERT(info != 0);
    info->Clear(info->Type);    /* clear fields (but set the type) */

    /* first create an index of the start positions of each pad (count the number
       of pads too); the index will speed up the operation of walking through the
       pads, as the pads are not necessarily stored in the order of pin numbers */
    struct PadDim {
        unsigned startidx;  /* index in the module where the pad definitions starts (or 0 for an invalid pad) */
        double x, y;
        double width, height;
        double orgwidth, orgheight; /* pad width/height may change if multiple overlapping pads are combined */
        long angle;
        char shape;
        double drillwidth, drillheight;     /* for round holes, only drillwidth is set */
        bool rotate;        /* set to true if the pad must be rotated by 90 degrees relative to the anchor pad */
    } *padlist = NULL;
    int padcount = 0;       /* total number of pads considered */
    int padvalid = 0;       /* valid, numbered pads */
    int padmech = 0;        /* mechanical pads (no name, no number) */
    bool DryRun = false;
    bool unit_mm = (info->Type >= VER_MM);
    do {
        DryRun = !DryRun;
        if (!DryRun) {
            padcount = (padvalid + 1) + padmech;    /* padvalid holds the top pad number, so +1 to include it */
            padlist = new PadDim[padcount];
            wxASSERT(padlist != NULL);
            for (int idx = 0; idx < padcount; idx++)
                padlist[idx].startidx = 0;
            padmech = 0;
        }
        double padx = 0, pady = 0, padwidth = 0, padheight = 0;
        double drillwidth = 0, drillheight = 0;
        char padshape = '\0';
        long padangle = 0;
        unsigned startidx = 0;
        long pinnr = PIN_UNKNOWN;
        for (unsigned idx = 0; idx < module->Count(); idx++) {
            wxString line = module->Item(idx);
            line.Trim(false);
            if (line[0] == wxT('$')) {
                if (line.CmpNoCase(wxT("$PAD")) == 0) {
                    startidx = idx;
                } else if (line.CmpNoCase(wxT("$EndPAD")) == 0) {
                    if (DryRun) {
                        if (pinnr > padvalid)
                            padvalid = pinnr;
                        else if (pinnr == PIN_ANOMYM)
                            padmech++;  /* this is redundant for overlapping anonymous pads that form an exposed pad, but it does not hurt either */
                    } else if (pinnr != PIN_UNKNOWN) {
                        wxASSERT((pinnr > 0 && pinnr <= padvalid) || pinnr == PIN_ANOMYM);
                        if (pinnr == PIN_ANOMYM) {
                            /* check whether this pad overlaps with the previous pad of the
                               same size, which is also an anonymous pad -> if so, these pads
                               will be merged, so the count of "mechanical" pads must not be
                               increased and the count of total pads (based on the dry run)
                               must be decreased */
                            int new_padmech = padmech + 1;  /* preset to non-merged pad */
                            if (padmech > 0) {
                                CoordSize pad, slice;
                                pinnr = padvalid + padmech;
                                if (Equal(padlist[pinnr].orgwidth, padwidth) && Equal(padlist[pinnr].orgheight, padheight)) {
                                    pad.SetMid(padlist[pinnr].x, padlist[pinnr].y, padlist[pinnr].width, padlist[pinnr].height);
                                    slice.SetMid(padx, pady, padwidth, padheight);
                                    if (pad.OverlapOrTouch(slice)) {
                                        padcount--;     /* this pad will be merged, total number of pads is 1 less than estimated in the dry run */
                                        new_padmech = padmech;  /* reset mechanical pad count */
                                    }
                                }
                            }
                            padmech = new_padmech;
                            pinnr = padvalid + padmech;
                        }
                        wxASSERT(pinnr > 0 && pinnr < padcount);
                        if (padlist[pinnr].startidx > 0) {
                            /* pad with this number already exists, see whether to merge with it */
                            if (Equal(padlist[pinnr].orgwidth, padwidth) && Equal(padlist[pinnr].orgheight, padheight)) {
                                CoordSize pad, slice;
                                pad.SetMid(padlist[pinnr].x, padlist[pinnr].y, padlist[pinnr].width, padlist[pinnr].height);
                                slice.SetMid(padx, pady, padwidth, padheight);
                                if (pad.OverlapOrTouch(slice)) {
                                    pad.Union(slice);
                                    padlist[pinnr].x = pad.GetMidX();
                                    padlist[pinnr].y = pad.GetMidY();
                                    padlist[pinnr].width = pad.GetWidth();
                                    padlist[pinnr].height = pad.GetHeight();
                                }
                            }
                        } else {
                            padlist[pinnr].startidx = startidx;
                            padlist[pinnr].x = padx;
                            padlist[pinnr].y = pady;
                            padlist[pinnr].width = padwidth;
                            padlist[pinnr].height = padheight;
                            padlist[pinnr].angle = padangle;
                            padlist[pinnr].shape = padshape;
                            padlist[pinnr].drillwidth = drillwidth;
                            padlist[pinnr].drillheight = drillheight;
                            padlist[pinnr].orgwidth = padwidth;
                            padlist[pinnr].orgheight = padheight;
                        }
                    }
                    padx = pady = padwidth = padheight = 0;
                    padangle = 0;
                    drillwidth = drillheight = 0;
                    padshape = '\0';
                    startidx = 0;
                    pinnr = PIN_UNKNOWN;
                }
                continue;
            } else if (line[0] == wxT('(') && line.Left(4).Cmp(wxT("(pad")) == 0) {
                GetToken(&line);    /* ignore "(pad" */
                wxString pin = GetToken(&line);
                pinnr = GetPadNumber(pin, &info->MtxWidth, &info->MtxHeight);
                if (pinnr <= 0 && pinnr != PIN_ANOMYM)
                    pinnr = PIN_UNKNOWN;
                if (DryRun) {
                    if (pinnr > padvalid)
                        padvalid = pinnr;
                    else if (pinnr == PIN_ANOMYM)
                        padmech++;
                } else if (pinnr != PIN_UNKNOWN) {
                    wxASSERT((pinnr >= 0 && pinnr <= padvalid) || pinnr == PIN_ANOMYM);
                    padlist[pinnr].startidx = idx;
                    if (pinnr == PIN_ANOMYM)
                        pinnr = padvalid + ++padmech;
                    wxASSERT(pinnr > 0 && pinnr < padcount);
                    GetToken(&line);    /* ignore smd/thru_hole/np_thru_hole type */
                    wxString shape = GetToken(&line);
                    wxString section = GetSection(line, wxT("at"));
                    if (section.length() > 0) {
                        padlist[pinnr].x = GetTokenDim(&section, true);
                        padlist[pinnr].y = GetTokenDim(&section, true);
                        if (section.length() > 0)
                            padlist[pinnr].angle = (long)GetTokenDouble(&section);
                        else
                            padlist[pinnr].angle = 0;
                    }
                    section = GetSection(line, wxT("size"));
                    if (section.length() > 0) {
                        padlist[pinnr].width = GetTokenDim(&section, true);
                        padlist[pinnr].height = GetTokenDim(&section, true);
                    }
                    section = GetSection(line, wxT("drill"));
                    if (section.length() > 0) {
                        if (section.Left(4).Cmp(wxT("oval"))==0) {
                            GetToken(&section);
                            padlist[pinnr].drillwidth = GetTokenDim(&section, true);
                            padlist[pinnr].drillheight = GetTokenDim(&section, true);
                        } else {
                            padlist[pinnr].drillwidth = GetTokenDim(&section, true);
                            padlist[pinnr].drillheight = 0;
                        }
                    }
                    if (shape.Cmp(wxT("circle")) == 0)
                        padlist[pinnr].shape = 'C';
                    else if (shape.Cmp(wxT("rect")) == 0)
                        padlist[pinnr].shape = 'R';
                    else if (shape.Cmp(wxT("oval")) == 0)
                        padlist[pinnr].shape = 'O';
                    else if (shape.Cmp(wxT("trapezoid")) == 0)
                        padlist[pinnr].shape = 'T';
                }
                pinnr = PIN_UNKNOWN;
                continue;
            }
            if (startidx == 0)
                continue;
            wxString token = GetToken(&line);
            if (token.CmpNoCase(wxT("Po")) == 0) {
                /* the pad position is relative to the module position, but in a
                   module library, the position is always presumed to be 0 (there may
                   be a non-zero position in the definition of a footprint module, but
                     its position is still presumed to be zero) */
                padx = GetTokenDim(&line, unit_mm);
                pady = GetTokenDim(&line, unit_mm);
            } else if (token.CmpNoCase(wxT("Sh")) == 0) {
                wxString pin = GetToken(&line);
                pinnr = GetPadNumber(pin, &info->MtxWidth, &info->MtxHeight);
                if (pinnr <= 0 && pinnr != PIN_ANOMYM)
                    pinnr = PIN_UNKNOWN;
                wxString shape = GetToken(&line);
                padshape = toupper(shape[0]);
                padwidth = GetTokenDim(&line, unit_mm);
                padheight = GetTokenDim(&line, unit_mm);
                GetToken(&line);    /* ignore x-delta */
                GetToken(&line);    /* ignore y-delta */
                padangle = GetTokenLong(&line);
            } else if (token.CmpNoCase(wxT("Dr")) == 0) {
                drillwidth = GetTokenDim(&line, unit_mm);
                GetToken(&line);            /* ignore drill offset */
                GetToken(&line);
                token = GetToken(&line);
                if (token == wxT('O')) {
                    drillwidth = GetTokenDim(&line, unit_mm);
                    drillheight = GetTokenDim(&line, unit_mm);
                }
            }
        } /* for (running over the pads in the module) */
    } while (DryRun && padvalid > 0);   /* if no numbered pad was found in the dry run, no need for a second run */

    /* walk through the data to find the pin pitch */
    double drillsize = -1;
    double pitchhor = -1, pitchver = -1;
    CoordPair padsize[2];
    padsize[0] = CoordPair(-1, -1);
    padsize[1] = CoordPair(-1, -1);
    bool padrightangle[2] = { false, false };
    int horpin_base = 0, verpin_base = 0;
    int horlevel = 0, verlevel = 0;
    int padsizecount[2] = { 0, 0 };
    char padshape = '\0';
    for (int pinnr = 1; pinnr < padcount; pinnr++) {
        if (padlist[pinnr].startidx == 0)
            continue;
        padlist[pinnr].rotate = false;  /* preset */
        if (!padsize[0].Equal(0,0)) {
            /* first pad is not yet set to "variable size" */
            if (padsize[0].Equal(-1,-1)) {
                /* first pad is not yet set, set dimensions */
                padsize[0].Set(padlist[pinnr].width, padlist[pinnr].height);
                padsizecount[0] += 1;
                padrightangle[0] = padlist[pinnr].angle == 90 || padlist[pinnr].angle == 270;
            } else if (padsize[0].Equal(padlist[pinnr].width, padlist[pinnr].height)) {
                /* this pad has equal size to the first pad */
                padsizecount[0] += 1;
            } else if (padsize[0].Equal(padlist[pinnr].height, padlist[pinnr].width)) {
                /* this pad has equal size to the first pad after rotation by 90 degrees */
                padsizecount[0] += 1;
                padlist[pinnr].rotate = true;
            } else {
                /* this pad does not match the first pad, check whether to
                     use the second pad */
                if (padsize[1].Equal(-1,-1)) {
                    /* second pad is not yet set, set dimensions */
                    padsize[1].Set(padlist[pinnr].width, padlist[pinnr].height);
                    padsizecount[1] += 1;
                    padrightangle[1] = padlist[pinnr].angle == 90 || padlist[pinnr].angle == 270;
                } else if (padsize[1].Equal(padlist[pinnr].width, padlist[pinnr].height)) {
                    /* this pad has equal size to the second pad */
                    padsizecount[1] += 1;
                } else if (padsize[1].Equal(padlist[pinnr].height, padlist[pinnr].width)) {
                    /* this pad has equal size to the second pad, after rotation by 90 degrees */
                    padsizecount[1] += 1;
                    padlist[pinnr].rotate = true;
                } else {
                    /* this pad mismatches the first and the second pads, set
                         both to "mismatch" */
                    padsize[0].Set(0, 0);
                    padsize[1].Set(0, 0);
                    padrightangle[0] = padrightangle[1] = false;
                }
            }
        }
        if (padlist[pinnr - 1].startidx > 0) {
            double dx = padlist[pinnr].x - padlist[pinnr - 1].x;
            double dy = padlist[pinnr].y - padlist[pinnr - 1].y;
            if (dx < 0)
                dx = -dx;
            if (dy < 0)
                dy = -dy;
            /* check horizontal pitch */
            if (horlevel < 2 && dx >= MIN_PITCH) {  /* deltas smaller than this are not seen as pitch */
                int newlevel = (dy < MIN_PITCH) ? 2 : 1;
                if (newlevel > horlevel) {
                    pitchhor = dx;
                    horlevel = newlevel;
                    horpin_base = pinnr;
                }
            }
            /* check vertical pitch */
            if (verlevel < 2 && dy >= MIN_PITCH) {
                int newlevel = (dx < MIN_PITCH) ? 2 : 1;
                if (newlevel > verlevel) {
                    pitchver = dy;
                    verlevel = newlevel;
                    verpin_base = pinnr;
                }
            }
        }
        if (padshape == '\0') {
            padshape = padlist[pinnr].shape;
        } else if (padshape != padlist[pinnr].shape) {
            if ((padshape == 'R' || padshape == 'S') && padlist[pinnr].shape == 'C'
                && padsize[0].Equal(padlist[pinnr].width,padlist[pinnr].height))
                padshape = 'S'; /* first pad is square with the same dimension as the round pad */
            else if (padlist[pinnr].shape != 'R' || pinnr < padvalid)
                padshape = 'v'; /* multiple pad shapes, but make an exception for the centre pad */
        }
        if (drillsize < -EPSILON && padlist[pinnr].drillwidth > EPSILON)
            drillsize = padlist[pinnr].drillwidth;
        else if (padlist[pinnr].drillwidth > EPSILON && (drillsize < padlist[pinnr].drillwidth - EPSILON || drillsize > padlist[pinnr].drillwidth + EPSILON))
            drillsize = 0;
    } /* for (running over pin numbers) */

    /* for any pad that has the rotate flag set, swap the width and height and
       add 90 degrees to the rotation value */
    for (int pinnr = 1; pinnr < padcount; pinnr++) {
        if (padlist[pinnr].startidx == 0 || !padlist[pinnr].rotate)
            continue;
        for (unsigned idx = padlist[pinnr].startidx; idx < module->Count(); idx++) {
            wxString line = module->Item(idx);
            wxString keyword = GetToken(&line);
            wxASSERT(keyword.CmpNoCase(wxT("$EndPAD")) != 0);   /* "Sh" must be found before end of pad */
            if (keyword.CmpNoCase(wxT("Sh")) == 0) {
                /* handle "Sh" to swap the width and height, and add a 90 degree rotation */
                wxString name = GetToken(&line);
                wxString type = GetToken(&line);
                wxString width = GetToken(&line);
                wxString height = GetToken(&line);
                wxString xdelta = GetToken(&line);
                wxString ydelta = GetToken(&line);
                long rot = GetTokenLong(&line);
                rot += 900;
                if (rot > 3600)
                    rot -= 3600;
                module->Item(idx) = wxString::Format(wxT("%s %s %s %s %s %s %s %ld"),
                                                     keyword.c_str(), name.c_str(), type.c_str(),
                                                     height.c_str(), width.c_str(), xdelta.c_str(), ydelta.c_str(),
                                                     rot);
                break;  /* no need to search further */
            } else if (keyword.CmpNoCase(wxT("(pad")) == 0) {
                wxString section = GetSection(line, wxT("at"));
                if (section.length() > 0) {
                    wxString x = GetToken(&section);
                    wxString y = GetToken(&section);
                    double rot = 0.0;
                    if (section.length() > 0)
                        rot = GetTokenDouble(&section);
                    rot += 90.0;
                    if (rot > 360.0)
                        rot -= 360.0;
                    section = wxString::Format(wxT("%s %s %.1f"), x.c_str(), y.c_str(), rot);
                    SetSection(line, wxT("at"), section);
                }
                section = GetSection(line, wxT("size"));
                if (section.length() > 0) {
                    wxString width = GetToken(&section);
                    wxString height = GetToken(&section);
                    section = height + wxT(" ") + width;
                    SetSection(line, wxT("size"), section);
                }
            }
        }
    }

    /* count the number of pads in a row span or column span */
    int pinhor_count = 0, pinver_count = 0;
    for (int pinnr = 1; pinnr < padcount; pinnr++) {
        if (padlist[pinnr].startidx <= 0)
            continue;
        if (pitchhor > EPSILON && Equal(padlist[pinnr].y, padlist[horpin_base].y, TOLERANCE))
            pinhor_count++;
        if (pitchver > EPSILON && Equal(padlist[pinnr].x, padlist[verpin_base].x, TOLERANCE))
            pinver_count++;
    }
    wxASSERT(pinhor_count == 0 && pitchhor <= EPSILON || pinhor_count > 0 && pitchhor > EPSILON);
    wxASSERT(pinver_count == 0 && pitchver <= EPSILON || pinver_count > 0 && pitchver > EPSILON);
    /* when both a horizontal span and a vertical span are found:
       - if one of the spans has only two pins, the pitch is in the other direction
       - otherwise take the smaller of the two
       - on equal size: prefer vertical on most shapes, but prefer horizontal on grid arrays
     */

    /* copy most of the information now */
    info->PadCount = padvalid;
    info->PadShape = padshape;
    info->DrillSize = (drillsize > EPSILON) ? drillsize : 0;
    if (pitchhor > EPSILON                                          /* hor. pitch is set */
        && (pitchver <= EPSILON                                     /* no ver. pitch */
            || pitchhor < pitchver - EPSILON                        /* or hor. pitch is smaller */
            || (Equal(pitchhor, pitchver) && info->MtxWidth > 0))   /* or hor. & ver. pitch are the same and it's a grid array */
        && (pinhor_count > 2 || pinver_count <= 2))                 /* hor. pitch has > 2 pads while ver. pitch has 2 pads (or no ver. pitch) */
    {
        info->Pitch = pitchhor;
        info->PitchVertical = false;
        info->PadLines = 1; /* assume, for now, may be adjusted later */
        wxASSERT(horpin_base > 0);
        wxASSERT(padlist[horpin_base - 1].startidx > 0 && padlist[horpin_base].startidx > 0);
        info->PitchPins[0] = CoordPair(padlist[horpin_base - 1].x, padlist[horpin_base - 1].y);
        info->PitchPins[1] = CoordPair(padlist[horpin_base].x, padlist[horpin_base].y);
    } else if (pitchver > EPSILON) {
        info->Pitch = pitchver;
        info->PitchVertical = true;
        info->PadLines = 1; /* assume, for now, may be adjusted later */
        wxASSERT(verpin_base > 0);
        wxASSERT(padlist[verpin_base - 1].startidx > 0 && padlist[verpin_base].startidx > 0);
        info->PitchPins[0] = CoordPair(padlist[verpin_base - 1].x, padlist[verpin_base - 1].y);
        info->PitchPins[1] = CoordPair(padlist[verpin_base].x, padlist[verpin_base].y);
    }
    /* avoid invalid values for the pad sizes */
    if (padsize[0].Equal(-1, -1))
        padsize[0].Set(0, 0);
    if (padsize[1].Equal(-1, -1))
        padsize[1].Set(0, 0);
    /* check whether to swap the pad numbers (the primary pad must be the one used most) */
    if (!info->PadSize[0].Equal(0,0) && padsizecount[1] > padsizecount[0]) {
        info->PadSize[0] = padsize[1];
        info->PadSize[1] = padsize[0];
        info->PadRightAngle[0] = padrightangle[1];
        info->PadRightAngle[1] = padrightangle[0];
        int t = padsizecount[0];
        padsizecount[0] = padsizecount[1];
        padsizecount[1] = t;
    } else {
        info->PadSize[0] = padsize[0];
        info->PadSize[1] = padsize[1];
        info->PadRightAngle[0] = padrightangle[0];
        info->PadRightAngle[1] = padrightangle[1];
    }
    /* check whether the primary pad is also the one attached to the pitch */
    if (info->Pitch > EPSILON) {
        int pin = info->PitchVertical ? (verpin_base - 1) : (horpin_base - 1);
        wxASSERT(pin >= 0);
        wxASSERT(padlist[pin].startidx > 0);
        if (padsize[0].Equal(padlist[pin].width, padlist[pin].height) || padsize[0].Equal(padlist[pin].height, padlist[pin].width))
            info->RegPadCount = padsizecount[0];
        else if (padsize[1].Equal(padlist[pin].width, padlist[pin].height) || padsize[1].Equal(padlist[pin].height, padlist[pin].width))
            info->RegPadCount = padsizecount[1];
    }

    if (padvalid > 2 && pitchhor > EPSILON && pitchver > EPSILON) {
        /* components have a span as well as a pitch, the span is detected now */
        if (padvalid == 3) {
            /* detect SOT23 and similar, as the pitch is detected incorrectly on these parts */
            if (Equal(padlist[1].x, padlist[2].x, TOLERANCE) && verpin_base == 2) {
                double d3 = padlist[3].y - padlist[1].y;
                double d2 = padlist[2].y - padlist[1].y;
                if (Equal(d3, d2 / 2, TOLERANCE)) {
                    info->SOT23pitch = true;
                    verpin_base = 3;
                    info->Pitch = pitchver / 2;
                    info->PitchVertical = true;
                    info->PitchPins[1] = CoordPair(padlist[verpin_base].x, padlist[verpin_base].y);
                    pitchver = 0;   /* clear, so the horizontal "pitch" will be detected as the span */
                }
            } else if (Equal(padlist[1].y, padlist[2].y, TOLERANCE) && horpin_base == 2) {
                /* incorrectly oriented SOT23 (or similar) */
                double d3 = padlist[3].x - padlist[1].x;
                double d2 = padlist[2].x - padlist[1].x;
                if (Equal(d3, d2 / 2, TOLERANCE)) {
                    info->SOT23pitch = true;
                    horpin_base = 3;
                    info->Pitch = pitchhor / 2;
                    info->PitchVertical = false;
                    info->PitchPins[1] = CoordPair(padlist[horpin_base].x, padlist[horpin_base].y);
                    pitchhor = 0;   /* clear, so the vertical "pitch" will be detected as the span */
                }
            }
        }

        /* if the detected horizontal pitch is different from the vertical pitch,
           then the "pitch" is set to one of these (already handled) and the single
           span is in the other direction (horizontal pitch -> vertical span, and
           vice versa); the case where the horizontal and vertical "pitch" are
           equal is handled separately */
        if (!Equal(pitchhor, pitchver)) {
            wxASSERT(info->Pitch > EPSILON);
            if (info->PitchVertical) {
                info->SpanHor = pitchhor;
                wxASSERT(horpin_base > 1);
                wxASSERT(padlist[horpin_base - 1].startidx > 0 && padlist[horpin_base].startidx > 0);
                info->SpanHorPins[0] = CoordPair(padlist[horpin_base - 1].x, padlist[horpin_base - 1].y);
                info->SpanHorPins[1] = CoordPair(padlist[horpin_base].x, padlist[horpin_base].y);
            } else {
                info->SpanVer = pitchver;
                wxASSERT(verpin_base > 0);
                wxASSERT(padlist[verpin_base - 1].startidx > 0 && padlist[verpin_base].startidx > 0);
                info->SpanVerPins[0] = CoordPair(padlist[verpin_base - 1].x, padlist[verpin_base - 1].y);
                info->SpanVerPins[1] = CoordPair(padlist[verpin_base].x, padlist[verpin_base].y);
            }
        } else {
            /* so the detected pitch is equal for horizontal and vertical, this
               can be a quad-pack or a pin array */
            wxASSERT(pitchhor - pitchver >= -EPSILON && pitchhor - pitchver <= EPSILON);
            /* first check for a pin array */
            bool matcharray = true;
            int colcount = 0, rowcount = 0;
            wxASSERT(verpin_base > 0);
            wxASSERT(padlist[verpin_base].startidx > 0);
            for (int pin1 = 1; matcharray && pin1 <= padvalid; pin1++) {
                if (padlist[pin1].startidx > 0 && Equal(padlist[pin1].x, padlist[verpin_base].x, TOLERANCE)) {
                    /* pin1 is in the same vertical row as where the vertical pitch
                       was detected */
                    rowcount++;
                    bool matchhor = false;
                    for (int pin2 = 1; !matchhor && pin2 <= padvalid; pin2++) {
                        if (pin2 != pin1 && padlist[pin2].startidx > 0
                                && Equal(padlist[pin2].y, padlist[pin1].y, TOLERANCE)) {
                            /* pin2 is in the same horizontal row as pin1, check the pitch */
                            matchhor = Equal(padlist[pin2].x, padlist[pin1].x + pitchhor, TOLERANCE)
                                    || Equal(padlist[pin2].x + pitchhor, padlist[pin1].x, TOLERANCE);
                        }
                    }
                    if (!matchhor)
                        matcharray = false; /* no opposing pin at the correct pitch was found for every pin */
                }
            }
            wxASSERT(horpin_base > 0);
            wxASSERT(padlist[horpin_base].startidx > 0);
            for (int pin1 = 1; matcharray && pin1 <= padvalid; pin1++) {
                if (padlist[pin1].startidx > 0 && Equal(padlist[pin1].y, padlist[horpin_base].y, TOLERANCE)) {
                    /* pin1 is in the same horizontal row as where the horizontal pitch
                         was detected */
                    colcount++;
                    bool matchver = false;
                    for (int pin2 = 1; !matchver && pin2 <= padvalid; pin2++) {
                        if (pin2 != pin1 && padlist[pin2].startidx > 0
                                && Equal(padlist[pin2].x, padlist[pin1].x, TOLERANCE)) {
                            /* pin2 is in the same vertical row as pin1, check the pitch */
                            matchver = Equal(padlist[pin2].y, padlist[pin1].y + pitchver, TOLERANCE)
                                    || Equal(padlist[pin2].y + pitchver, padlist[pin1].y, TOLERANCE);
                        }
                    }
                    if (!matchver)
                        matcharray = false; /* no opposing pin at the correct pitch was found for every pin */
                }
            }
            if (matcharray) {
                if (colcount == 2) {
                    info->PadLines = 2;
                    info->Pitch = pitchver;
                    info->PitchVertical = true;
                    /* for pin arrays, the pin numbering is often in a zig-zag style; for
                       the span, this is handled here, for the pitch, it is handled later
                         in the routine */
                    wxASSERT(horpin_base > 1 && padlist[horpin_base].startidx > 0);
                    for (int pinnr = horpin_base - 1; pinnr > 0; pinnr--) {
                        if (padlist[pinnr].startidx > 0 && Equal(padlist[pinnr].y, padlist[horpin_base].y, TOLERANCE)) {
                            info->SpanHor = pitchhor;
                            info->SpanHorPins[0] = CoordPair(padlist[pinnr].x, padlist[pinnr].y);
                            info->SpanHorPins[1] = CoordPair(padlist[horpin_base].x, padlist[horpin_base].y);
                            break;
                        }
                    }
                } else if (rowcount == 2) {
                    info->PadLines = 2;
                    info->Pitch = pitchhor;
                    info->PitchVertical = false;
                    wxASSERT(verpin_base > 1 && padlist[verpin_base].startidx > 0);
                    for (int pinnr = verpin_base - 1; pinnr > 0; pinnr--) {
                        if (padlist[pinnr].startidx > 0 && Equal(padlist[pinnr].x, padlist[verpin_base].x, TOLERANCE)) {
                            info->SpanVer = pitchver;
                            info->SpanVerPins[0] = CoordPair(padlist[pinnr].x, padlist[pinnr].y);
                            info->SpanVerPins[1] = CoordPair(padlist[verpin_base].x, padlist[verpin_base].y);
                            break;
                        }
                    }
                } else {
                    /* this is a BGA or PGA, we cannot currently edit these */
                    info->PitchValid = false;
                }
            } else {
                /* this is a quad-pack, meaning that the spans must be separately detected */
                info->PadLines = 4; /* this is a quad-pack */
                /* find horizontal span */
                wxASSERT(verpin_base > 0);
                wxASSERT(padlist[verpin_base].startidx > 0);
                for (int pinnr = 1; pinnr <= padvalid; pinnr++) {
                    if (padlist[pinnr].startidx > 0
                        && Equal(padlist[pinnr].x, padlist[verpin_base].x, TOLERANCE)
                        && padlist[pinnr].y > padlist[verpin_base].y)
                            verpin_base = pinnr;
                }
                for (int pinnr = 1; pinnr <= padvalid; pinnr++) {
                    if (padlist[pinnr].startidx > 0 && pinnr != verpin_base && Equal(padlist[pinnr].y, padlist[verpin_base].y, TOLERANCE)) {
                        int lo = verpin_base;
                        int hi = pinnr;
                        if (padlist[lo].x > padlist[hi].x) {
                            hi = verpin_base;
                            lo = pinnr;
                        }
                        info->SpanHorPins[0] = CoordPair(padlist[lo].x, padlist[lo].y);
                        info->SpanHorPins[1] = CoordPair(padlist[hi].x, padlist[hi].y);
                        info->SpanHor = padlist[hi].x - padlist[lo].x;
                        wxASSERT(info->SpanHor > 0);
                        break;
                    }
                }
                /* find vertical span */
                wxASSERT(horpin_base > 0);
                wxASSERT(padlist[horpin_base].startidx > 0);
                for (int pinnr = 1; pinnr <= padvalid; pinnr++) {
                    if (padlist[pinnr].startidx > 0
                        && Equal(padlist[pinnr].y, padlist[horpin_base].y, TOLERANCE)
                        && padlist[pinnr].x > padlist[verpin_base].x)
                            horpin_base = pinnr;
                }
                for (int pinnr = 1; pinnr <= padvalid; pinnr++) {
                    if (padlist[pinnr].startidx > 0 && pinnr != horpin_base && Equal(padlist[pinnr].x, padlist[horpin_base].x, TOLERANCE)) {
                        int lo = horpin_base;
                        int hi = pinnr;
                        if (padlist[lo].y > padlist[hi].y) {
                            hi = horpin_base;
                            lo = pinnr;
                        }
                        info->SpanVerPins[0] = CoordPair(padlist[lo].x, padlist[lo].y);
                        info->SpanVerPins[1] = CoordPair(padlist[hi].x, padlist[hi].y);
                        info->SpanVer = padlist[hi].y - padlist[lo].y;
                        wxASSERT(info->SpanVer > 0);
                        break;
                    }
                }
            } /* !matcharray */
        } /* pitchhor == pitchver */
    } /* more than 2 pins and 2 pitches/spans detected */

    /* To be able to adjust the pitch, also verify the number of lines (or
       rows and columns) that are "pitch-separated". If both the detected
       horizontal and vertical pitch are equal, we may safely assume that
       the number is 4; this was already handled. But a DIL lay-out needs
       to be checked for explicitly. Fortunately this is easy: it will have
       both a pitch and a span (in one direction). */
    if (info->PadLines == 1 && padvalid > 3) {
        /* a 3-pin SOT23 must remain single line */
        wxASSERT(info->Pitch > EPSILON);
        if (info->SpanHor > EPSILON || info->SpanVer > EPSILON)
            info->PadLines = 2;
    }
    /* However, if the number of "regular" pads do fit in the number of lines
       for these pads, clear all (to mark the pitch as "non-detected", and
       therefore non-editable) */
    if (info->PadLines > 1 && info->RegPadCount % info->PadLines != 0) {
        /* we do need to make an exception for 5-pin SOT23 variants */
        if (info->RegPadCount == 5
            && Equal(padlist[1].x, padlist[2].x, TOLERANCE)     /* pins 1, 2 and 3 must be vertically aligned */
            && Equal(padlist[1].x, padlist[3].x, TOLERANCE)
            && Equal(padlist[4].x, padlist[5].x, TOLERANCE)     /* pins 4 and 5 must be vertically aligned */
            && Equal(padlist[1].y, padlist[5].y, TOLERANCE)     /* pins 1 and 5 must be horizontally aligned */
            && Equal(padlist[3].y, padlist[4].y, TOLERANCE))    /* pins 2 and 4 must be horizontally aligned */
        {
            info->SOT23pitch = true;    /* 5-pin SOT23 */
        } else {
            info->RegPadCount = 0;
            info->PadLines = 0;
        }
    }
    /* for dual-row pin arrays, the pin numbering is often in a zig-zag style */
    if (info->PadLines == 2) {
        wxASSERT(info->Pitch > EPSILON);
        if (info->PitchVertical) {
            for (int pinnr = verpin_base - 1; pinnr > 0; pinnr--) {
                if (padlist[pinnr].startidx > 0 && Equal(padlist[pinnr].x, info->PitchPins[1].GetX(), TOLERANCE)) {
                    info->PitchPins[0] = CoordPair(padlist[pinnr].x, padlist[pinnr].y);
                    info->PadSequence = verpin_base - pinnr;
                    break;
                }
            }
        } else {
            for (int pinnr = horpin_base - 1; pinnr > 0; pinnr--) {
                if (padlist[pinnr].startidx > 0 && Equal(padlist[pinnr].y, info->PitchPins[1].GetY(), TOLERANCE)) {
                    info->PitchPins[0] = CoordPair(padlist[pinnr].x, padlist[pinnr].y);
                    info->PadSequence = horpin_base - pinnr;
                    break;
                }
            }
        }
    }
    /* The pitch may not be edited if the footprint is not centred; so this should
       be verified too */
    info->OriginCentred = true;
    if (info->SpanHor > 0 &&
            !Equal(-info->SpanHorPins[0].GetX(), info->SpanHorPins[1].GetX()))
        info->OriginCentred = false;
    if (info->SpanVer > 0 &&
            !Equal(-info->SpanVerPins[0].GetY(), info->SpanVerPins[1].GetY()))
        info->OriginCentred = false;

    /* For grid arrays (BGA/PGA), check for a centre void (because the number
       for pins needs to be reduced by the pins in the centre void) */
    if (info->MtxWidth > 0 && info->MtxHeight > 0) {
        int c1 = -1, r1 = -1, c2 = -1, r2 = -1;
        for (int pinnr = 1; pinnr <= padvalid; pinnr++) {
            if (padlist[pinnr].startidx <= 0) {
                int row = (pinnr - 1) / info->MtxWidth;     /* zero-based row */
                int col = pinnr - row * info->MtxWidth - 1; /* zero-based column */
                if (c1 == -1 || col < c1)
                    c1 = col;
                if (c2 == -1 || col > c2)
                    c2 = col;
                if (r1 == -1 || row < r1)
                    r1 = row;
                if (r2 == -1 || row > r2)
                    r2 = row;
            }
        }
        if (c1 >= 0)
            info->MtxCentreX = c2 - c1 + 1;
        if (r1 >= 0)
            info->MtxCentreY = r2 - r1 + 1;
        info->PadCount -= info->MtxCentreX * info->MtxCentreY;
    }

    /* a special case for connectors that have a mechanical pad on each side */
    if (info->RegPadCount > 0 && padsizecount[1] == 2 && padmech == 2) {
        wxASSERT(padvalid + 2 < padcount);
        /* if the pads are horizontally spaced and SpanHor is zero, set SpanHor */
        if (info->SpanHor < EPSILON && Equal(padlist[padvalid + 1].y, padlist[padvalid + 2].y) && !Equal(padlist[padvalid + 1].x, padlist[padvalid + 2].x)) {
            int i1, i2;
            if (padlist[padvalid + 1].x > padlist[padvalid + 2].x) {
                i1 = padvalid + 1;
                i2 = padvalid + 2;
            } else {
                i1 = padvalid + 2;
                i2 = padvalid + 1;
            }
            info->SpanHor = padlist[i1].x - padlist[i2].x;
            info->SpanHorPins[0] = CoordPair(padlist[i1].x, padlist[i1].y);
            info->SpanHorPins[1] = CoordPair(padlist[i2].x, padlist[i2].y);
        }
        /* idem for vertical span */
        if (info->SpanVer < EPSILON && Equal(padlist[padvalid + 1].x, padlist[padvalid + 2].x) && !Equal(padlist[padvalid + 1].y, padlist[padvalid + 2].y)) {
            int i1, i2;
            if (padlist[padvalid + 1].y > padlist[padvalid + 2].y) {
                i1 = padvalid + 1;
                i2 = padvalid + 2;
            } else {
                i1 = padvalid + 2;
                i2 = padvalid + 1;
            }
            info->SpanVer = padlist[i1].y - padlist[i2].y;
            info->SpanVerPins[0] = CoordPair(padlist[i1].x, padlist[i1].y);
            info->SpanVerPins[1] = CoordPair(padlist[i2].x, padlist[i2].y);
        }
    }

    /* copy pad outline (coordinates) into the footprint information, for ease of export */
    for (int pinnr = 1; pinnr < padcount; pinnr++) {
        if (padlist[pinnr].startidx > 0) {
            CoordSize cs;
            if (padlist[pinnr].angle==900 || padlist[pinnr].angle==2700)
                cs.SetMid(padlist[pinnr].x, padlist[pinnr].y, padlist[pinnr].height, padlist[pinnr].width);
            else
                cs.SetMid(padlist[pinnr].x, padlist[pinnr].y, padlist[pinnr].width, padlist[pinnr].height);
            info->Pads.Add(cs);
        }
    }

    delete[] padlist;
    return true;
}

/** Stores extra information on the footprint in the repository; this is used
 *  for the on-line overview.
 */
bool StoreFootprintInfo(const wxString& name, const wxString& description,
                        const wxString& keywords, double pitch, double span, int pincount,
                        const wxString& imagefile)
{
    #if !defined NO_CURL
        wxString params;
        params = wxT("descr=") + URLEncode(description);
        if (pitch > EPSILON)
            params += wxT("&pitch=") + wxString::Format(wxT("%.3f"), pitch);
        if (span > EPSILON)
            params += wxT("&span=") + wxString::Format(wxT("%.3f"), span);
        if (pincount > 0)
            params += wxT("&pins=") + wxString::Format(wxT("%d"), pincount);
        if (keywords.length() > 0)
            params += wxT("&keywords=") + keywords;
        wxString msg = curlPutInfo(name, wxT("footprints"), params, imagefile);
        return (msg.length() == 0);
    #else
        return true;
    #endif
}

/** Stores extra information on the symbol in the repository; this is used
 *  for the on-line overview.
 */
bool StoreSymbolInfo(const wxString& name, const wxString& description,
                     const wxString& keywords, const wxString& aliases,
                     const wxString& footprints, const wxString& imagefile)
{
    #if !defined NO_CURL
        wxString params;
        params = wxT("descr=") + URLEncode(description);
        if (aliases.length() > 0)
            params += wxT("&alias=") + URLEncode(aliases);
        if (footprints.length() > 0)
            params += wxT("&fplist=") + URLEncode(footprints);
        if (keywords.length() > 0)
            params += wxT("&keywords=") + keywords;
        wxString msg = curlPutInfo(name, wxT("symbols"), params, imagefile);
        return (msg.length() == 0);
    #else
        return true;
    #endif
}

static unsigned FindSymbolStart(const wxString& filename, const wxString& name)
{
    if (!wxFileExists(filename))
        return 0;   /* since a library file always has a header, a symbol cannot start at line 0 */
    wxTextFile file;
    if (!file.Open(filename))
        return 0;

    /* verify the header */
    wxString line = file.GetLine(0);
    if (line.Left(16).CmpNoCase(wxT("EESchema-LIBRARY")) != 0
            && line.Left(13).CmpNoCase(wxT("EESchema-LIB ")) != 0)
    {
        file.Close();
        return 0;
    }

    /* find the symbol name in the file */
    for (unsigned idx = 1; idx < file.GetLineCount(); idx++) {
        line = file.GetLine(idx);
        /* check for the first letter and for the complete word if that first letter
           matches; this is an optimization, because the "Left" and "CmpNoCase"
           methods are relatively costly (and most lines won't start with a 'D') */
        if (line[0] == wxT('D') && line.Left(4).CmpNoCase(wxT("DEF ")) == 0) {
            line = line.Mid(4);
            wxString symname = GetToken(&line);
            /* remove leading tilde, this is an "non-visible" flag */
            if (symname[0] == wxT('~'))
                symname = symname.Mid(1);
            if (symname.CmpNoCase(name) == 0) {
                /* found the part, now first back up a few lines up to the comments */
                while (idx > 2) {
                    line = file.GetLine(idx - 1);
                    if (line[0] != wxT('#'))
                        break;  /* not a comment, do not include this line */
                    idx -= 1;
                }
                return idx;
            }
        }
    }

    return 0;   /* gone through the whole file, symbol was not found */
}

static unsigned FindSymDocStart(const wxString& filename, const wxString& name)
{
    if (!wxFileExists(filename))
        return 0;   /* since a library file always has a header, a symbol cannot start at line 0 */
    wxTextFile file;
    if (!file.Open(filename))
        return 0;

    /* verify the header */
    wxString line = file.GetLine(0);
    line = line.Left(15);
    if (line.CmpNoCase(wxT("EESchema-DOCLIB")) != 0) {
        file.Close();
        return 0;
    }

    /* find the symbol name in the file */
    for (unsigned idx = 1; idx < file.GetLineCount(); idx++) {
        line = file.GetLine(idx);
        /* check for the first letter and for the complete word if that first letter
           matches; this is an optimization, because the "Left" and "CmpNoCase"
           methods are relatively costly */
        if (line[0] == wxT('$') && line.Left(5).CmpNoCase(wxT("$CMP ")) == 0) {
            line = line.Mid(5);
            wxString symname = GetToken(&line);
            if (symname.CmpNoCase(name) == 0) {
                /* found the part, now first back up a few lines up to the comments */
                while (idx > 2) {
                    line = file.GetLine(idx - 1);
                    if (line[0] != wxT('#'))
                        break;  /* not a comment, do not include this line */
                    idx -= 1;
                }
                return idx;
            }
        }
    }

    return 0;   /* gone through the whole file, symbol was not found */
}

/** ExistSymbol() checks wether the symbol is present in the library. The
 *  author name is only used for the repository. If the author name is empty
 *  (the default), the name of the current user (defined for the repository
 *  access) is used.
 */
bool ExistSymbol(const wxString& filename, const wxString& name, const wxString& author)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlGet(name, author, wxT("symbols"), 0);
            return msg.length() == 0;
        #endif
    } else {
        unsigned start = FindSymbolStart(filename, name);
        return start > 0;
    }
}

bool InsertSymbol(const wxString& filename, const wxString& name, const wxArrayString& symbol)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlPut(name, wxT("symbols"), symbol);
            return msg.length() == 0;
        #endif
    } else {
        wxTextFile file;
        if (!file.Open(filename))
            return false;

        /* verify the header */
        wxString line = file.GetLine(0);
        if (line.Left(16).CmpNoCase(wxT("EESchema-LIBRARY")) != 0) {
            file.Close();
            return false;
        }

        /* find the insertion point */
        unsigned insertionpoint = 1;
        if (file.GetLineCount() > insertionpoint) {
            line = file.GetLine(insertionpoint);
            if (line.Left(9).CmpNoCase(wxT("#encoding")) == 0)
                insertionpoint += 1;
        }
        for (unsigned idx = insertionpoint; idx < file.GetLineCount(); idx++) {
            line = file.GetLine(idx);
            if (line[0] == wxT('D') && line.Left(4).CmpNoCase(wxT("DEF ")) == 0) {
                line = line.Mid(4);
                wxString symname = GetToken(&line);
                /* remove leading tilde, this is an "non-visible" flag */
                if (symname[0] == wxT('~'))
                    symname = symname.Mid(1);
                int result = symname.CmpNoCase(name);
                if (result < 0) {
                    insertionpoint = idx;
                } else if (result > 0) {
                    break;
                } else {
                    /* found a symbol with the same name; this should not occur */
                    wxASSERT(false);
                    file.Close();
                    return false;
                }
            } else if (line[0] == wxT('#') && line.CmpNoCase(wxT("#End Library")) == 0) {
                /* if not yet broken out of the loop, the symbol must be at the end of the list */
                insertionpoint = idx;
            }
        }
        /* move the insertion point up to above the comment (there usually is one) */
        while (insertionpoint > 2) {
            line = file.GetLine(insertionpoint - 1);
            if (line[0] != wxT('#'))
                break;  /* not a comment, do not include this line */
            insertionpoint -= 1;
        }
        /* insert the symbol */
        unsigned linenr = 0;
        while (linenr < symbol.Count()) {
            wxString tmp = symbol[linenr];
            file.InsertLine(tmp, insertionpoint + linenr);
            linenr += 1;
            if (tmp.CmpNoCase(wxT("ENDDEF")) == 0)
                break;
        }
        file.Write();
        file.Close();

        /* write the remainder in the documentation file */
        if (linenr < symbol.Count()) {
            wxFileName fname(filename);
            fname.SetExt(wxT("dcm"));
            if (wxFileExists(fname.GetFullPath())) {
                file.Open(fname.GetFullPath());
            } else {
                /* create the DCM file if none exists */
                if (!file.Create(fname.GetFullPath()))
                    return true;
                file.InsertLine(wxT("EESchema-DOCLIB  Version 2.0  Date: ") + wxNow(), 0);
                file.InsertLine(wxT("#"), 1);
                file.InsertLine(wxT("#End Library"), 2);
            }
            /* again, find the insertion point */
            insertionpoint = 1;
            for (unsigned idx = 1; idx < file.GetLineCount(); idx++) {
                line = file.GetLine(idx);
                if (line[0] == wxT('$') && line.Left(5).CmpNoCase(wxT("$CMP ")) == 0) {
                    line = line.Mid(5);
                    wxString symname = GetToken(&line);
                    int result = symname.CmpNoCase(name);
                    if (result < 0) {
                        insertionpoint = idx;
                    } else if (result > 0) {
                        break;
                    } else {
                        /* found a symbol with the same name; this should not occur */
                        wxASSERT(false);
                        file.Close();
                        return false;
                    }
                } else if (line[0] == wxT('#') && line.CmpNoCase(wxT("#End Library")) == 0) {
                    /* if not yet broken out of the loop, the symbol must be at the end of the list */
                    insertionpoint = idx;
                }
            }
            /* move the insertion point up to above the comment (there usually is one) */
            while (insertionpoint > 1) {
                line = file.GetLine(insertionpoint - 1);
                if (line[0] != wxT('#'))
                    break;  /* not a comment, do not include this line */
                insertionpoint -= 1;
            }
            /* insert the symbol */
            unsigned dcmline = 0;
            while (linenr < symbol.Count()) {
                file.InsertLine(symbol[linenr], insertionpoint + dcmline);
                linenr += 1;
                dcmline += 1;
            }
            file.Write();
            file.Close();
        }
        return true;
    }
}

bool RemoveSymbol(const wxString& filename, const wxString& name)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlDelete(name, wxT("symbols"));
            return msg.length() == 0;
        #endif
    } else {
        unsigned start = FindSymbolStart(filename, name);
        if (start == 0)
            return false;
        wxTextFile file;
        wxCHECK(file.Open(filename), false);

        /* delete everything up to (and including) the "ENDDEF" */
        while (start < file.GetLineCount()) {
            wxString line = file.GetLine(start);
            file.RemoveLine(start);
            if (line.CmpNoCase(wxT("ENDDEF")) == 0)
                break;
        }
        file.Write();
        file.Close();

        /* now go through the documentation file to rename the symbol
             first replace the extension of the input library file */
        wxFileName fname(filename);
        fname.SetExt(wxT("dcm"));
        start = FindSymDocStart(fname.GetFullPath(), name);
        if (start > 0) {
            wxCHECK(file.Open(fname.GetFullPath()), false);
            while (start < file.GetLineCount()) {
                wxString line = file.GetLine(start);
                file.RemoveLine(start);
                if (line.CmpNoCase(wxT("$ENDCMP")) == 0)
                    break;
            }
            file.Write();
            file.Close();
        }

        return true;
    }
}

bool RenameSymbol(wxArrayString* symbol, const wxString& oldname, const wxString& newname)
{
    for (int idx = 0; idx < (int)symbol->Count(); idx++) {
        wxString line = (*symbol)[idx];
        if (line[0] == wxT('#') && line.Find(oldname) > 0) {
            (*symbol)[idx] = wxT("# ") + newname;
        } else if (line.Left(4).CmpNoCase(wxT("DEF ")) == 0
                             || line.Left(3).CmpNoCase(wxT("F1 ")) == 0)
        {
            (*symbol)[idx].Replace(oldname, newname);
        }
    }
    return true;
}

bool RenameSymbol(const wxString& filename, const wxString& oldname, const wxString& newname)
{
    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxArrayString module;
            wxString msg = curlGet(oldname, wxEmptyString, wxT("symbols"), &module);
            if (msg.length() > 0)
                return false;
            RenameFootprint(&module, oldname, newname);
            msg = curlPut(newname, wxT("symbols"), module);
            if (msg.length() > 0)
                return false;
            msg = curlDelete(oldname, wxT("symbols"));
            return msg.length() == 0;
        #endif
    } else {
        unsigned start = FindSymbolStart(filename, oldname);
        if (start == 0)
            return false;
        wxTextFile file;
        wxCHECK(file.Open(filename), false);

        /* include everything up to the "ENDDEF" */
        for (unsigned idx = start; idx < file.GetLineCount(); idx++) {
            wxString line = file.GetLine(idx);
            bool replace = false;
            if (line.CmpNoCase(wxT("ENDDEF")) == 0)
                break;
            if (line[0] == wxT('#') && line.Find(oldname) > 0) {
                line = wxT("# ") + newname;
                replace = true;
            } else if (line.Left(4).CmpNoCase(wxT("DEF ")) == 0
                       || line.Left(3).CmpNoCase(wxT("F1 ")) == 0)
            {
                line.Replace(oldname, newname);
                replace = true;
            }
            if (replace) {
                file.RemoveLine(idx);
                file.InsertLine(line, idx);
            }
        }
        file.Write();
        file.Close();

        /* now go through the documentation file to rename the symbol
             first replace the extension of the input library file */
        wxFileName fname(filename);
        fname.SetExt(wxT("dcm"));
        start = FindSymDocStart(fname.GetFullPath(), oldname);
        if (start > 0) {
            wxCHECK(file.Open(fname.GetFullPath()), false);
            for (unsigned idx = start; idx < file.GetLineCount(); idx++) {
                wxString line = file.GetLine(idx);
                if (line.CmpNoCase(wxT("$ENDCMP")) == 0)
                    break;
                if (line.Left(5).CmpNoCase(wxT("$CMP ")) == 0) {
                    line = wxT("$CMP ") + newname;
                    file.RemoveLine(idx);
                    file.InsertLine(line, idx);
                }
            }
            file.Write();
            file.Close();
        }

        return true;
    }
}

/** LoadSymbol()
 *  \param filename     The name of the library, or the repository string
 *  \param name         The symbol name
 *  \param author       The name of the author of the symbol; only used for the repository
 *  \param striplink    If true, the link to the template is stripped from the
 *                      symbol
 *  \param symbol       The array that will hold the symbol data on output
 */
bool LoadSymbol(const wxString& filename, const wxString& name, const wxString& author,
                                bool striplink, wxArrayString* symbol)
{
    wxASSERT(symbol != NULL);
    symbol->Clear();

    if (filename.CmpNoCase(LIB_REPOS) == 0) {
        #if defined NO_CURL
            return false;
        #else
            wxString msg = curlGet(name, author, wxT("symbols"), symbol);
            return msg.length() == 0;
        #endif
    } else {
        unsigned start = FindSymbolStart(filename, name);
        if (start == 0)
            return false;
        wxTextFile file;
        wxCHECK(file.Open(filename), false);

        /* include everything up to the "ENDDEF", except (perhaps) "F4" */
        for (unsigned idx = start; idx < file.GetLineCount(); idx++) {
            wxString line = file.GetLine(idx);
            line.Trim();
            line.Trim(false);
            if (!striplink || line.Left(3).Cmp(wxT("F4 ")) != 0)
                symbol->Add(line);
            if (line.Cmp(wxT("ENDDEF")) == 0)
                break;
        }
        file.Close();

        /* also load the description and keywords from the documentation file; these
           are simply appended to the symbol definition */
        wxFileName fname(filename);
        fname.SetExt(wxT("dcm"));
        start = FindSymDocStart(fname.GetFullPath(), name);
        if (start > 0) {
            wxCHECK(file.Open(fname.GetFullPath()), false);
            for (unsigned idx = start; idx < file.GetLineCount(); idx++) {
                wxString line = file.GetLine(idx);
                line.Trim();
                line.Trim(false);
                symbol->Add(line);
                if (line.CmpNoCase(wxT("$ENDCMP")) == 0)
                    break;
            }
            file.Close();
        }

        return true;
    }
}
