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
 *  $Id: cxffont.cpp 5686 2017-05-24 13:56:46Z thiadmer $
 */
#include <cstdio>
#include <cstring>
#include "cxffont.h"


#if !defined sizearray
  #define sizearray(a)  (sizeof(a) / sizeof(a[0]))
#endif
#if defined _WIN32
  #define strncasecmp _strnicmp
  #define strcasecmp  _stricmp
#endif

#define EPSILON 0.0000005


/* CXF font format
 *
 * The file starts with a header that contain lines starting with a '#'. A few
 * fields is this header are mandatory.
 *
 * Each character consists of a number of strokes. Each stroke is represented
 * by two coordinate pairs. Glyphs are aligned horizontally at the left (meaning
 * that the zero x-coordinate is the left extent of the character, and that all
 * x-coordinates are positive values); glyphs are aligned vertically on the
 * base line (y-coordinates may be positive and negative). The positive y-axis
 * points upwards.
 *
 * By convention, the "caps height" is 9.0. In many plotter fonts, the ascender
 * height is the same as the caps height.
 */

bool CXFFont::Read(const char *path)
{
  FILE *fp;
  if ((fp = fopen(path, "r")) == NULL)
    return false;

  bool result = true;
  char line[128]; /* lines in CXF fonts are short */
  while (fgets(line, sizearray(line), fp) != NULL && result) {
    if (line[0] == '#') {
      /* parse header line */
      char *ptr = line + 1;
      while (*ptr != '\0' && *ptr <= ' ')
        ptr++;
      if (strncasecmp(ptr, "LetterSpacing", 13) == 0) {
        ptr += 13;
        if (*ptr == ':')
          ptr++;
        m_LetterSpacing = (float)strtod(ptr, NULL);
      } else if (strncasecmp(ptr, "WordSpacing", 11) == 0) {
        ptr += 11;
        if (*ptr == ':')
          ptr++;
        m_WordSpacing = (float)strtod(ptr, NULL);
      } else if (strncasecmp(ptr, "LineSpacingFactor", 17) == 0) {
        ptr += 17;
        if (*ptr == ':')
          ptr++;
        m_LineSpacingFactor = (float)strtod(ptr, NULL);
      }
    } else if (line[0] == '[') {
      /* parse glyph */
      char *ptr;
      unsigned short code;
      if (line[1] == '#' && line[2] != ']') {
        /* Unicode character as a hexadecimal code */
        code = (unsigned short)strtol(line + 2, &ptr, 16);
      } else {
        /* ASCII character */
        code = line[1];
        ptr = line + 2;
      }
      if (*ptr != ']') {
        result = false; /* invalid (or unsupported) file format */
        break;
      }
      unsigned short count = (unsigned short)strtol(ptr + 1, NULL, 10);
      if (count == 0) {
        result = false; /* invalid (or unsupported) file format */
        break;
      }
      /* now parse the line segments, and collect consecutive segments into a
         polyline */
      CXFGlyph glyph(code);
      CXFPolyLine stroke;
      float xp = -1, yp = -1; /* force new stroke for the first coordinates */
      float width = 0;
      for (unsigned short i = 0; i < count; i++) {
        if (fgets(line, sizearray(line), fp) == 0)
          break;
        assert(line[0] == 'L');
        float x1, y1, x2, y2;
        sscanf(line, "L %f,%f,%f,%f", &x1, &y1, &x2, &y2);
        /* see whether we can continue the stroke, or whether a new one must be
           added */
        if (xp < x1 - EPSILON || xp > x1 + EPSILON || yp < y1 - EPSILON || yp > y1 + EPSILON) {
          /* non-continuous line segments, finish this polyline and start a new one */
          if (stroke.GetCount() > 0) {
            glyph.AddStroke(stroke);
            stroke.Clear();
          }
          stroke.AddPoint(CXFPoint(x1, y1));
          if (width < x1)
            width = x1;
        }
        stroke.AddPoint(CXFPoint(x2, y2));
        xp = x2;
        yp = y2;
        if (width < x2)
          width = x2;
      }
      if (stroke.GetCount() > 0)
        glyph.AddStroke(stroke);  /* add the final stroke */
      glyph.SetWidth(width);
      m_Glyphs.push_back(glyph);
    }
  }
  fclose(fp);

  /* analyze a few characters to find the typical font metrics */
  float dummy;
  GetGlyphVSize('A', &m_CapsHeight, &dummy);
  GetGlyphVSize('x', &m_XHeight, &dummy);
  GetGlyphVSize('h', &m_Ascender, &dummy);
  GetGlyphVSize('p', &dummy, &m_Descender);

  return result;
}

/** Internal function that returns the height & depth of the character. These
 *  sizes are unscaled.
 *
 *  height = character height above the base line
 *  depth = descender height (negative value, or zero)
 */
void CXFFont::GetGlyphVSize(unsigned short code, float *height, float* depth)
{
  assert(height);
  assert(depth);
  *height = *depth = 0;

  const CXFGlyph* glyph = FindGlyph(code);
  if (glyph) {
    const CXFPolyLine* stroke;
    for (size_t sidx = 0; (stroke = glyph->GetPolyLine(sidx)) != 0; sidx++) {
      const CXFPoint* pt;
      for (size_t pidx = 0; (pt = stroke->GetPoint(pidx)) != 0; pidx++) {
        if (*depth > pt->m_y)
          *depth = pt->m_y;
        else if (*height < pt->m_y)
          *height = pt->m_y;
      }
    }
  }
}

/** Internal function to find a glyph */
const CXFGlyph* CXFFont::FindGlyph(unsigned short code) const
{
  for (size_t idx = 0; idx < m_Glyphs.size(); idx++)
    if (m_Glyphs[idx].GetCode() == code)
      return &m_Glyphs[idx];
  return 0;
}

/** Returns the width of a string. Only a single-line string is handled at
 *  this moment. The returned value is scaled. This routine ignores any
 *  rotation, so the returned value is the width of the bounding box when the
 *  text is drawn at zero degrees (horizontally, left to right).
 */
float CXFFont::GetTextExtent(const wchar_t* text) const
{
  assert(text);
  float total = 0;
  for (unsigned idx = 0; text[idx]; idx++) {
    unsigned short code = text[idx];
    if (idx > 0)
      total += m_LetterSpacing;
    if (code == ' ') {
      total += m_WordSpacing;
    } else {
      const CXFGlyph* glyph = FindGlyph(code);
      if (glyph)
        total += glyph->GetWidth();
    }
  }
  return total * m_ScaleX;
}

void CXFFont::DrawText(const wchar_t* text, std::vector<CXFPolyLine>* strokes) const
{
  assert(text);
  assert(strokes);
  strokes->clear();

  float scalex = m_ScaleX, scaley = m_ScaleY;
  if (m_Rotation > 135 && m_Rotation <= 315) {
    scalex = -m_ScaleX;
    scaley = -m_ScaleY;
  }
  bool vertmode = (m_Rotation > 45 && m_Rotation <= 135) || (m_Rotation > 225 && m_Rotation <= 315);

  float xpos = 0, ypos = 0;
  switch (m_AlignHor) {
  case CXF_ALIGNLEFT:
    xpos = 0;
    break;
  case CXF_ALIGNRIGHT:
    xpos = - GetTextExtent(text);
    break;
  case CXF_ALIGNCENTRE:
    xpos = - GetTextExtent(text) / 2;
    break;
  }
  switch (m_AlignVer) {
  case CXF_ALIGNBASE:
    ypos = 0;
    break;
  case CXF_ALIGNTOP:
    ypos = m_Ascender * m_ScaleY * 1.15;  /* assume 15% internal leading above the ascender */
    break;
  case CXF_ALIGNBOTTOM:
    ypos = m_Descender * m_ScaleY * 1.1;  /* assume 10% internal leading below the descender */
    break;
  case CXF_ALIGNCENTRE:
    ypos = (m_Ascender + m_Descender) * m_ScaleY;
    break;
  }
  if (!vertmode)
    ypos = -ypos;
  float xbase = xpos; /* save, for the overbar */

  for (unsigned idx = 0; text[idx]; idx++) {
    unsigned short code = text[idx];
    if (idx > 0)
      xpos += m_LetterSpacing * scalex;
    if (code == ' ') {
      xpos += m_WordSpacing * scalex;
    } else {
      const CXFGlyph* glyph = FindGlyph(code);
      if (glyph) {
        const CXFPolyLine *stroke;
        CXFPolyLine newstroke;
        for (size_t sidx = 0; (stroke = glyph->GetPolyLine(sidx)) != 0; sidx++) {
          newstroke.Clear();
          const CXFPoint* pt;
          CXFPoint newpt;
          for (size_t pidx = 0; (pt = stroke->GetPoint(pidx)) != 0; pidx++) {
            if (vertmode) {
              newpt.m_x = ypos - (pt->m_y * scaley);
              newpt.m_y = xpos + (pt->m_x + pt->m_y * m_Slant) * scalex;
            } else {
              newpt.m_x = xpos + (pt->m_x + pt->m_y * m_Slant) * scalex;
              newpt.m_y = ypos + pt->m_y * scaley;
            }
            newstroke.AddPoint(newpt);
          }
          strokes->push_back(newstroke);
        }
        xpos += glyph->GetWidth() * scalex;
      }
    }
  }
  /* optionally draw the overbar */
  if (m_Overbar) {
    CXFPolyLine newstroke;
    float y = m_CapsHeight * scaley * 1.2;  /* overbar 12% above caps height */
    if (vertmode) {
      newstroke.AddPoint(CXFPoint(ypos - y, xbase));
      newstroke.AddPoint(CXFPoint(ypos - y, xpos));
    } else {
      newstroke.AddPoint(CXFPoint(xbase, ypos + y));
      newstroke.AddPoint(CXFPoint(xpos, ypos + y));
    }
    strokes->push_back(newstroke);
  }
}

