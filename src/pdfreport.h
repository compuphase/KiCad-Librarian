/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  Report generation functions, based on libHaru.
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
 *  $Id: pdfreport.h 5685 2017-05-23 10:35:40Z thiadmer $
 */
#ifndef PDFREPORT_H
#define PDFREPORT_H

#include <wx/dynarray.h>
#include <wx/frame.h>
#include <wx/string.h>
#include <hpdf.h>
#include "cxffont.h"


class PdfReport {
public:
  bool SetPage(const wxString& paper, bool landscape = false);
  void SetFont(int points) { FontSize = points; }

  bool FootprintReport(wxWindow* parent, const wxString& library, const wxArrayString& modules, const wxString& reportfile);
  void FootprintOptions(bool description, bool padinfo, bool labels) {
    PrintDescription = description;
    PrintPadInfo = padinfo;
    DrawLabels = labels;
  }

  bool SymbolReport(wxWindow* parent, const wxString& library, const wxArrayString& symbols, const wxString& reportfile);
  void SymbolOptions(bool description, bool fp_list) {
    PrintDescription = description;
    PrintFPList = fp_list;
  }

private:
  void Text(HPDF_Page page,double x,double y,const char *text,double angle,HPDF_TextAlignment align);
  unsigned TextWrap(HPDF_Page page,double x1,double y1,double x2,double y2,double linespacing,double firstline,const char *text,int *numlines);
  void Line(HPDF_Page page,double x1,double y1,double x2,double y2);
  void Circle(HPDF_Page page,double x,double y,double radius,bool border,bool filled);
  void Arc(HPDF_Page page,double x,double y,double radius,double angle1,double angle2,bool border,bool filled);
  void Rect(HPDF_Page page,double x1,double y1,double x2,double y2,bool border,bool filled);
  void RoundedRect(HPDF_Page page,double x1,double y1,double x2,double y2,double radius,bool border,bool filled);
  void Trapezium(HPDF_Page page,double x1,double y1,double x2,double y2,double delta,int orientation,bool border,bool filled);
  void StrokeText(HPDF_Page page,double x,double y,const wchar_t *text,double cx,double cy,int angle);

  void PageHeader(HPDF_Doc pdf,HPDF_Page page,const char *project);
  void PageFooter(HPDF_Doc pdf,HPDF_Page page,time_t timestamp,bool printunits,
                  int pagenum,int pagecount);

private:
  wxString Paper;
  double PageWidth, PageHeight;
  bool PrintDescription;
  bool PrintPadInfo;
  bool DrawLabels;
  bool PrintFPList;
  double FontSize;	/* in points */

  HPDF_PageSizes PaperId;
  HPDF_PageDirection PaperOrientation;

  HPDF_Doc pdf;
  HPDF_Page page;
  bool DryRun;

  CXFFont VFont;

  wxArrayLong OffsetLeft;
  wxArrayLong OffsetTop;
  wxArrayLong ModuleHeight;
  wxArrayLong ModuleWidth;
};

#endif /* PDFREPORT_H */