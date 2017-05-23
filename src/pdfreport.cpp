/*
 *  Librarian for KiCad, a free EDA CAD application.
 *  Report generation functions, based on libHaru.
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
 *  $Id: pdfreport.cpp 5685 2017-05-23 10:35:40Z thiadmer $
 */

#include <limits.h>
#include <stdio.h>
#include <time.h>

#include "pdfreport.h"
#include "librarymanager.h"
#include "libraryfunctions.h"
#include <wx/hashmap.h>
#include <wx/msgdlg.h>
#include <wx/progdlg.h>
#include <wx/tokenzr.h> 
#include "svnrev.h"


#if !defined M_PI
    #define M_PI 3.14159265358979323846
#endif


/* PDF measurements are in points and (in the PDF format) there are exactly 72
 * points in an inch
 */
#define INCH(p)       ((p)*72.0)
#define MM(mm)        ((mm)*72.0/25.4)
#define PAGEMARGIN    INCH(0.5)
#define ROWSEPARATION 6         /* in points */
#define COLUMNMARGIN  INCH(0.1)
#define COLUMN_1      INCH(1.2)
#define COLUMN_2      INCH(3.0) /* for portrait */
#define COLUMN_2_L    INCH(4.0) /* for landscape */

static int CompareFootprint(const wxString& first, const wxString& second)
{
    return first.CmpNoCase(second);
}

bool PdfReport::SetPage(const wxString& paper, bool landscape)
{
  if (paper.CmpNoCase(wxT("Letter")) == 0) {
    PaperId = HPDF_PAGE_SIZE_LETTER;
    PageWidth = INCH(8.5);
    PageHeight = INCH(11.0);
  } else if (paper.CmpNoCase(wxT("Legal")) == 0) {
    PaperId = HPDF_PAGE_SIZE_LEGAL;
    PageWidth = INCH(8.5);
    PageHeight = INCH(14.0);
  } else if (paper.CmpNoCase(wxT("Executive")) == 0) {
    PaperId = HPDF_PAGE_SIZE_EXECUTIVE;
    PageWidth = INCH(7.5);
    PageHeight = INCH(10.0);
  } else if (paper.CmpNoCase(wxT("A3")) == 0) {
    PaperId = HPDF_PAGE_SIZE_A3;
    PageWidth = INCH(11.69);
    PageHeight = INCH(16.54);
  } else if (paper.CmpNoCase(wxT("A4")) == 0) {
    PaperId = HPDF_PAGE_SIZE_A4;
    PageWidth=INCH(8.26);
    PageHeight=INCH(11.69);
  } else {
    return false;
  }

  if (landscape) {
    PaperOrientation = HPDF_PAGE_LANDSCAPE;
    double t = PageWidth;
    PageWidth = PageHeight;
    PageHeight = t;
  } else {
    PaperOrientation = HPDF_PAGE_PORTRAIT;
  }

  return true;
}

static void
#if defined _WIN32
__stdcall
#endif
hpdf_ErrorHandler(HPDF_STATUS error_no, HPDF_STATUS detail_no, void *user_data)
{
  (void)user_data;
  wxString errmsg = wxString::Format(wxT("Error %04x, code %u"), (HPDF_UINT)error_no, (HPDF_UINT)detail_no);
  wxMessageBox(errmsg);
}


/** FootprintReport() generates a report for the library. It assumes that the
 *  paper size and orientation have already been set (see SetPage()).
 *  \param parent       The parent window.
 *  \param library      The pathname to the library that is printed
 *  \param modules      An array with all the footprint names in the library
 *  \param reportfile   The path to the generated report
 */
bool PdfReport::FootprintReport(wxWindow* parent, const wxString& library, const wxArrayString& modules, const wxString& reportfile)
{
  time_t rawtime;
  time(&rawtime);

  int progresspos = 0;
  wxProgressDialog progress(wxT("Generating report"), wxT("Reading resources"),
                            2*(int)modules.Count() + 2, parent,
                            wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_ESTIMATED_TIME);

  HPDF_Doc pdf=HPDF_New(hpdf_ErrorHandler,0);
  if (pdf==NULL)
    return false; /* error message already given */

  wxString path=theApp->GetFontFile();
  if (!VFont.Read(path.mb_str(wxConvFile))) {
    wxMessageBox(wxT("Font file could not be loaded, please verify the installation"));
    return false;
  }
  VFont.SetOverbar(false);
  VFont.SetAlign(CXF_ALIGNCENTRE,CXF_ALIGNCENTRE);

  progress.Update(++progresspos,wxT("Scanning footprints, determining page lay-out"));

  HPDF_SetCompressionMode(pdf,HPDF_COMP_ALL);
  HPDF_SetCurrentEncoder(pdf,"WinAnsiEncoding");
  /* create the default font */
  HPDF_Font font=HPDF_GetFont(pdf,"Helvetica","WinAnsiEncoding");
  wxASSERT(font!=NULL);

  /* create a page, so that the font can be selected */
  HPDF_Page page=HPDF_AddPage(pdf);
  if (page==NULL)
    return false;   /* error message already given */
  HPDF_Page_SetSize(page,PaperId,PaperOrientation);
  HPDF_Page_SetWidth(page,PageWidth);
  HPDF_Page_SetHeight(page,PageHeight);

  wxStringToStringHashMap FootprintIndex;

  ModuleHeight.Clear();
  ModuleWidth.Clear();
  OffsetLeft.Clear();
  OffsetTop.Clear();
  double column1=COLUMN_1;
  double column2=(PaperOrientation==HPDF_PAGE_LANDSCAPE) ? COLUMN_2_L : COLUMN_2;
  int pagecount=0;
  int pagenr=0;
  unsigned IndexLines=(PageHeight-4*PAGEMARGIN-12-FontSize)/(FontSize*1.2)-2;
  DryRun=true;
  wxASSERT(library.length() > 0);
  do {
    pagenr=0;
    double ypos=2*PAGEMARGIN;
    wxASSERT(font!=NULL);
    for (unsigned mod=0; mod<modules.Count(); mod++) {
      double xpos=PAGEMARGIN;
      HPDF_Page_SetFontAndSize(page,font,FontSize);
      HPDF_Page_SetRGBFill(page,0,0,0);
      /* load the footprint */
      wxArrayString module;
      FootprintInfo info;
      LoadFootprint(library,modules[mod],wxEmptyString,false,&module,&info.Type);
      bool unit_mm= info.Type>=VER_MM;
      int lines = 0;
      wxString description = wxEmptyString;
      if (PrintDescription)
        description = GetDescription(module, false);
      if (description.length() > 0)
        lines++;
      if (PrintPadInfo) {
        lines+=2;
        TranslatePadInfo(&module,&info);
      }
      if (lines < 1)
        lines = 1;
      double height = 1.2*FontSize*lines;
      if (!DryRun && height<MM(ModuleHeight[mod]))
        height=MM(ModuleHeight[mod]);
      /* check whether there is space on the page for the extra headers */
      if (ypos > PageHeight - PAGEMARGIN - height || pagenr == 0) {
        /* page header */
        if (pagenr>0 && !DryRun) {
          page=HPDF_AddPage(pdf);
          wxASSERT(page!=NULL);
          HPDF_Page_SetSize(page,PaperId,PaperOrientation);
          HPDF_Page_SetWidth(page,PageWidth);
          HPDF_Page_SetHeight(page,PageHeight);
        } /* if */
        pagenr++;
        if (DryRun)
          progress.Update(progresspos,wxString::Format(wxT("Scanning footprints for page %d"),pagenr));
        else
          progress.Update(progresspos,wxString::Format(wxT("Generating page %d"),pagenr));
        /* page header & footer */
        PageHeader(pdf,page,library.utf8_str());
        PageFooter(pdf,page,rawtime,true,pagenr,pagecount);
        ypos=2*PAGEMARGIN+FontSize;
      }
      HPDF_REAL cx=HPDF_Page_TextWidth(page,modules[mod].utf8_str());
      if (column1<cx)
        column1=cx;
      Text(page,xpos,ypos,modules[mod].utf8_str(),0,HPDF_TALIGN_LEFT);
      double ybase=ypos;
      xpos+=column1+COLUMNMARGIN;
      if (description.length() > 0) {
        cx=HPDF_Page_TextWidth(page,description.utf8_str());
        if (column2<cx)
          column2=cx;
        Text(page,xpos,ybase,description.utf8_str(),0,HPDF_TALIGN_LEFT);
        ybase+=1.2*FontSize;
      }
      if (PrintPadInfo) {
        wxString msg;
        if (info.Pitch>EPSILON)
          msg=wxString::Format(wxT("Pitch: %.3f (%.1f)"),info.Pitch,info.Pitch/0.0254);
        else
          msg=wxT("Pitch: --");
        if (info.SpanHor>EPSILON && info.SpanVer>EPSILON)
          msg+=wxString::Format(wxT(", Span: %.2f x %.2f (%.1f x %.1f)"),
                                info.SpanHor,info.SpanVer,
                                info.SpanHor/0.0254,info.SpanVer/0.0254);
        else if (info.SpanHor>EPSILON)
          msg+=wxString::Format(wxT(", Span: %.2f (%.1f)"),
                                info.SpanHor,info.SpanHor/0.0254);
        else if (info.SpanVer>EPSILON && !info.PitchVertical)
          msg+=wxString::Format(wxT(", Span: %.2f (%.1f)"),
                                info.SpanVer,info.SpanVer/0.0254);
        cx=HPDF_Page_TextWidth(page,msg.mb_str(wxConvISO8859_1));
        if (column2<cx)
          column2=cx;
        Text(page,xpos,ybase,msg.mb_str(wxConvISO8859_1),0,HPDF_TALIGN_LEFT);
        ybase+=1.2*FontSize;
        if (info.PadSize[0].GetX()>EPSILON) {
          if (info.PadShape=='C' ||info.PadShape=='S')
            msg=wxString::Format(wxT("Pad size: \xd8 %.2f (%.1f)"),
                                 info.PadSize[0].GetX(), info.PadSize[0].GetX()/0.0254);
          else
            msg=wxString::Format(wxT("Pad size: %.2fx%.2f (%.1fx%.1f)"),
                                 info.PadSize[0].GetX(), info.PadSize[0].GetY(),
                                 info.PadSize[0].GetX()/0.0254, info.PadSize[0].GetY()/0.0254);
          wxString msgpad;
          if (info.PadSize[1].GetX()>EPSILON) {
            wxString msgpad;
            if (info.PadSize[1].GetY()>EPSILON)
              msgpad=wxString::Format(wxT(" / %.2fx%.2f (%.1fx%.1f)"),
                                      info.PadSize[1].GetX(), info.PadSize[1].GetY(),
                                      info.PadSize[1].GetX()/0.0254, info.PadSize[1].GetY()/0.0254);
            else
              msgpad=wxString::Format(wxT(" / \xd8 %.2f (%.1f)"),
                                      info.PadSize[1].GetX(), info.PadSize[1].GetX()/0.0254);
            msg+=msgpad;
          }
        } else {
          msg=wxT("Pad size: varies");
        }
        cx=HPDF_Page_TextWidth(page,msg.mb_str(wxConvISO8859_1));
        if (column2<cx)
          column2=cx;
        Text(page,xpos,ybase,msg.mb_str(wxConvISO8859_1),0,HPDF_TALIGN_LEFT);
        ybase+=1.2*FontSize;
      }
      /* draw the footprint */
      double extent_x1=0,extent_y1=0,extent_x2=0,extent_y2=0;
      #define UPDATE_EXTENTS(x,y) { if ((x)<extent_x1) extent_x1=(x); else if ((x)>extent_x2) extent_x2=(x); if ((y)<extent_y1) extent_y1=(y); else if ((y)>extent_y2) extent_y2=(y); }
      double xoffs,yoffs;
      double module_angle = 0;	/* all angles should be corrected with the footprint angle */
      if (DryRun) {
        xoffs=yoffs=0;
      } else {
        xoffs=OffsetLeft[mod];
        yoffs=OffsetTop[mod];
      }
      xpos+=column2+COLUMNMARGIN;
      ybase=ypos-FontSize;
      if (!DryRun) {
        /* check the extents of the footprint; if the footprint does not fit
           next to the description, put it below the descriptions */
        if (xpos+MM(ModuleWidth[mod])>PageWidth-PAGEMARGIN) {
          xpos=PageWidth-PAGEMARGIN-MM(ModuleWidth[mod]);
          ybase=ypos+1.2*FontSize*lines-FontSize;
          height=1.2*FontSize*lines + MM(ModuleHeight[mod]);
        }
      }
      for (unsigned r=0; r<module.Count(); r++) {
        wxString line=module[r];
        wxString token=GetToken(&line);
        if (token.CmpNoCase(wxT("Po")) == 0) {
          GetToken(&line);	/* ignore X position */
          GetToken(&line);	/* ignore Y position */
          if (line.length()>0)
            module_angle=GetTokenLong(&line)/10.0;
        } else if (token.CmpNoCase(wxT("DS"))==0) {
          double x1=GetTokenDim(&line,unit_mm)-xoffs;
          double y1=GetTokenDim(&line,unit_mm)-yoffs;
          double x2=GetTokenDim(&line,unit_mm)-xoffs;
          double y2=GetTokenDim(&line,unit_mm)-yoffs;
          double penwidth=GetTokenDim(&line,unit_mm);
          /* ignore layer */
          UPDATE_EXTENTS(x1,y1);
          UPDATE_EXTENTS(x2,y2);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Line(page,xpos+MM(x1),ybase+MM(y1),xpos+MM(x2),ybase+MM(y2));
        } else if (token.CmpNoCase(wxT("DC")) == 0) {
          double x=GetTokenDim(&line,unit_mm);
          double y=GetTokenDim(&line,unit_mm);
          double dx=GetTokenDim(&line,unit_mm);
          double dy=GetTokenDim(&line,unit_mm);
          double penwidth=GetTokenDim(&line,unit_mm);
          /* ignore layer */
          dx-=x;
          dy-=y;
          x-=xoffs;
          y-=yoffs;
          double radius=sqrt(dx*dx+dy*dy);
          UPDATE_EXTENTS(x-radius,y-radius);
          UPDATE_EXTENTS(x+radius,y+radius);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Circle(page,xpos+MM(x),ybase+MM(y),MM(radius),true,false);
        } else if (token.CmpNoCase(wxT("DA")) == 0) {
          double x=GetTokenDim(&line,unit_mm);
          double y=GetTokenDim(&line,unit_mm);
          double dx=GetTokenDim(&line,unit_mm);
          double dy=GetTokenDim(&line,unit_mm);
          double angle=GetTokenLong(&line) / 10.0;
          double penwidth=GetTokenDim(&line,unit_mm);
          /* ignore layer */
          dx-=x;
          dy-=y;
          x-=xoffs;
          y-=yoffs;
          double radius=sqrt(dx*dx+dy*dy);
          double startangle=atan2(dy,dx);
          double endangle=startangle + (double)angle*M_PI/180.0;
          UPDATE_EXTENTS(x-radius,y-radius);
          UPDATE_EXTENTS(x+radius,y+radius);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Arc(page,xpos+MM(x),ybase+MM(y),MM(radius),startangle,endangle,true,false);
        } else if (token.CmpNoCase(wxT("DP")) == 0) {
          GetToken(&line);	/* ignore first four unknown values */
          GetToken(&line);
          GetToken(&line);
          GetToken(&line);
          long count=GetTokenLong(&line);
          wxASSERT(count>0);
          double penwidth=GetTokenDim(&line,unit_mm);
          if (penwidth<0.1)
            penwidth=0.1;	/* minimum pen width, as we are only drawing the outline */
          /* ignore layer */
          /* Haru PDF has a limitation in drawing polygons, so we draw just the outline */
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          double x=0,y=0, startx=0,starty=0, prevx,prevy;
          for (long p=0; p<count; p++) {
            line=module[++r];
            token=GetToken(&line);
            wxASSERT(token.CmpNoCase(wxT("Dl"))==0);
            prevx=x;
            prevy=y;
            x=GetTokenDim(&line,unit_mm)-xoffs;
            y=GetTokenDim(&line,unit_mm)-yoffs;
            UPDATE_EXTENTS(x,y);
            if (p==0) {
              startx=x;
              starty=y;
            } else {
              Line(page,xpos+MM(prevx),ybase+MM(prevy),xpos+MM(x),ybase+MM(y));
            }
          }
          Line(page,xpos+MM(x),ybase+MM(y),xpos+MM(startx),ybase+MM(starty));
        } else if (toupper(token[0])==wxT('T') && isdigit(token[1])) {
          long field;
          if (DrawLabels || (token.Mid(1).ToLong(&field) && field >= 2)) {
            double x=GetTokenDim(&line,unit_mm)-xoffs;
            double y=GetTokenDim(&line,unit_mm)-yoffs;
            double cy=GetTokenDim(&line,unit_mm);
            double cx=GetTokenDim(&line,unit_mm);
            long rot=(long)NormalizeAngle(GetTokenLong(&line)/10.0-module_angle);
            double penwidth=GetTokenDim(&line,unit_mm);
            GetToken(&line);    /* ignore mirror */
            GetToken(&line);    /* ignore visibility */
            GetToken(&line);    /* ignore layer */
            if (line[0] == '"' || (line.length() > 1 && line[1] == '"')) {
              /* the italic flag is absent or it is glued to the text */
              if (line[0] != '"')
                line = line.Mid(1);
              wxASSERT(line[0] == '"');
              token = GetToken(&line);
            } else {
              GetToken(&line);  /* italic flag is present, but ignored */
              token = GetToken(&line);
            }
            UPDATE_EXTENTS(x,y-cy/2);
            UPDATE_EXTENTS(x,y+cy/2);
            HPDF_Page_SetLineWidth(page,MM(penwidth));
            HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
            StrokeText(page,xpos+MM(x),ybase+MM(y),token.wc_str(wxConvLibc),MM(cx),MM(cy),rot);
          }
        } else if (token.Cmp(wxT("(at")) == 0) {
          GetToken(&line);	/* ignore X */
          GetToken(&line);	/* ignore Y */
          if (line.length() > 0)
            module_angle=GetTokenDouble(&line);
        } else if (token.Cmp(wxT("(fp_line"))==0) {
          double x1=0,y1=0,x2=0,y2=0,penwidth=1;
          wxString section=GetSection(line,wxT("start"));
          if (section.length()>0) {
            x1=GetTokenDim(&section,true)-xoffs;
            y1=GetTokenDim(&section,true)-yoffs;
          }
          section=GetSection(line,wxT("end"));
          if (section.length()>0) {
            x2=GetTokenDim(&section,true)-xoffs;
            y2=GetTokenDim(&section,true)-yoffs;
          }
          section=GetSection(line,wxT("width"));
          if (section.length()>0)
            penwidth=GetTokenDim(&section,true);
          UPDATE_EXTENTS(x1,y1);
          UPDATE_EXTENTS(x2,y2);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Line(page,xpos+MM(x1),ybase+MM(y1),xpos+MM(x2),ybase+MM(y2));
        } else if (token.Cmp(wxT("(fp_circle")) == 0) {
          double x=0,y=0,dx=0,dy=0,penwidth=1;
          wxString section=GetSection(line,wxT("center"));
          if (section.length()>0) {
            x=GetTokenDim(&section,true);
            y=GetTokenDim(&section,true);
          }
          section=GetSection(line,wxT("end"));
          if (section.length()>0) {
            dx=GetTokenDim(&section,true);
            dy=GetTokenDim(&section,true);
          }
          section=GetSection(line,wxT("width"));
          if (section.length()>0)
            penwidth=GetTokenDim(&section,true);
          dx-=x;
          dy-=y;
          x-=xoffs;
          y-=yoffs;
          double radius=sqrt(dx*dx+dy*dy);
          UPDATE_EXTENTS(x-radius,y-radius);
          UPDATE_EXTENTS(x+radius,y+radius);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Circle(page,xpos+MM(x),ybase+MM(y),MM(radius),true,false);
        } else if (token.Cmp(wxT("(fp_arc")) == 0) {
          double x=0,y=0,dx=0,dy=0,penwidth=1;
          double angle=0;
          wxString section=GetSection(line,wxT("start"));
          if (section.length()>0) {
            x=GetTokenDim(&section,true);
            y=GetTokenDim(&section,true);
          }
          section=GetSection(line,wxT("end"));
          if (section.length()>0) {
            dx=GetTokenDim(&section,true);
            dy=GetTokenDim(&section,true);
          }
          section=GetSection(line,wxT("angle"));
          if (section.length()>0)
            angle=GetTokenDouble(&section);
          section=GetSection(line,wxT("width"));
          if (section.length()>0)
            penwidth=GetTokenDim(&section,true);
          /* ignore layer */
          dx-=x;
          dy-=y;
          x-=xoffs;
          y-=yoffs;
          double radius=sqrt(dx*dx+dy*dy);
          double startangle=atan2(dy,dx);
          double endangle=startangle + (double)angle*M_PI/180.0;
          UPDATE_EXTENTS(x-radius,y-radius);
          UPDATE_EXTENTS(x+radius,y+radius);
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          Arc(page,xpos+MM(x),ybase+MM(y),MM(radius),startangle,endangle,true,false);
        } else if (token.Cmp(wxT("(fp_poly")) == 0) {
          double penwidth=0.1;
          wxString section=GetSection(line,wxT("width"));
          if (section.length()>0) {
            penwidth=GetTokenDim(&section,true);
            if (penwidth<0.1)
              penwidth=0.1;
          }
          /* first count the number of vertices */
          section=GetSection(line,wxT("pts"));
          long count=0;
          while (GetSection(section,wxT("xy"),count).Length()>0)
            count++;
          /* Haru PDF has a limitation in drawing polygons, so we draw just the outline */
          HPDF_Page_SetLineWidth(page,MM(penwidth));
          HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
          double x=0,y=0, startx=0,starty=0, prevx,prevy;
          for (long p=0; p<count; p++) {
            wxString subsect=GetSection(section,wxT("xy"),p);
            wxASSERT(subsect.length()>0);
            prevx=x;
            prevy=y;
            x=GetTokenDim(&subsect,true)-xoffs;
            y=GetTokenDim(&subsect,true)-yoffs;
            UPDATE_EXTENTS(x,y);
            if (p==0) {
              startx=x;
              starty=y;
            } else {
              Line(page,xpos+MM(prevx),ybase+MM(prevy),xpos+MM(x),ybase+MM(y));
            }
          }
          Line(page,xpos+MM(x),ybase+MM(y),xpos+MM(startx),ybase+MM(starty));
        } else if (token.Cmp(wxT("(fp_text")) == 0) {
          wxString ident = GetToken(&line);
          if (DrawLabels || ident.Cmp(wxT("user")) == 0) {
            double x=0,y=0,cx=0,cy=0;
            double penwidth=1;
            long rot=0;
            token=GetToken(&line);  /* get the text itself */
            wxString section=GetSection(line,wxT("at"));
            if (section.length()>0) {
              x=GetTokenDim(&section,true)-xoffs;
              y=GetTokenDim(&section,true)-yoffs;
              if (section.length()>0)
                rot=(long)NormalizeAngle(GetTokenDouble(&section)-module_angle);
            }
            section=GetSection(line,wxT("effects"));
            if (section.length()>0) {
              section=GetSection(section,wxT("font"));
              if (section.length()>0) {
                wxString subsect=GetSection(section,wxT("size"));
                if (subsect.length()>0) {
                  cy=GetTokenDim(&subsect,true);
                  cx=GetTokenDim(&subsect,true);
                }
                subsect=GetSection(section,wxT("thickness"));
                if (subsect.length()>0)
                  penwidth=GetTokenDim(&subsect,true);
              }
            }
            UPDATE_EXTENTS(x,y-cy/2);
            UPDATE_EXTENTS(x,y+cy/2);
            HPDF_Page_SetLineWidth(page,MM(penwidth));
            HPDF_Page_SetRGBStroke(page,(HPDF_REAL)0.7,(HPDF_REAL)0.7,(HPDF_REAL)0.0);
            StrokeText(page,xpos+MM(x),ybase+MM(y),token.wc_str(wxConvLibc),MM(cx),MM(cy),rot);
          }
        }
        /* draw the pads */
        bool inpad = false;
        double padx = 0, pady = 0, padwidth = 0, padheight = 0, padrot = 0;
        double drillx = 0, drilly = 0, drillwidth = 0, drillheight = 0;
		double paddelta = 0;
		int paddeltadir = 0;
        wxString padpin, padshape;
        for (unsigned r=0; r<module.Count(); r++) {
          wxString line=module[r];
          if (line[0]==wxT('$')) {
            if (line.CmpNoCase(wxT("$PAD"))==0) {
              inpad=true;
              drillwidth=drillheight=0;
            } else if (line.CmpNoCase(wxT("$EndPAD")) == 0) {
                /* draw the pad */
                /* handle only 90, 180 and 270 degree rotations (for now) */
                if ((int)(padrot+0.5) == 90 || (int)(padrot+0.5) == 270) {
                  double t=padwidth;
                  padwidth=padheight;
                  padheight=t;
                  t=drillwidth;
                  drillwidth=drillheight;
                  drillheight=t;
                }
                padx-=xoffs;
                pady-=yoffs;
                UPDATE_EXTENTS(padx-padwidth/2,pady-padheight/2);
                UPDATE_EXTENTS(padx+padwidth/2,pady+padheight/2);
                HPDF_Page_SetRGBFill(page,(HPDF_REAL)0.7,(HPDF_REAL)0,(HPDF_REAL)0);
                CoordSize cs(xpos+MM(padx-padwidth/2),ybase+MM(pady-padheight/2),MM(padwidth),MM(padheight));
				if (padshape.CmpNoCase(wxT("C")) == 0) 
                  Circle(page,xpos+MM(padx),ybase+MM(pady),MM(padwidth/2),false,true);
				else if (padshape.CmpNoCase(wxT("O")) == 0)
                  RoundedRect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),
                              cs.GetWidth()<cs.GetHeight() ? cs.GetWidth() : cs.GetHeight() / 2,false,true);
				else if (padshape.CmpNoCase(wxT("T")) == 0)
                  Trapezium(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),MM(paddelta),paddeltadir,false,true);
				else
                  Rect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),false,true);
                /* optionally the hole in the pad */
                if (drillwidth>EPSILON) {
                  if (padshape==wxT('C') && padwidth-drillwidth<0.05)
                    HPDF_Page_SetRGBFill(page,(HPDF_REAL)1,(HPDF_REAL)1,(HPDF_REAL)0.5);
                  else
                    HPDF_Page_SetRGBFill(page,(HPDF_REAL)1,(HPDF_REAL)1,(HPDF_REAL)1);
                  if (drillheight>EPSILON) {
                    CoordSize cs(xpos+MM(padx+drillx-drillwidth/2),ybase+MM(pady+drilly-drillheight/2),MM(drillwidth),MM(drillheight));
                    RoundedRect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),
                                cs.GetWidth()<cs.GetHeight() ? cs.GetWidth() : cs.GetHeight() / 2,false,true);
                  } else {
                    Circle(page,xpos+MM(padx+drillx),ybase+MM(pady+drilly),MM(drillwidth/2),false,true);
                  }
                }
                /* ignore the pin name (for now) */
                inpad = false;
            }
            continue;
          } else if (line[0]==wxT('(') && line.Left(4).Cmp(wxT("(pad"))==0) {
            GetToken(&line);    /* ignore "(pad" */
            padpin=GetToken(&line);
            GetToken(&line);    /* ignore smd/thru_hole/np_thru_hole type */
            padshape=GetToken(&line);
            padrot=0;           /* preset pad rotation (frequently omitted) */
            wxString section=GetSection(line, wxT("at"));
            if (section.length()>0) {
              padx=GetTokenDim(&section,true);
              pady=GetTokenDim(&section,true);
              if (section.length()>0)
                padrot=NormalizeAngle(GetTokenDouble(&section)-module_angle);
            }
            section=GetSection(line,wxT("size"));
            if (section.length()>0) {
              padwidth=GetTokenDim(&section,true);
              padheight=GetTokenDim(&section,true);
            }
			section = GetSection(line, wxT("rect_delta"));
			if (section.length() > 0) {
			  double paddeltax = GetTokenDim(&section, true);
			  double paddeltay = GetTokenDim(&section, true);
			  if (!Equal(paddeltax, 0.0)) {
				paddelta = paddeltax;
				paddeltadir = 1;
			  } else {
				paddelta = paddeltay;
				paddeltadir = 0;
			  }
			}
            drillx=drilly=0;    /* preset drill parameters (as these are optional) */
            drillwidth=drillheight=0;
            section=GetSection(line,wxT("drill"));
            if (section.length()>0) {
              wxString sectoffset=GetSection(section,wxT("offset"));
              if (section.Left(4).Cmp(wxT("oval"))==0) {
                GetToken(&section);
                drillwidth=GetTokenDim(&section,true);
                drillheight=GetTokenDim(&section,true);
              } else {
                drillwidth=GetTokenDim(&section,true);
              }
              if (sectoffset.length() > 0) {
                drillx=GetTokenDim(&sectoffset,true);
                drilly=GetTokenDim(&sectoffset,true);
              }
            }
            /* draw the pad */
            /* handle only 90, 180 and 270 degree rotations (for now) */
            if ((int)(padrot+0.5) == 90 || (int)(padrot+0.5) == 270) {
              double t=padwidth;
              padwidth=padheight;
              padheight=t;
              t=drillwidth;
              drillwidth=drillheight;
              drillheight=t;
            }
            padx-=xoffs;
            pady-=yoffs;
            UPDATE_EXTENTS(padx-padwidth/2,pady-padheight/2);
            UPDATE_EXTENTS(padx+padwidth/2,pady+padheight/2);
            HPDF_Page_SetRGBFill(page,(HPDF_REAL)0.7,(HPDF_REAL)0,(HPDF_REAL)0);
            CoordSize cs(xpos+MM(padx-padwidth/2),ybase+MM(pady-padheight/2),MM(padwidth),MM(padheight));
            if (padshape.Cmp(wxT("circle")) == 0)
              Circle(page,xpos+MM(padx),ybase+MM(pady),MM(padwidth/2),false,true);
            else if (padshape.Cmp(wxT("oblong")) == 0)
              RoundedRect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),
                              cs.GetWidth()<cs.GetHeight() ? cs.GetWidth() : cs.GetHeight() / 2,false,true);
            else if (padshape.Cmp(wxT("trapezoid")) == 0)
              Trapezium(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),MM(paddelta),paddeltadir,false,true);			
            else
              Rect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),false,true);
            /* optionally the hole in the pad */
            if (drillwidth>EPSILON) {
              if (padshape.Cmp(wxT("circle"))==0 && padwidth-drillwidth<0.05)
                HPDF_Page_SetRGBFill(page,(HPDF_REAL)1,(HPDF_REAL)1,(HPDF_REAL)0.5);
              else
                HPDF_Page_SetRGBFill(page,(HPDF_REAL)1,(HPDF_REAL)1,(HPDF_REAL)1);
              if (drillheight>EPSILON) {
                CoordSize cs(xpos+MM(padx+drillx-drillwidth/2),ybase+MM(pady+drilly-drillheight/2),MM(drillwidth),MM(drillheight));
                RoundedRect(page,cs.GetX(),cs.GetY(),cs.GetX()+cs.GetWidth(),cs.GetY()+cs.GetHeight(),
                            cs.GetWidth()<cs.GetHeight() ? cs.GetWidth() : cs.GetHeight() / 2,false,true);
              } else {
                Circle(page,xpos+MM(padx+drillx),ybase+MM(pady+drilly),MM(drillwidth/2),false,true);
              }
            }
            /* ignore the pin name (for now) */
            continue;
          }
          if (!inpad)
            continue;
          wxString token=GetToken(&line);
          if (token.CmpNoCase(wxT("Po"))==0) {
            padx=GetTokenDim(&line,unit_mm); /* this is relative to the footprint position, but in a footprint file, the position is always 0 */
            pady=GetTokenDim(&line,unit_mm);
          } else if (token.CmpNoCase(wxT("Sh"))==0) {
            padpin=GetToken(&line);
            padshape=GetToken(&line);
            padwidth=GetTokenDim(&line,unit_mm);
            padheight=GetTokenDim(&line,unit_mm);
			double paddeltax = GetTokenDim(&line, unit_mm);
			double paddeltay = GetTokenDim(&line, unit_mm);
            padrot=NormalizeAngle(GetTokenLong(&line)/10.0-module_angle);
			if (!Equal(paddeltax, 0.0)) {
			  paddelta = paddeltax;
			  paddeltadir = 1;
			} else {
			  paddelta = paddeltay;
			  paddeltadir = 0;
			}
          } else if (token.CmpNoCase(wxT("Dr"))==0) {
            drillwidth=GetTokenDim(&line, unit_mm);
            drillx=GetTokenDim(&line,unit_mm);   /* this is relative to the pad position */
            drilly=GetTokenDim(&line,unit_mm);
            token=GetToken(&line);
            if (token==wxT('O')) {
              drillwidth=GetTokenDim(&line,unit_mm);
              drillheight=GetTokenDim(&line,unit_mm);
            }
          }
        } /* for (pad handling) */
      }
      /* store size information about the footprint */
      if (DryRun) {
        wxASSERT(mod==ModuleHeight.Count());
        ModuleHeight.Add((long)(extent_y2-extent_y1));
        wxASSERT(mod==ModuleWidth.Count());
        ModuleWidth.Add((long)(extent_x2-extent_x1));
        wxASSERT(mod==OffsetLeft.Count());
        OffsetLeft.Add((long)extent_x1);
        wxASSERT(mod==OffsetTop.Count());
        OffsetTop.Add((long)extent_y1);
        if (height<MM(extent_y2-extent_y1))
            height=MM(extent_y2-extent_y1);
      }
      /* add to index */
      if (DryRun) {
        wxString key = modules[mod];
        if (FootprintIndex.find(key) == FootprintIndex.end())
          FootprintIndex[key] = wxString::Format(wxT("%d"), pagenr);
        else
          FootprintIndex[key] += wxString::Format(wxT(", %d"), pagenr);
        wxString keywords = GetKeywords(module, false);
        keywords = keywords.Trim();
        if (keywords.Length() > 0) {
          wxArrayString aliases = wxStringTokenize(keywords, wxT(" \t,;"), wxTOKEN_STRTOK);
          for (size_t a = 0; a < aliases.Count(); a++) {
            wxString alias = aliases[a];
            if (alias.CmpNoCase(key) != 0) {
              if (FootprintIndex.find(alias) == FootprintIndex.end())
                FootprintIndex[alias] = wxT("see ") + key + wxString::Format(wxT(" (%d)"), pagenr);
              else if (FootprintIndex[alias].Left(4).Cmp(wxT("see ")) == 0 || FootprintIndex[alias].Find(wxT(", see ")) > 0)
                FootprintIndex[alias] += wxT(", ") + key + wxString::Format(wxT(" (%d)"), pagenr);
              else
                FootprintIndex[alias] += wxT("; see ") + key + wxString::Format(wxT(" (%d)"), pagenr);
            }
          }
        }
      }
      /* prepare for the next line */
      progress.Update(++progresspos);
      ypos+=height+ROWSEPARATION;
    }
    if (DryRun) {
      pagecount=pagenr;
      /* add the pages for the index */
      long lines=0;
      for (wxStringToStringHashMap::iterator iter=FootprintIndex.begin(); iter!=FootprintIndex.end(); iter++) {
        int numlines=0;
        wxString line=iter->first + wxT(" : ") + iter->second;
        TextWrap(page,PAGEMARGIN,0,(PageWidth-PAGEMARGIN)/2,PageHeight,1.2*FontSize,-1.5*FontSize,line.utf8_str(),&numlines);
        lines+=numlines;
      }
      long columns=(lines+IndexLines-1)/IndexLines;
      pagecount+=(columns+1)/2;
    }

    DryRun=!DryRun;
  } while (!DryRun);

  /* sort the index */
  wxSortedArrayString SortedIndex(CompareFootprint);
  for (wxStringToStringHashMap::iterator iter = FootprintIndex.begin(); iter != FootprintIndex.end(); iter++) {
    wxString line = iter->first + wxT(" : ") + iter->second;
    SortedIndex.Add(line);
  }
  FootprintIndex.clear();

  /* print the index */
  progress.Update(++progresspos,wxT("Generating the index"));
  DryRun=false;
  size_t base=0;
  while (base<SortedIndex.Count()) {
    page=HPDF_AddPage(pdf);
    wxASSERT(page!=NULL);
    HPDF_Page_SetSize(page,PaperId,PaperOrientation);
    HPDF_Page_SetWidth(page,PageWidth);
    HPDF_Page_SetHeight(page,PageHeight);
    /* page header & footer */
    HPDF_Page_SetRGBFill(page,0,0,0);
    PageHeader(pdf,page,library.utf8_str());
    PageFooter(pdf,page,rawtime,true,++pagenr,pagecount);
    double xpos=PAGEMARGIN;
    double ypos=2*PAGEMARGIN+FontSize;
    HPDF_Font HeaderFont=HPDF_GetFont(pdf,"Helvetica-BoldOblique","WinAnsiEncoding");
    wxASSERT(HeaderFont!=NULL);
    HPDF_Page_SetFontAndSize(page,HeaderFont,10);
    if (base==0)
      Text(page,xpos,ypos,"Index",0,HPDF_TALIGN_LEFT);
    else
      Text(page,xpos,ypos,"Index (continued)",0,HPDF_TALIGN_LEFT);
    ypos+=12;
    double ypos_base=ypos;
    HPDF_Page_SetFontAndSize(page,font,FontSize);
    /* left column */
    size_t idx=base;
    while (idx<SortedIndex.Count() && ypos<PageHeight-2*PAGEMARGIN) {
      int numlines=0;
      TextWrap(page,xpos,ypos,(PageWidth-PAGEMARGIN)/2,PageHeight-2*PAGEMARGIN,1.2*FontSize,-1.5*FontSize,SortedIndex[idx].utf8_str(),&numlines);
      ypos += 1.2*FontSize*numlines;
      idx++;
    }
    /* right column */
    ypos=ypos_base;
    xpos=(PageWidth+PAGEMARGIN)/2;
    while (idx<SortedIndex.Count() && ypos<PageHeight-2*PAGEMARGIN) {
      int numlines=0;
      TextWrap(page,xpos,ypos,PageWidth-PAGEMARGIN,PageHeight-2*PAGEMARGIN,1.2*FontSize,-1.5*FontSize,SortedIndex[idx].utf8_str(),&numlines);
      ypos += 1.2*FontSize*numlines;
      idx++;
    }
    base=idx;
  }

  HPDF_SaveToFile(pdf,reportfile.mb_str(wxConvFile));
  HPDF_Free(pdf);
  return true;
}

/** SymbolReport() generates a report for the library. It assumes that the
 *  paper size and orientation have already been set (see SetPage()).
 */
bool PdfReport::SymbolReport(wxWindow* parent, const wxString& library, const wxArrayString& symbols, const wxString& reportfile)
{
  time_t rawtime;
  time(&rawtime);

  int progresspos = 0;
  wxProgressDialog progress(wxT("Generating report"), wxT("Reading resources"),
                            4*(int)symbols.Count(), parent,
                            wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_SMOOTH | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_ESTIMATED_TIME);

  wxArrayString warnings;

  HPDF_Doc pdf=HPDF_New(hpdf_ErrorHandler,0);
  if (pdf==NULL)
    return false; /* error message already given */

  HPDF_SetCompressionMode(pdf,HPDF_COMP_ALL);
  HPDF_SetCurrentEncoder(pdf,"WinAnsiEncoding");
  /* create the fonts */
  HPDF_Font font=HPDF_GetFont(pdf,"Helvetica","WinAnsiEncoding");
  wxASSERT(font!=NULL);
  HPDF_Font fontbold=HPDF_GetFont(pdf,"Helvetica-BoldOblique","WinAnsiEncoding");
  wxASSERT(fontbold!=NULL);

  /* create a page, so that the font can be selected */
  HPDF_Page page=HPDF_AddPage(pdf);
  if (page==NULL)
    return false;   /* error message already given */
  HPDF_Page_SetSize(page,PaperId,PaperOrientation);
  HPDF_Page_SetWidth(page,PageWidth);
  HPDF_Page_SetHeight(page,PageHeight);

  /* make a run to collect all symbol names plus their alias names */
  progress.Update(progresspos,wxT("Scanning symbols"));
  wxArrayString list;
  for (unsigned sym = 0; sym < symbols.Count(); sym++) {
    list.Add(symbols[sym]);
    /* load the symbol to get the aliases */
    wxArrayString symbol;
    LoadSymbol(library,symbols[sym],wxEmptyString,false,&symbol);
    wxString aliases = GetAliases(symbol);
    if (aliases.length() > 0) {
      for ( ;; ) {
        wxString name = GetToken(&aliases);
        if (name.Length() == 0)
          break;
        list.Add(name);
      }
    }
    progress.Update(++progresspos);
  }
  list.Sort();

  /* now make a run to create the cross-reference */
  struct tagXREF {
    unsigned idx; /* index to the symbol in the list */
    unsigned alt; /* if set, an alias maps to more than one symbol */
  } *xref;
  xref = new tagXREF[list.Count()];
  for (unsigned i = 0; i < list.Count(); i++) {
    xref[i].idx = UINT_MAX;
    xref[i].alt = UINT_MAX;
  }
  progress.Update(progresspos,wxT("Building cross-reference"));
  for (unsigned sym = 0; sym < symbols.Count(); sym++) {
    wxArrayString symbol;
    LoadSymbol(library,symbols[sym],wxEmptyString,false,&symbol);
    wxString aliases = GetAliases(symbol);
    if (aliases.length() > 0) {
      for ( ;; ) {
        wxString name = GetToken(&aliases);
        if (name.Length() == 0)
          break;
        /* locate the name in the list, then map the alias name to the symbol */
        for (unsigned i = 0; i < list.Count(); i++) {
          if (name.CmpNoCase(list[i]) == 0) {
            if (xref[i].idx == UINT_MAX)
              xref[i].idx = sym;
            else if (xref[i].alt == UINT_MAX)
              xref[i].alt = sym;
            break;
          }
        }
      }
    }
    progress.Update(++progresspos);
  }

  double column1 = COLUMN_1;
  double column2 = COLUMN_1;
  int pagecount = 0;
  DryRun = true;
  wxASSERT(library.length() > 0);
  do {
    int pagenr = 0;
    double ypos = 2*PAGEMARGIN;
    wxASSERT(font != NULL);
    for (unsigned idx = 0; idx < list.Count(); idx++) {
      double xpos = PAGEMARGIN;
      HPDF_Page_SetFontAndSize(page,font,FontSize);
      HPDF_Page_SetRGBFill(page,0,0,0);
      int lines = 1;
      wxString description = wxEmptyString;
      wxString fplist = wxEmptyString;
      int pincount = 0;
      //??? also print the keywords for the symbol in the report
      unsigned sym;
      for (sym = 0; sym < symbols.Count(); sym++) {
        if (symbols[sym].CmpNoCase(list[idx]) == 0)
          break;
      }
      if (sym < symbols.Count()) {
        wxArrayString symbol;
        LoadSymbol(library,symbols[sym],wxEmptyString,false,&symbol);
        if (PrintDescription)
          description = GetDescription(symbol, true);
        if (PrintFPList) {
          fplist = GetFootprints(symbol);
          if (fplist.length() > 0 && description.Length() > 0)
            lines += 1;
        }
        GetPinNames(symbol, NULL, &pincount);
        progresspos++;
      } else {
        //??? there should be a mapping to a symbol, load that symbol for the description and the fp-list
        sym = UINT_MAX; /* set to fixed value, for easier testing */
      }

      double height = 1.2*FontSize*lines;
      /* check whether there is space on the page for the extra headers */
      if (ypos > PageHeight - PAGEMARGIN - height || pagenr == 0) {
        /* page header */
        if (pagenr>0 && !DryRun) {
          page=HPDF_AddPage(pdf);
          wxASSERT(page!=NULL);
          HPDF_Page_SetSize(page,PaperId,PaperOrientation);
          HPDF_Page_SetWidth(page,PageWidth);
          HPDF_Page_SetHeight(page,PageHeight);
        } /* if */
        pagenr++;
        if (DryRun)
          progress.Update(progresspos,wxString::Format(wxT("Scanning symbols for page %d"),pagenr));
        else
          progress.Update(progresspos,wxString::Format(wxT("Generating page %d"),pagenr));
        /* page header & footer */
        PageHeader(pdf,page,library.utf8_str());
        PageFooter(pdf,page,rawtime,false,pagenr,pagecount);
        ypos=2*PAGEMARGIN+FontSize;
        HPDF_Page_SetFontAndSize(page,fontbold,FontSize);
        Text(page,xpos,ypos,"name",0,HPDF_TALIGN_LEFT);
        Text(page,xpos+column1+COLUMNMARGIN,ypos,"defined as",0,HPDF_TALIGN_LEFT);
        Text(page,xpos+column1+column2+2*COLUMNMARGIN,ypos,"description / footprints",0,HPDF_TALIGN_LEFT);
        HPDF_Page_SetFontAndSize(page,font,FontSize);
        ypos+=2*FontSize;
      }
      HPDF_REAL cx=HPDF_Page_TextWidth(page,list[idx].utf8_str());
      if (column1<cx)
        column1=cx;
      Text(page,xpos,ypos,list[idx].utf8_str(),0,HPDF_TALIGN_LEFT);
      xpos+=column1+COLUMNMARGIN;
      HPDF_Page_SetRGBFill(page,0,0,0);
      wxString msg = wxEmptyString;
      if (xref[idx].idx != UINT_MAX) {
        msg = symbols[xref[idx].idx];
        if (xref[idx].alt != UINT_MAX)
          msg += wxT(" ") + symbols[xref[idx].alt];
      } else if (pincount > 0) {
        msg = wxString::Format(wxT("%d pins"), pincount);
      }
      Text(page,xpos,ypos,msg.utf8_str(),0,HPDF_TALIGN_LEFT);
      cx=HPDF_Page_TextWidth(page,list[idx].utf8_str());
      if (column2<cx)
        column2=cx;
      if (xref[idx].alt != UINT_MAX || (xref[idx].alt != UINT_MAX && sym < UINT_MAX)) {
        HPDF_Page_SetRGBFill(page,1.0,0,0); /* error/warning, so red colour */
        double x2 = xpos + column2 + COLUMNMARGIN;
        if (xref[idx].alt != UINT_MAX)
          Text(page,x2,ypos,"alias maps to multiple symbols",0,HPDF_TALIGN_LEFT);
        else
          Text(page,x2,ypos,"alias collides with symbol",0,HPDF_TALIGN_LEFT);
        HPDF_Page_SetRGBFill(page,0,0,0);
      } else if (description.length() > 0) {
        double x2 = xpos + column2 + COLUMNMARGIN;
        Text(page,x2,ypos,description.utf8_str(),0,HPDF_TALIGN_LEFT);
      }
      if (fplist.length() > 0) {
        double x2 = xpos + column2 + COLUMNMARGIN;
        double y2 = ypos;
        if (description.length() > 0)
          y2 += 1.2 * FontSize;
        Text(page,x2,y2,fplist.utf8_str(),0,HPDF_TALIGN_LEFT);
      }
      /* prepare for the next line */
      progress.Update(progresspos);
      ypos+=height+ROWSEPARATION;
    }
    pagecount=pagenr;
    DryRun=!DryRun;
  } while (!DryRun);

  delete[] xref;
  HPDF_SaveToFile(pdf,reportfile.mb_str(wxConvFile));
  HPDF_Free(pdf);
  return true;
}

/** Text() draws a text string. The coordinates are in points and refer to
 *  the baseline of the text. The origin is the upper left corner of the page.
 *  The alignment is from the HPDF_TextAlignment enumeration. The font and the
 *  text colour should have been set earlier.
 */
void PdfReport::Text(HPDF_Page page, double x, double y, const char *text,
                     double angle, HPDF_TextAlignment align)
{
  if (!DryRun) {
    HPDF_REAL height=HPDF_Page_GetHeight(page);
    HPDF_REAL cx;
    HPDF_TransMatrix mtx;

    switch (align) {
    case HPDF_TALIGN_LEFT:
      cx=0.0;
      break;
    case HPDF_TALIGN_RIGHT:
      cx=HPDF_Page_TextWidth(page,text);
      break;
    case HPDF_TALIGN_CENTER:
      cx=HPDF_Page_TextWidth(page,text)/2.0;
      break;
    default:
      wxASSERT(false);
      cx = 0;
    }
    HPDF_Page_BeginText(page);
    mtx = HPDF_Page_GetTransMatrix(page);
    if (angle == 0) {
      HPDF_Page_MoveTextPos(page,x-cx,height-y);
    } else {
      HPDF_Page_SetTextMatrix(page,
                              cos(angle*M_PI/180.0),
                              sin(angle*M_PI/180.0),
                              -sin(angle*M_PI/180.0),
                              cos(angle*M_PI/180.0),
                              x, height-y-cx);
    }
    HPDF_Page_ShowText(page,text);
    if (angle != 0)
      HPDF_Page_SetTextMatrix(page, mtx.a, mtx.b, mtx.c, mtx.d, mtx.x, mtx.y);
    HPDF_Page_EndText(page);
  }
}

/** TextWrap() draws a text string with wrap-around in a rectangular area.
 *  The coordinates are in points. The origin is the upper left corner of the
 *  page. The text is always left-aligned. The font and the text colour should
 *  have been set earlier.
 */
unsigned PdfReport::TextWrap(HPDF_Page page, double x1, double y1, double x2, double y2,
                             double linespacing, double firstline, const char *text, int *numlines)
{
  unsigned count;
  HPDF_REAL height,width;
  HPDF_REAL cx=0;
  char *start,*ptr,*cutpoint;
  char ch='\0';

  if (x1>x2) {
    double t=x1;
    x1=x2;
    x2=t;
  }
  if (y1>y2) {
    double t=y1;
    y1=y2;
    y2=t;
  }
  if (firstline>0.001)
      x1+=firstline;
  width=x2-x1;
  height=HPDF_Page_GetHeight(page);
  if (numlines!=NULL)
    *numlines=0;
  count=0;

  wxASSERT(text!=NULL);
  start=(char *)text;
  while (*start!='\0' && y1<y2) {
    /* find next space */
    ptr=start;
    do {
      cutpoint=ptr;
      if (*ptr=='\0')
        break;
      if (*ptr==' ')
        ptr++;  /* skip space */
      while (*ptr!='\0' && *ptr!=' ')
        ptr++;
      /* check width of the line */
      ch=*ptr;
      *ptr='\0';
      cx=HPDF_Page_TextWidth(page,start);
      *ptr=ch;
    } while (cx<=width);
    if (cutpoint==start && cx>width) {
      /* there was no space, but the text did not fit; try cutting at a dash
         or a comma */
      ptr=start;
      do {
        cutpoint=ptr;
        if (*ptr=='\0')
          break;
        if (*ptr=='-' || *ptr==',' || *ptr==';' || *ptr==':')
          ptr++;
        while (*ptr!='\0' && *ptr!='-' && *ptr!=',' && *ptr!=';' && *ptr!=':')
          ptr++;
        /* check width of the line */
        if (*ptr!='\0') {
          ch=*(ptr+1);
          *(ptr+1)='\0';
        }
        cx=HPDF_Page_TextWidth(page,start);
        if (*ptr!='\0')
          *(ptr+1)=ch;
      } while (cx<=width);
      if (cutpoint!=start)
        cutpoint+=1;  /* cut behind the dash or comma */
    }
    if (cutpoint==start)
      cutpoint=strchr(start,'\0');
    wxASSERT(cutpoint!=NULL && cutpoint!=start);
    ch=*cutpoint;
    *cutpoint='\0';
    if (!DryRun) {
      HPDF_Page_BeginText(page);
      HPDF_Page_MoveTextPos(page,x1,height-y1);
      HPDF_Page_ShowText(page,start);
      HPDF_Page_EndText(page);
    }
    count+=(unsigned)strlen(start);
    *cutpoint=ch;
    start=cutpoint;
    if (*start==' ') {
      start+=1;
      count++;
    }
    if (firstline<-0.001 || firstline>0.001) {
      x1-=firstline;
      width=x2-x1;
      firstline=0;
    }
    if (numlines!=NULL)
      *numlines+=1;
    y1+=linespacing;
  }
  return count;
}

void PdfReport::StrokeText(HPDF_Page page,double x,double y,const wchar_t *text,double cx,double cy,int angle)
{
  if (!DryRun) {
    VFont.SetRotation(angle);
    VFont.SetScale(cx/CXF_CAPSHEIGHT,cy/CXF_CAPSHEIGHT);

    /* create a stroke array for the text (applying scale and rotation) */
    std::vector<CXFPolyLine> strokes;
    VFont.DrawText(text,&strokes);

    HPDF_REAL height=HPDF_Page_GetHeight(page);
    for (size_t sidx = 0; sidx < strokes.size(); sidx++) {
      const CXFPolyLine *stroke = &strokes[sidx];
      const CXFPoint* pt;
      if ((pt=stroke->GetPoint(0))!=0)
        HPDF_Page_MoveTo(page,x+pt->m_x,height-y+pt->m_y);
      for (size_t pidx=1; (pt=stroke->GetPoint(pidx))!=0; pidx++)
        HPDF_Page_LineTo(page,x+pt->m_x,height-y+pt->m_y);
    }
    HPDF_Page_Stroke(page);
  }
}

void PdfReport::Line(HPDF_Page page,double x1,double y1,double x2,double y2)
{
  if (!DryRun) {
    HPDF_REAL height=HPDF_Page_GetHeight(page);
    HPDF_Page_MoveTo(page,x1,height-y1);
    HPDF_Page_LineTo(page,x2,height-y2);
    HPDF_Page_Stroke(page);
  }
}

void PdfReport::Rect(HPDF_Page page,double x1,double y1,double x2,double y2,bool border,bool filled)
{
  if (!DryRun) {
    HPDF_REAL height;

    if (x1>x2) {
      double t=x1;
      x1=x2;
      x2=t;
    }
    if (y1>y2) {
      double t=y1;
      y1=y2;
      y2=t;
    }
    height=HPDF_Page_GetHeight(page);
    HPDF_Page_Rectangle(page,x1,height-y2,x2-x1,y2-y1);
    if (border && filled)
      HPDF_Page_FillStroke(page);
    else if (filled)
      HPDF_Page_Fill(page);
    else
      HPDF_Page_Stroke(page);
  }
}

void PdfReport::RoundedRect(HPDF_Page page,double x1,double y1,double x2,double y2,double radius,bool border,bool filled)
{
  if (!DryRun) {
    HPDF_REAL height;

    if (x1>x2) {
      double t=x1;
      x1=x2;
      x2=t;
    }
    if (y1>y2) {
      double t=y1;
      y1=y2;
      y2=t;
    }
    height=HPDF_Page_GetHeight(page);
    double x=x1;
    double y=height-y2;
    double w=x2-x1;
    double h=y2-y1;

    //   d 8---------7 c
    //    /           \
    //   1             6
    //   |             |
    //   2             5
    // a'.\           /.b"
    //  a".3---------4.b'

    const double kappa =  4 * (sqrt(2.0) - 1) / 3;  // see http://www.whizkidtech.redprince.net/bezier/circle/
    const double l = kappa * radius ;

    // equivalent to HPDF_Page_Rectangle(page, x, y, w, h)
    // from a message by Laurent on LibHaru Google groups

    HPDF_Page_MoveTo(page,  x                 , y + radius);        // 1
    HPDF_Page_LineTo(page,  x                 , y + h - radius);    // 2
    HPDF_Page_CurveTo(page, x                 , y + h - radius + l, // a'
                            x + radius - l    , y + h,              // a"
                            x + radius        , y + h);             // 3
    HPDF_Page_LineTo(page,  x + w - radius    , y + h);             // 4
    HPDF_Page_CurveTo(page, x + w - radius + l, y + h,              // b'
                            x + w             , y + h - radius + l, // b"
                            x + w             , y + h - radius);    // 5
    HPDF_Page_LineTo(page,  x + w             , y + radius);        // 6
    HPDF_Page_CurveTo(page, x + w             , y + radius - l,     // c'
                            x + w - radius + l, y ,
                            x + w -radius     , y);                 // 7
    HPDF_Page_LineTo(page,  x + radius        , y);                 // 8
    HPDF_Page_CurveTo(page, x + radius - l    , y ,                 // d'
                            x                 , y + radius - l,     //d"
                            x                 , y + radius);        // 1

    if (border && filled)
      HPDF_Page_FillStroke(page);
    else if (filled)
      HPDF_Page_Fill(page);
    else
      HPDF_Page_Stroke(page);
  }
}

void PdfReport::Trapezium(HPDF_Page page,double x1,double y1,double x2,double y2,double delta,int orientation,bool border,bool filled)
{
  if (!DryRun) {
    HPDF_REAL height=HPDF_Page_GetHeight(page);
    y1=height-y1;
    y2=height-y2;

    if (x1>x2) {
      double t=x1;
      x1=x2;
      x2=t;
    }
    if (y1>y2) {
      double t=y1;
      y1=y2;
      y2=t;
    }

	if (orientation == 0) {
		HPDF_Page_MoveTo(page, x1 + delta / 2, y1);
		HPDF_Page_LineTo(page, x2 - delta / 2, y1);
		HPDF_Page_LineTo(page, x2 + delta / 2, y2);
		HPDF_Page_LineTo(page, x1 - delta / 2, y2);
		HPDF_Page_LineTo(page, x1 + delta / 2, y1);
	} else {
		HPDF_Page_MoveTo(page, x1, y1 + delta / 2);
		HPDF_Page_LineTo(page, x1, y2 - delta / 2);
		HPDF_Page_LineTo(page, x2, y2 + delta / 2);
		HPDF_Page_LineTo(page, x2, y1 - delta / 2);
		HPDF_Page_LineTo(page, x1, y1 + delta / 2);
	}

    if (border && filled)
      HPDF_Page_FillStroke(page);
    else if (filled)
      HPDF_Page_Fill(page);
    else
      HPDF_Page_Stroke(page);
  }
}

void PdfReport::Circle(HPDF_Page page,double x,double y,double radius,bool border,bool filled)
{
  if (!DryRun) {
    HPDF_REAL height=HPDF_Page_GetHeight(page);
    HPDF_Page_Circle(page,x,height-y,radius);
    if (border && filled)
      HPDF_Page_FillStroke(page);
    else if (filled)
      HPDF_Page_Fill(page);
    else
      HPDF_Page_Stroke(page);
  }
}

void PdfReport::Arc(HPDF_Page page,double x,double y,double radius,double angle1,double angle2,bool border,bool filled)
{
  if (!DryRun) {
    HPDF_REAL height=HPDF_Page_GetHeight(page);
    HPDF_Page_Arc(page,x,height-y,radius,angle1,angle2);
    if (border && filled)
      HPDF_Page_FillStroke(page);
    else if (filled)
      HPDF_Page_Fill(page);
    else
      HPDF_Page_Stroke(page);
  }
}

void PdfReport::PageHeader(HPDF_Doc pdf,HPDF_Page page,const char *project)
{
  if (!DryRun) {
    HPDF_Font font=HPDF_GetFont(pdf,"Helvetica-BoldOblique","WinAnsiEncoding");
    wxASSERT(font!=NULL);
    HPDF_Page_SetFontAndSize(page,font,10);
    Text(page,PAGEMARGIN,PAGEMARGIN+10,project,0,HPDF_TALIGN_LEFT);
  }
}

void PdfReport::PageFooter(HPDF_Doc pdf,HPDF_Page page,time_t timestamp,bool printunits,
                           int pagenum,int pagecount)
{
  if (!DryRun) {

    HPDF_REAL width=HPDF_Page_GetWidth(page);
    HPDF_REAL height=HPDF_Page_GetHeight(page);

    HPDF_Font font=HPDF_GetFont(pdf,"Helvetica","WinAnsiEncoding");
    wxASSERT(font!=NULL);
    HPDF_Page_SetFontAndSize(page,font,8);
    HPDF_Page_SetRGBFill(page,0,0,0);

    Text(page,PAGEMARGIN,height-PAGEMARGIN,"KiCad Librarian",0,HPDF_TALIGN_LEFT);

    if (printunits)
      Text(page,width/2,height-PAGEMARGIN,"Dimensions in mm (mil)",0,HPDF_TALIGN_CENTER);

	char str[100];
    struct tm *timeinfo;
    timeinfo=localtime(&timestamp);
    strftime(str,sizeof(str),"%Y-%m-%d %H:%M:%S",timeinfo);
    sprintf(str+strlen(str)," - Page %d/%d",pagenum,pagecount);
    Text(page,width-PAGEMARGIN,height-PAGEMARGIN,str,0,HPDF_TALIGN_RIGHT);
  }
}
