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
 *  $Id: libraryfunctions.h 5405 2015-11-20 09:40:55Z thiadmer $
 */
#ifndef LIBRARYFUNCTIONS_H
#define LIBRARYFUNCTIONS_H

#include <wx/dynarray.h>
#include "rpn.h"

#define TOLERANCE	0.03		/* in mm, tolerance for matching pad and drill sizes */
#define MIN_PITCH	0.2			/* in mm, deltas smaller than this are not seen as pitch */
#define EPSILON		0.000001	/* default margin for comparing floating point values */

#define LIB_NONE	wxT("(None)")
#define LIB_ALL		wxT("(All)")
#define LIB_REPOS	wxT("(Repository)")

#define PIN_UNKNOWN (-1)
#define PIN_ANOMYM  (-2)

class CoordPair {
public:
	CoordPair() : m_x(0), m_y(0) { }
	CoordPair(double x, double y) { m_x = x; m_y = y; }
	void Set(double x, double y) { m_x = x; m_y = y; }
	double GetX() const { return m_x; }
	double GetY() const { return m_y; }
	bool Equal(const CoordPair& c, double margin=EPSILON) const
		{ return (m_x - c.GetX()) < margin && (m_x - c.GetX()) > -margin && (m_y - c.GetY()) < margin && (m_y - c.GetY()) > -margin; }
	bool Equal(double x, double y, double margin=EPSILON) const
		{ return (m_x - x) < margin && (m_x - x) > -margin && (m_y - y) < margin && (m_y - y) > -margin; }
	CoordPair Swap() const { return CoordPair(m_y, m_x); }
private:
	double m_x, m_y;
};


class CoordSize {
public:
	CoordSize() : m_x(0), m_y(0), m_width(0), m_height(0) { }
	CoordSize(double x, double y, double width, double height) { Set(x, y, width, height); }
	void Set(double x, double y, double width, double height) {
		if (width < 0) {
			m_x = x + width;
			m_width = -width;
		} else {
			m_x = x;
			m_width = width;
		}
		if (height < 0) {
			m_y = y + height;
			m_height = -height;
		} else {
			m_y = y;
			m_height = height;
		}
	}
	double GetX() const { return m_x; }
	double GetY() const { return m_y; }
	double GetWidth() const { return m_width; }
	double GetHeight() const { return m_height; }

    double GetMidX() const { return m_x + m_width / 2; }
    double GetMidY() const { return m_y + m_height / 2; }

	double GetLeft() const { return m_x; }
	double GetTop() const { return m_y; }
	double GetRight() const { return m_x + m_width; }
	double GetBottom() const { return m_y + m_height; }
	void SetLeft(double x) { m_width += m_x - x; m_x = x; if (m_width < 0) m_width = 0; }
	void SetTop(double y) { m_height += m_y - y; m_y = y; if (m_height < 0) m_height = 0; }
	void SetRight(double x) { m_width = x - m_x; if (m_width < 0) m_width = 0; }
	void SetBottom(double y) { m_height = y - m_y; if (m_height < 0) m_height = 0; }
private:
	double m_x, m_y;
	double m_width, m_height;
};

inline bool Equal(double v1, double v2, double margin=EPSILON)
	{ return (v1 - v2) < margin && (v1 - v2) > -margin; }


enum {	/* module format version */
	VER_INVALID,
	VER_MIL,	/* units in 1/10th of a mil */
	VER_MM,		/* units in mm */
	VER_S_EXPR,
};

WX_DECLARE_OBJARRAY(CoordSize, ArrayCoordSize);

class FootprintInfo {
public:
	explicit FootprintInfo(int type = VER_INVALID) { Clear(type); }

	void Clear(int type) {
		Pitch = 0;
		PitchVertical = false;
		PitchValid = true;
		SpanHor = SpanVer = 0;
		PadCount = RegPadCount = 0;
		PadLines = 0;
		PadSequence = 1;
		PadShape = '\0';
		DrillSize = 0;
		SOT23pitch = false;
		MtxWidth = MtxHeight = 0;
		MtxCentreX = MtxCentreY = 0;
		OriginCentred = false;
		Type = type;
		for (int idx = 0; idx < 2; idx++)
			PadRightAngle[idx] = false;
        Pads.Clear();
	}

	/* The difference between pitch and span is that the pitch is between
	   neighbouring pins and span between pins on opposite rows. A shape like
	   SOIC has only one pitch (1.27mm), vertically. The span on an SOIC16 will
	   be between pins 1 and 16 (or between pins 8 and 9, which is the same
	   value). */

	double Pitch;	        /* pad pitch */
	CoordPair PitchPins[2];	/* the anchor pins for the pitch */
	bool PitchVertical;		/* whether the pitch is in the vertical direction (instead of horizontal) */
	bool PitchValid;		/* is the pitch/pad layout supported? */
	/* for pitch adjustment, we need to know the total number of "regular"  pins,
	   which is the total number of pins minus a thermal pad (if any) */
	int RegPadCount;
	int PadLines;			/* 1 for SIL, 2 for DIL, 4 for quad-pack */
	int PadSequence;		/* normally 1, but 2 for dual row pin headers */
	/* there is only a single pitch value, but the span can be different in both directions */
	double SpanHor, SpanVer;
	CoordPair SpanHorPins[2], SpanVerPins[2];
	bool OriginCentred;

	int PadCount;           /* this is the total, so regular pins plus aux pins/thermal pad */
	CoordPair PadSize[2];
	bool PadRightAngle[2];	/* true if rotated by 90 or 270 degrees */
	char PadShape;
	double DrillSize;

    ArrayCoordSize Pads;  /* array with pad positions (outlines) */

	/* for 3-pin SOT23, the pitch is measured between pins 1 and 3, while for
	   all others, it is between pins 1 and 2; for a 5-pin SOT23, there is a
	   gap between pins 4 and 5 */
	bool SOT23pitch;

	/* BGA and PGA use a grid (or matrix) and may have a centre void (a rectangular
	   section without balls or pins); these footprints need to keep extra data */
	int MtxWidth, MtxHeight;	/* number of columns & rows */
	int MtxCentreX, MtxCentreY;	/* size of the centre void */

	int Type;	/* one of VER_MIL, VER_MM or VER_S_EXPR */
};


class BodyInfo {
public:
	BodyInfo() { BodyLength = BodyWidth = 0; }
	void Clear() { BodyLength = BodyWidth = 0; }

	double BodyLength, BodyWidth;
};


class LabelInfo {
public:
	LabelInfo() { Clear(); }

	void Clear() {
		RefLabelSize = 0;
		RefLabelVisible = false;
		ValueLabelSize = 0;
		ValueLabelVisible = false;
	}

	double RefLabelSize;
	bool RefLabelVisible;
	double ValueLabelSize;
	bool ValueLabelVisible;
};


class PinInfo {
public:
	PinInfo() { Clear(); }

	void Clear() {
		seq = -1;
		number.Clear();
		name.Clear();
		type = Passive;
		shape = Line;
		section = Left;
        part = 0;
		x = y = 0;
	}

	enum { Left, Right, Top, Bottom, LeftCustom, RightCustom, /* --- */ SectionCount };
	enum { Line, Inverted, Clock, InvClock, Other };
	enum { Input, Output, BiDir, Tristate, Passive, OpenCol, OpenEmit, NC, Unspecified, PowerIn, PowerOut };

	int seq;		/* sequence (order of definition) */
	wxString number;/* a string, but should usually be a number */
	wxString name;	/* pin description/label */
	double x, y;	/* anchor position (in mm) */
	int type;		/* electrical type */
	int shape;		/* pin shape */
	int section;	/* side at which the pin is (assuming a rectangular shape), or custom section */
    int part;       /* zero for "common to all parts", else part number (1 = A, etc.) */
};

class PinSection {
public:
	PinSection() { SetCriterion(); }
	void Clear() { SetCriterion(); }
	void SetCriterion(const wxString& label = wxEmptyString, const wxString& side = wxEmptyString, double fraction = 0) {
		name = label;
		section = side.CmpNoCase(wxT("right")) ? PinInfo::LeftCustom : PinInfo::RightCustom;
		criterion = fraction;
	}
	bool IsValid() const { return criterion != 0 && name.Length() > 0; }
	wxString Name() const { return name; }
	int Section() const { return section; }
	double Criterion() const { return criterion; }

private:
	wxString name;
	int section;
	double criterion;
};

wxString GetToken(wxString* string);
long GetTokenLong(wxString* string);
double GetTokenDouble(wxString* string);
double GetTokenDim(wxString* string, bool unit_mm);

wxString GetSection(const wxString& string, const wxString& token, int skip = 0);
bool SetSection(wxString& string, const wxString& token, const wxString& params);

double NormalizeAngle(double angle);
long NormalizeAngle(long angle);

bool TranslateUnits(wxArrayString& module, bool from_mm, bool to_mm);
void TranslateToSexpr(wxArrayString* output, const wxArrayString& module);
void TranslateToLegacy(wxArrayString* output, const wxArrayString& module);

int AdjustPad(wxArrayString& module, FootprintInfo* current, const FootprintInfo& adjusted);
int AdjustPitchHor(wxArrayString& module, int firstpin, int lastpin, int sequence, double ymatch, double pitch);
int AdjustPitchVer(wxArrayString& module, int firstpin, int lastpin, int sequence, double xmatch, double pitch);
int AdjustPitchGrid(wxArrayString& module, double curpitch, double newpitch);
int MovePadHor(wxArrayString& module, double from_x, double to_x);
int MovePadVer(wxArrayString& module, double from_y, double to_y);

int LibraryType(const wxString& filename);

bool ExistFootprint(const wxString& filename, const wxString& nameconst, const wxString& author = wxEmptyString);
bool InsertFootprint(const wxString& filename, const wxString& name, const wxArrayString& module, bool unit_mm);
bool RemoveFootprint(const wxString& filename, const wxString& name);
bool RenameFootprint(wxArrayString* module, const wxString& oldname, const wxString& newname);
bool RenameFootprint(const wxString& filename, const wxString& oldname, const wxString& newname);
bool LoadFootprint(const wxString& filename, const wxString& name, const wxString& author,
				   bool striplink, wxArrayString* module, int* version);

bool TranslatePadInfo(wxArrayString* module, FootprintInfo* info);

wxString GetFileHeaderField(const wxString& path, const wxString& key, int sequence = 0);

bool LoadTemplate(const wxString& templatename, wxArrayString* module, bool SymbolMode);
wxString GetTemplateHeaderField(const wxString& templatename, const wxString& key,
								bool SymbolMode, int sequence = 0);
void GetTemplateSections(const wxString& templatename, PinSection sections[], size_t max);
bool ValidPinCount(long pins, const wxString& templatename, bool symbolmode);
bool FootprintFromTemplate(wxArrayString* module, const wxArrayString& templat,
						   RPNexpression& rpn, bool bodyonly);
bool SymbolFromTemplate(wxArrayString* module, const wxArrayString& templat,
						RPNexpression& rpn, const PinInfo* pininfo, int pincount);

wxString GetVRMLPath(const wxString& library, const wxArrayString& module);
bool CopyVRMLfile(const wxString& source, const wxString& target,
				  const wxString& author, const wxArrayString& module);
bool VRMLFromTemplate(const wxString vrmlpath, const wxString& templatename, RPNexpression& rpn);

wxString GetTemplateName(const wxArrayString& module);
wxString GetDescription(const wxArrayString& module, bool symbolmode);
bool SetDescription(wxArrayString& module, const wxString& description, bool symbolmode);
wxString GetKeywords(const wxArrayString& module, bool symbolmode);
bool SetKeywords(wxArrayString& module, const wxString& keywords, bool symbolmode);
bool GetBodySize(const wxArrayString& module, BodyInfo* info, bool symbolmode, bool unit_mm);
bool GetTextLabelSize(const wxArrayString& module, LabelInfo* info, bool symbolmode, bool unit_mm);
void SetTextLabelSize(wxArrayString& module, const LabelInfo& info, bool symbolmode, bool unit_mm);

wxString GetPrefix(const wxArrayString& symbol);
wxString GetAliases(const wxArrayString& symbol);
bool SetAliases(wxArrayString& module, const wxString& aliases);
wxString GetFootprints(const wxArrayString& symbol);
bool SetFootprints(wxArrayString& symbol, const wxString& footprints);
bool GetPinNames(const wxArrayString& symbol, PinInfo* info, int* count);
bool SetPinNames(wxArrayString& symbol, const PinInfo* info, int count);
void ReAssignPins(PinInfo* info, int count, const BodyInfo& body, const PinSection custsections[], size_t maxsections);

bool StoreFootprintInfo(const wxString& name, const wxString& description, const wxString& keywords, 
						double pitch, double span, int pincount, const wxString& imagefile);
bool StoreSymbolInfo(const wxString& name, const wxString& description, const wxString& keywords,
					 const wxString& aliases, const wxString& footprints, const wxString& imagefile);

bool ExistSymbol(const wxString& filename, const wxString& name, const wxString& author = wxEmptyString);
bool InsertSymbol(const wxString& filename, const wxString& name, const wxArrayString& symbol);
bool RemoveSymbol(const wxString& filename, const wxString& name);
bool RenameSymbol(wxArrayString* symbol, const wxString& oldname, const wxString& newname);
bool RenameSymbol(const wxString& filename, const wxString& oldname, const wxString& newname);
bool LoadSymbol(const wxString& filename, const wxString& name, const wxString& author, bool striplink, wxArrayString* symbol);

#endif /* LIBRARYFUNCTIONS_H */
