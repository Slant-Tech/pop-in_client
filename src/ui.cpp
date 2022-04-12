///////////////////////////////////////////////////////////////////////////
// C++ code generated with wxFormBuilder (version 3.10.1)
// http://www.wxformbuilder.org/
//
// PLEASE DO *NOT* EDIT THIS FILE!
///////////////////////////////////////////////////////////////////////////

#include <ui.h>
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
	#include <wx/wx.h>
#endif

class client_ui: public wxApp{

	public:
		virtual bool OnInit();
};

bool client_ui::OnInit(){
	main_window *main = new main_window( NULL, NULL, "POP Client", wxPoint(50, 50), wxSize(800, 600), wxDEFAULT_FRAME_STYLE);
	main->Show(true);
	return true;
}

wxIMPLEMENT_APP(client_ui);


///////////////////////////////////////////////////////////////////////////

main_window::main_window( wxWindow* parent, wxWindowID id, const wxString& title, const wxPoint& pos, const wxSize& size, long style ) : wxFrame( parent, id, title, pos, size, style )
{
	this->SetSizeHints( wxDefaultSize, wxDefaultSize );

	main_menubar = new wxMenuBar( 0 );
	menu_file = new wxMenu();
	wxMenuItem* menu_file_quit;
	menu_file_quit = new wxMenuItem( menu_file, wxID_ANY, wxString( wxT("Quit") ) , wxEmptyString, wxITEM_NORMAL );
	menu_file->Append( menu_file_quit );

	main_menubar->Append( menu_file, wxT("File") );

	menu_edit = new wxMenu();
	main_menubar->Append( menu_edit, wxT("Edit") );

	menu_project = new wxMenu();
	main_menubar->Append( menu_project, wxT("Project") );

	menu_bom = new wxMenu();
	main_menubar->Append( menu_bom, wxT("BOM") );

	menu_network = new wxMenu();
	main_menubar->Append( menu_network, wxT("Network") );

	menu_help = new wxMenu();
	main_menubar->Append( menu_help, wxT("Help") );

	this->SetMenuBar( main_menubar );

	wxBoxSizer* main_sizer;
	main_sizer = new wxBoxSizer( wxHORIZONTAL );

	split_view = new wxSplitterWindow( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D );
	split_view->Connect( wxEVT_IDLE, wxIdleEventHandler( main_window::split_viewOnIdle ), NULL, this );
	split_view->SetMinimumPaneSize( 200 );

	split_view->SetMinSize( wxSize( 10,-1 ) );

	project_panel = new wxPanel( split_view, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* project_sizer;
	project_sizer = new wxBoxSizer( wxVERTICAL );

	project_label = new wxStaticText( project_panel, wxID_ANY, wxT("Projects"), wxDefaultPosition, wxDefaultSize, 0 );
	project_label->Wrap( -1 );
	project_sizer->Add( project_label, 0, wxALL, 5 );

	m_searchCtrl4 = new wxSearchCtrl( project_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0 );
	#ifndef __WXMAC__
	m_searchCtrl4->ShowSearchButton( true );
	#endif
	m_searchCtrl4->ShowCancelButton( false );
	project_sizer->Add( m_searchCtrl4, 0, wxALL, 5 );

	m_scrolledWindow5 = new wxScrolledWindow( project_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	m_scrolledWindow5->SetScrollRate( 5, 5 );
	wxBoxSizer* project_view_scroll;
	project_view_scroll = new wxBoxSizer( wxVERTICAL );

	project_tree_view = new wxTreeListCtrl( m_scrolledWindow5, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_DEFAULT_STYLE );
	project_tree_view->AppendColumn( wxT("project_tree_column"), wxCOL_WIDTH_DEFAULT, wxALIGN_LEFT, wxCOL_RESIZABLE );

	project_view_scroll->Add( project_tree_view, 1, wxEXPAND | wxALL, 5 );


	m_scrolledWindow5->SetSizer( project_view_scroll );
	m_scrolledWindow5->Layout();
	project_view_scroll->Fit( m_scrolledWindow5 );
	project_sizer->Add( m_scrolledWindow5, 1, wxEXPAND | wxALL, 5 );


	project_panel->SetSizer( project_sizer );
	project_panel->Layout();
	project_sizer->Fit( project_panel );
	info_panel = new wxPanel( split_view, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* info_sizer;
	info_sizer = new wxBoxSizer( wxVERTICAL );

	info_splitter = new wxSplitterWindow( info_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_3D );
	info_splitter->SetSashSize( 10 );
	info_splitter->Connect( wxEVT_IDLE, wxIdleEventHandler( main_window::info_splitterOnIdle ), NULL, this );

	info_splitter->SetMinSize( wxSize( -1,10 ) );

	info_header_panel = new wxPanel( info_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	info_tab_panel = new wxPanel( info_splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL );
	wxBoxSizer* info_tab_sizer;
	info_tab_sizer = new wxBoxSizer( wxVERTICAL );

	info_notebook = new wxNotebook( info_tab_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0 );
	info_tab_overview = new wxScrolledWindow( info_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	info_tab_overview->SetScrollRate( 5, 5 );
	info_notebook->AddPage( info_tab_overview, wxT("Overview"), false );
	info_tab_bom = new wxScrolledWindow( info_notebook, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL|wxVSCROLL );
	info_tab_bom->SetScrollRate( 5, 5 );
	info_notebook->AddPage( info_tab_bom, wxT("BOM"), true );

	info_tab_sizer->Add( info_notebook, 1, wxEXPAND | wxALL, 5 );


	info_tab_panel->SetSizer( info_tab_sizer );
	info_tab_panel->Layout();
	info_tab_sizer->Fit( info_tab_panel );
	info_splitter->SplitHorizontally( info_header_panel, info_tab_panel, 80 );
	info_sizer->Add( info_splitter, 1, wxEXPAND, 5 );


	info_panel->SetSizer( info_sizer );
	info_panel->Layout();
	info_sizer->Fit( info_panel );
	split_view->SplitVertically( project_panel, info_panel, 220 );
	main_sizer->Add( split_view, 1, wxEXPAND, 5 );


	this->SetSizer( main_sizer );
	this->Layout();

	this->Centre( wxBOTH );
}

main_window::~main_window()
{
}
