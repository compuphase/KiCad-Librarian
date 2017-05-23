/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  Support for CXF fonts.
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
 *  $Id: cxffont.h 5685 2017-05-23 10:35:40Z thiadmer $
 */
#ifndef __cxffont_h
#define __cxffont_h

#include <cassert>
#include <vector>
#include <wx/gdicmn.h>
#include <wx/graphics.h>
#include <wx/pen.h>
#include <wx/string.h>


class CXFPoint {
public:
  CXFPoint(float x=0, float y=0) : m_x(x), m_y(y) {}
  float m_x, m_y;
};

class CXFPolyLine {
public:
  CXFPolyLine() { m_Points.clear(); }
  void Clear() { m_Points.clear(); }
  size_t GetCount() const { return m_Points.size(); }
  void AddPoint(const CXFPoint& pt) { m_Points.push_back(pt); }
  const CXFPoint* GetPoint(size_t index) const { return (index < m_Points.size()) ? &m_Points[index] : 0; }
private:
  std::vector<CXFPoint> m_Points;
};

class CXFGlyph {
public:
  explicit CXFGlyph(unsigned short code = 0) : m_Code(code), m_width(1.0) { m_Strokes.clear(); }
  void Clear() { m_Strokes.clear(); }
  void SetCode(unsigned short code) { m_Code = code; }
  unsigned short GetCode() const { return m_Code; }
  void SetWidth(float width) { m_width = width; }
  float GetWidth() const { return m_width; }
  size_t GetCount() const { return m_Strokes.size(); }
  void AddStroke(const CXFPolyLine& stroke) { m_Strokes.push_back(stroke); }
  const CXFPolyLine* GetPolyLine(size_t index) const { return (index < m_Strokes.size()) ? &m_Strokes[index] : 0; }
private:
  unsigned short m_Code;  /* the character code (Unicode) */
  float m_width;
  std::vector<CXFPolyLine> m_Strokes;
};

enum {
  CXF_ALIGNLEFT,
  CXF_ALIGNRIGHT,
  CXF_ALIGNCENTRE,
  CXF_ALIGNBASE,
  CXF_ALIGNTOP,
  CXF_ALIGNBOTTOM,
};

class CXFFont {
public:
  CXFFont() : m_LetterSpacing(0), m_WordSpacing(0), m_LineSpacingFactor(0),
    m_CapsHeight(0), m_XHeight(0), m_Ascender(0), m_Descender(0),
    m_ScaleX(1), m_ScaleY(1), m_Rotation(0), m_Overbar(false),
    m_AlignHor(CXF_ALIGNLEFT), m_AlignVer(CXF_ALIGNBASE), m_Slant(0)
    { m_Glyphs.clear(); }
  ~CXFFont() { m_Glyphs.clear(); }

  bool Read(const char *path);

  /* all returned values depend on the scale */
  float GetCapsHeight() const { return m_CapsHeight; }
  float GetXHeight() const { return m_XHeight * m_ScaleY; }
  float GetAscender() const { return m_Ascender * m_ScaleY; }
  float GetDescender() const { return m_Descender * m_ScaleY; }
  float GetLetterSpacing() const { return m_LetterSpacing * m_ScaleY; }
  float GetWordSpacing() const { return m_WordSpacing * m_ScaleY; }
  float GetTextExtent(const wchar_t* text) const;
  void DrawText(const wchar_t* text, std::vector<CXFPolyLine>* strokes) const;

  void SetScale(float scale) { m_ScaleX = m_ScaleY = scale; }
  void SetScale(float xscale, float yscale) { m_ScaleX = xscale; m_ScaleY = yscale; }
  void SetRotation(int rotation) { m_Rotation = rotation; while (m_Rotation < 0) m_Rotation += 360; while (m_Rotation > 360) m_Rotation -= 360; }
  void SetOverbar(bool overbar) { m_Overbar = overbar; }
  void SetAlign(int hor, int ver) { m_AlignHor = hor; m_AlignVer = ver; }
  void SetItalic(bool italic) { m_Slant = italic ? 0.4f : 0; }

private:
  /* font characteristics */
  float m_LetterSpacing;
  float m_WordSpacing;
  float m_LineSpacingFactor;
  float m_CapsHeight;
  float m_XHeight;
  float m_Ascender;
  float m_Descender;

  /* font options */
  float m_ScaleX, m_ScaleY;
  int m_Rotation; /* limited to 0, 90, 180, 270 */
  bool m_Overbar;
  int m_AlignHor, m_AlignVer;
  float m_Slant;

  std::vector<CXFGlyph> m_Glyphs;

  void GetGlyphVSize(unsigned short code, float *height, float* depth);
  const CXFGlyph* FindGlyph(unsigned short code) const;
};

#define CXF_CAPSHEIGHT  9.0

#endif /* __cxffont_h */
