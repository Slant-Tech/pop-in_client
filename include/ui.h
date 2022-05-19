///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#pragma once

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
#include <wx/stattext.h>
#include <wx/srchctrl.h>
#include <wx/treelist.h>
#include <wx/sizer.h>
#include <wx/scrolwin.h>
#include <wx/panel.h>
#include <wx/notebook.h>
#include <wx/splitter.h>
#include <wx/frame.h>

///////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
/// Class main_window
///////////////////////////////////////////////////////////////////////////////
class main_window : public wxFrame
{
	private:

	protected:
		wxMenuBar* main_menubar;
		wxMenu* menu_file;
		wxMenu* menu_edit;
		wxMenu* menu_project;
		wxMenu* menu_bom;
		wxMenu* menu_network;
		wxMenu* menu_help;
		wxSplitterWindow* split_view;
		wxPanel* project_panel;
		wxStaticText* project_label;
		wxSearchCtrl* m_searchCtrl4;
		wxScrolledWindow* m_scrolledWindow5;
		wxTreeListCtrl* project_tree_view;
		wxPanel* info_panel;
		wxSplitterWindow* info_splitter;
		wxPanel* info_header_panel;
		wxPanel* info_tab_panel;
		wxNotebook* info_notebook;
		wxScrolledWindow* info_tab_overview;
		wxScrolledWindow* info_tab_bom;

	public:

		main_window( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxEmptyString, const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 600,400 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL );

		~main_window();

		void split_viewOnIdle( wxIdleEvent& )
		{
			split_view->SetSashPosition( 220 );
			split_view->Disconnect( wxEVT_IDLE, wxIdleEventHandler( main_window::split_viewOnIdle ), NULL, this );
		}

		void info_splitterOnIdle( wxIdleEvent& )
		{
			info_splitter->SetSashPosition( 80 );
			info_splitter->Disconnect( wxEVT_IDLE, wxIdleEventHandler( main_window::info_splitterOnIdle ), NULL, this );
		}

};

