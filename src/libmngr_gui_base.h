///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version Feb 14 2017)
// http://www.wxformbuilder.org/
//
// PLEASE DO "NOT" EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#ifndef __LIBMNGR_GUI_BASE_H__
#define __LIBMNGR_GUI_BASE_H__

#include <wx/artprov.h>
#include <wx/xrc/xmlres.h>
#include <wx/string.h>
#include <wx/bitmap.h>
#include <wx/image.h>
#include <wx/icon.h>
#include <wx/menu.h>
#include <wx/gdicmn.h>
#include <wx/font.h>
#include <wx/colour.h>
#include <wx/settings.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/panel.h>
#include <wx/aui/aui.h>
#include <wx/aui/auibar.h>
#include <wx/stattext.h>
#include <wx/radiobut.h>
#include <wx/spinctrl.h>
#include <wx/textctrl.h>
#include <wx/grid.h>
#include <wx/checkbox.h>
#include <wx/scrolwin.h>
#include <wx/splitter.h>
#include <wx/statusbr.h>
#include <wx/frame.h>
#include <wx/listbox.h>
#include <wx/dialog.h>
#include <wx/radiobox.h>
#include <wx/statline.h>
#include <wx/clrpicker.h>
#include <wx/statbmp.h>
#include <wx/spinbutt.h>

///////////////////////////////////////////////////////////////////////////

#define IDM_RENAMELIBRAY 1000
#define IDM_NEWFOOTPRINT 1001
#define IDM_NEWSYMBOL 1002
#define IDM_REPORTFOOTPRINT 1003
#define IDM_REPORTSYMBOL 1004
#define IDM_FOOTPRINTMODE 1005
#define IDM_SCHEMATICMODE 1006
#define IDM_COMPAREMODE 1007
#define IDM_DETAILSPANEL 1008
#define IDM_SYNCMODE 1009
#define IDM_FILTER 1010
#define IDM_DLGREMOTE 1011
#define IDM_DLGREPORT 1012
#define IDM_TEMPLATEOPTIONS 1013
#define IDM_DLGOPTIONS 1014
#define IDM_LEFT_LIB 1015
#define IDM_LEFT_MOD 1016
#define IDM_MOVEMODULE 1017
#define IDM_COPYMODULE 1018
#define IDM_DELETEMODULE 1019
#define IDM_RENAMEMODULE 1020
#define IDM_RIGHT_LIB 1021
#define IDM_RIGHT_MOD 1022
#define IDT_ZOOMIN 1023
#define IDT_ZOOMOUT 1024
#define IDT_3DVIEW 1025
#define IDT_MEASUREMENTS 1026
#define IDT_DETAILSPANEL 1027
#define IDT_LEFTFOOTPRINT 1028
#define IDT_RIGHTFOOTPRINT 1029
#define IDM_PANELVIEW 1030
#define IDC_PADSHAPE 1031
#define IDC_BODYSHAPE 1032
#define IDT_SAVE 1033
#define IDT_REVERT 1034

///////////////////////////////////////////////////////////////////////////////
/// Class AppFrame
///////////////////////////////////////////////////////////////////////////////
class AppFrame : public wxFrame 
{
	private:
	
	protected:
		wxMenuBar* m_menubar;
		wxMenu* m_mnuFile;
		wxMenu* m_mnuView;
		wxMenu* m_mnuPreferences;
		wxMenu* m_mnuHelp;
		wxSplitterWindow* m_splitter;
		wxPanel* m_panelTop;
		wxChoice* m_choiceModuleLeft;
		wxListCtrl* m_listModulesLeft;
		wxButton* m_btnMove;
		wxButton* m_btnCopy;
		wxButton* m_btnDelete;
		wxButton* m_btnRename;
		wxButton* m_btnDuplicate;
		wxChoice* m_choiceModuleRight;
		wxListCtrl* m_listModulesRight;
		wxPanel* m_panelBottom;
		wxAuiToolBar* m_toolBar;
		wxAuiToolBarItem* m_toolZoomIn; 
		wxAuiToolBarItem* m_toolZoomOut; 
		wxAuiToolBarItem* m_tool3DView; 
		wxAuiToolBarItem* m_toolMeasure; 
		wxAuiToolBarItem* m_toolDetailsPanel; 
		wxAuiToolBarItem* m_toolLeftFootprint; 
		wxAuiToolBarItem* m_toolRightFootprint; 
		wxSplitterWindow* m_splitterViewPanel;
		wxPanel* m_panelView;
		wxScrolledWindow* m_panelSettings;
		wxStaticText* m_lblViewSide;
		wxRadioButton* m_radioViewLeft;
		wxRadioButton* m_radioViewRight;
		wxStaticText* m_lblUnitSelect;
		wxSpinCtrl* m_spinUnitSelect;
		wxStaticText* m_lblDescription;
		wxTextCtrl* m_txtDescription;
		wxStaticText* m_lblAlias;
		wxTextCtrl* m_txtAlias;
		wxStaticText* m_lblFootprintFilter;
		wxTextCtrl* m_txtFootprintFilter;
		wxStaticText* m_lblPadCount;
		wxTextCtrl* m_txtPadCount;
		wxStaticText* m_lblPinNames;
		wxGrid* m_gridPinNames;
		wxStaticText* m_lblPadShape;
		wxChoice* m_choicePadShape;
		wxStaticText* m_lblPadSize;
		wxTextCtrl* m_txtPadWidth;
		wxTextCtrl* m_txtPadLength;
		wxStaticText* m_lblPitch;
		wxTextCtrl* m_txtPitch;
		wxStaticText* m_lblPadSpan;
		wxTextCtrl* m_txtPadSpanX;
		wxTextCtrl* m_txtPadSpanY;
		wxStaticText* m_lblDrillSize;
		wxTextCtrl* m_txtDrillSize;
		wxStaticText* m_lblAuxPad;
		wxTextCtrl* m_txtAuxPadWidth;
		wxTextCtrl* m_txtAuxPadLength;
		wxStaticText* m_lblBodySize;
		wxTextCtrl* m_txtBodyWidth;
		wxTextCtrl* m_txtBodyLength;
		wxStaticText* m_lblRefLabel;
		wxTextCtrl* m_txtRefLabel;
		wxCheckBox* m_chkRefLabelVisible;
		wxStaticText* m_lblValueLabel;
		wxTextCtrl* m_txtValueLabel;
		wxCheckBox* m_chkValueLabelVisible;
		wxStaticText* m_lblShape;
		wxChoice* m_choiceShape;
		wxTextCtrl* m_txtShapeHeight;
		wxButton* m_btnSavePart;
		wxButton* m_btnRevertPart;
		wxStatusBar* m_statusBar;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnCloseApp( wxCloseEvent& event ) { event.Skip(); }
		virtual void OnNewLibrary( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRenameLibrary( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnNewFootprint( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnNewSymbol( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFootprintReport( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSymbolReport( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnQuit( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFootprintMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSymbolMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCompareMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDetailsPanel( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSyncMode( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnFilterToggle( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSearchPaths( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRemoteLink( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnReportSettings( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnTemplateOptions( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnUIOptions( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnHelp( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAbout( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLeftLibSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLeftModSelect( wxListEvent& event ) { event.Skip(); }
		virtual void OnMovePart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCopyPart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDeletePart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRenamePart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDuplicatePart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRightLibSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRightModSelect( wxListEvent& event ) { event.Skip(); }
		virtual void OnZoomIn( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnZoomOut( wxCommandEvent& event ) { event.Skip(); }
		virtual void On3DView( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowMeasurements( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowDetails( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowLeftFootprint( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowRightFootprint( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnDetailsPanelUnsplit( wxSplitterEvent& event ) { event.Skip(); }
		virtual void OnViewCentre( wxMouseEvent& event ) { event.Skip(); }
		virtual void OnViewStartDrag( wxMouseEvent& event ) { event.Skip(); }
		virtual void OnViewDrag( wxMouseEvent& event ) { event.Skip(); }
		virtual void OnPaintViewport( wxPaintEvent& event ) { event.Skip(); }
		virtual void OnSizeViewport( wxSizeEvent& event ) { event.Skip(); }
		virtual void OnShowLeftDetails( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnShowRightDetails( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnUnitSelect( wxSpinEvent& event ) { event.Skip(); }
		virtual void OnKillFocusTextInfo( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnTextFieldChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnEnterTextInfo( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusPadCount( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterPadCount( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnPinNameChanged( wxGridEvent& event ) { event.Skip(); }
		virtual void OnPinRightClick( wxGridEvent& event ) { event.Skip(); }
		virtual void OnPinNameRearrange( wxKeyEvent& event ) { event.Skip(); }
		virtual void OnChoiceFieldChange( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusPadInfo( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterPadInfo( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusPitchInfo( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterPitchInfo( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusSpanInfo( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterSpanInfo( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusBodyInfo( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterBodyInfo( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusLabelField( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterLabelField( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnLabelShowHide( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnKillFocusShapeHeight( wxFocusEvent& event ) { event.Skip(); }
		virtual void OnEnterShapeHeight( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSavePart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRevertPart( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnStatusBarDblClk( wxMouseEvent& event ) { event.Skip(); }
		
	
	public:
		
		AppFrame( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("KiCad Librarian"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 700,450 ), long style = wxDEFAULT_FRAME_STYLE|wxRESIZE_BORDER|wxTAB_TRAVERSAL );
		
		~AppFrame();
		
		void m_splitterOnIdle( wxIdleEvent& )
		{
			m_splitter->SetSashPosition( 185 );
			m_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( AppFrame::m_splitterOnIdle ), NULL, this );
		}
		
		void m_splitterViewPanelOnIdle( wxIdleEvent& )
		{
			m_splitterViewPanel->SetSashPosition( -200 );
			m_splitterViewPanel->Disconnect( wxEVT_IDLE, wxIdleEventHandler( AppFrame::m_splitterViewPanelOnIdle ), NULL, this );
		}
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgPaths
///////////////////////////////////////////////////////////////////////////////
class DlgPaths : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblFootprintPaths;
		wxListBox* m_lstFootprints;
		wxButton* m_btnAddFootprint;
		wxButton* m_btnRemoveFootprint;
		wxStaticText* m_lblSchematicPath;
		wxListBox* m_lstSymbols;
		wxButton* m_btnAddSymbol;
		wxButton* m_btnRemoveSymbol;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnFootprintPathSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAddFootprintPath( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRemoveFootprintPath( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnSymbolPathSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnAddSymbolPath( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnRemoveSymbolPath( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgPaths( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Search paths"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgPaths();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgReport
///////////////////////////////////////////////////////////////////////////////
class DlgReport : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblPageSize;
		wxChoice* m_choicePageSize;
		wxRadioBox* m_radioLayout;
		wxStaticText* m_lblFontSize;
		wxSpinCtrl* m_spinFontSize;
		wxStaticText* m_lblFontUnit;
		wxCheckBox* m_chkDescription;
		wxCheckBox* m_chkValueLabels;
		wxCheckBox* m_chkPadInfo;
		wxCheckBox* m_chkFPList;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgReport( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Report options"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgReport();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgOptions
///////////////////////////////////////////////////////////////////////////////
class DlgOptions : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblFontSize;
		wxSpinCtrl* m_spinFontSize;
		wxStaticText* m_lblFontUnit;
		wxStaticText* m_lblDimOffset;
		wxSpinCtrl* m_spinDimOffset;
		wxStaticText* m_lblDimUnit;
		wxCheckBox* m_chkDrawLabels;
		wxCheckBox* m_chkDrawCentreCross;
		wxCheckBox* m_chkFullPaths;
		wxStaticLine* m_staticline1;
		wxStaticText* m_lblClrFootprint;
		wxColourPickerCtrl* m_colourFootprint;
		wxStaticText* m_lblClrSymbol;
		wxColourPickerCtrl* m_colourSchematic;
		wxStaticText* m_lblClr3DModel;
		wxColourPickerCtrl* m_colour3DModel;
		wxStaticLine* m_staticline11;
		wxCheckBox* m_chkCopyVRML;
		wxCheckBox* m_chkDisableTemplates;
		wxStaticLine* m_staticline2;
		wxCheckBox* m_chkConfirmOverwrite;
		wxCheckBox* m_chkConfirmDelete;
		wxStaticLine* m_staticline4;
		wxCheckBox* m_chkReloadSession;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgOptions( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("User Interface"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgOptions();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgTemplateOpts
///////////////////////////////////////////////////////////////////////////////
class DlgTemplateOpts : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblVariables;
		wxGrid* m_gridTemplateVars;
		wxTextCtrl* m_txtInfo;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgTemplateOpts( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Template variables"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgTemplateOpts();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgNewFootprint
///////////////////////////////////////////////////////////////////////////////
class DlgNewFootprint : public wxDialog 
{
	private:
	
	protected:
		wxListBox* m_lstTemplates;
		wxStaticText* m_lblName;
		wxTextCtrl* m_txtName;
		wxStaticBitmap* m_bmpExample;
		wxSpinButton* m_spinPreview;
		wxTextCtrl* m_txtDescription;
		wxStdDialogButtonSizer* m_sdbSizerOkCancel;
		wxButton* m_sdbSizerOkCancelOK;
		wxButton* m_sdbSizerOkCancelCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnTemplateSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnNextImage( wxSpinEvent& event ) { event.Skip(); }
		virtual void OnPrevImage( wxSpinEvent& event ) { event.Skip(); }
		virtual void OnOk( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgNewFootprint( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("New footprint"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgNewFootprint();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgRemoteLink
///////////////////////////////////////////////////////////////////////////////
class DlgRemoteLink : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblURL;
		wxTextCtrl* m_txtURL;
		wxStaticText* m_lblUserName;
		wxTextCtrl* m_txtUserName;
		wxStaticText* m_lblPassword;
		wxTextCtrl* m_txtPassword;
		wxStaticText* m_lblAuthentication;
		wxStaticText* m_lblAuthUser;
		wxTextCtrl* m_txtAuthUser;
		wxStaticText* m_lblAuthPwd;
		wxTextCtrl* m_txtAuthPWD;
		wxCheckBox* m_checkVerifyPeer;
		wxCheckBox* m_checkVerifyHost;
		wxStaticText* m_lblHelp;
		wxButton* m_btnSignUp;
		wxButton* m_btnOK;
		wxButton* m_btnCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnSignUp( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnCancel( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgRemoteLink( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Repository"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgRemoteLink();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgRemoteSignUp
///////////////////////////////////////////////////////////////////////////////
class DlgRemoteSignUp : public wxDialog 
{
	private:
	
	protected:
		wxStaticText* m_lblURL;
		wxTextCtrl* m_txtURL;
		wxStaticText* m_lblUserName;
		wxTextCtrl* m_txtUserName;
		wxStaticText* m_lbEmail;
		wxTextCtrl* m_txtEmail;
		wxStaticText* m_lblAuthentication;
		wxStaticText* m_lblAuthUser;
		wxTextCtrl* m_txtAuthUser;
		wxStaticText* m_lblAuthPwd;
		wxTextCtrl* m_txtAuthPWD;
		wxCheckBox* m_checkVerifyPeer;
		wxCheckBox* m_checkVerifyHost;
		wxStaticText* m_lblHelp;
		wxStdDialogButtonSizer* m_sdbSizer;
		wxButton* m_sdbSizerOK;
		wxButton* m_sdbSizerCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnOK( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgRemoteSignUp( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("Repository - sign up"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgRemoteSignUp();
	
};

///////////////////////////////////////////////////////////////////////////////
/// Class DlgNewSymbol
///////////////////////////////////////////////////////////////////////////////
class DlgNewSymbol : public wxDialog 
{
	private:
	
	protected:
		wxListBox* m_lstTemplates;
		wxStaticText* m_lblName;
		wxTextCtrl* m_txtName;
		wxStaticText* m_lblPrefix;
		wxTextCtrl* m_txtPrefix;
		wxStaticBitmap* m_bmpExample;
		wxTextCtrl* m_txtDescription;
		wxStdDialogButtonSizer* m_sdbSizerOkCancel;
		wxButton* m_sdbSizerOkCancelOK;
		wxButton* m_sdbSizerOkCancelCancel;
		
		// Virtual event handlers, overide them in your derived class
		virtual void OnTemplateSelect( wxCommandEvent& event ) { event.Skip(); }
		virtual void OnOk( wxCommandEvent& event ) { event.Skip(); }
		
	
	public:
		
		DlgNewSymbol( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("New symbol"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( -1,-1 ), long style = wxDEFAULT_DIALOG_STYLE|wxRESIZE_BORDER ); 
		~DlgNewSymbol();
	
};

#endif //__LIBMNGR_GUI_BASE_H__
