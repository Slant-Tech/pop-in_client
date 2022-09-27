#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <iterator>
#include <GLFW/glfw3.h>
//#include <projectview.h>
#include <yder.h>
#include <db_handle.h>
#include <L2DFileDialog.h>

static struct proj_t* selected_prj = 0; /* index that has been selected */


///* BOM Line item structure */
//struct BomLineItem {
//	int int_pn; 	/* internal part number */
//	int index;	/* Bom item index */
//	char* mpn;	/* Manufacturer Part Number */
//	char* mfg;	/* Manufacturer */
//	unsigned int q;		/* Quantity */
//	unsigned int type;	/* Part type */
//};

/* BOM Structure */
//struct ProjectBom {
//	int id;					/* BOM id number */
//	struct part_t*  item;	/* BOM Items */
//};

static void glfw_error_callback(int error, const char* description){
	y_log_message(Y_LOG_LEVEL_ERROR, "GLFW Error %d: %s", error, description);
}

/* Pass structure of projects and number of projects inside of struct */
static void show_project_select_window( struct proj_t** projects);
static void show_new_part_popup( void );
static void show_root_window( struct proj_t** projects, struct bom_t** boms );
static void import_parts_window( void );
void DisplayNode( struct proj_t* node );

bool show_new_part_window = false;
bool show_import_parts_window = false;

#define DEFAULT_ROOT_W	1280
#define DEFAULT_ROOT_H	720

/* Variable to continue running */
static bool run_flag = true;

int main( int, char** ){

	/* Projects */
	static proj_t* projects[4] = {};
#if 0
	static ProjectNode projects[] = {
		/* Name						Date			Version	 Author		BOM ID	Child Index		Child Count  	Selected */
		{ "Project Top\0", 			"2022-05-01\0", 	"1.2.3\0", "Dylan\0",	0,		1,				2,				true},
		{ "Subproject 1\0", 		"2022-05-02\0", 	"2.3.4\0", "Dylan\0",	1,		3,				1,				false},
		{ "Subproject 2\0", 		"2022-05-03\0", 	"3.4.5\0", "Dylan\0",	2,		1,				-1,				false},
		{ "subsubproject 1\0",		"2022-05-04\0", 	"4.5.6\0", "Dylan\0",	3,		-1,				-1,				false}
	};
#endif
	/* BOMs */	
	static bom_t* boms[4] = {};
#if 0
	static ProjectBom boms[] = {
		/* BOM ID	BOM Line Items	*/
		{ 0, 			{	
								/* Internal Part Number		BOM Index	MPN						MFG								Quantity	Type */
								{	1000,					0,			"CL10B104KB8NNNC\0",	"Samsung Electro-Mechanics\0",	5000,		1},
								{	2001,					1,			"RC0603FR-071KL\0",		"Yageo\0",						1000,		2},
								{	2002,					2,			"RC0603FR-0710K\0",		"Yageo\0",						250,		2},
								{	2006,					3,			"RC0603FR-0720K\0",		"Yageo\0",						250,		2},
						}		                                                       
		},
		{ 1, 			{	
								/* Internal Part Number		BOM Index	MPN						MFG								Quantity	Type */
								{	1000,					0,			"CL10B104KB8NNNC\0",	"Samsung Electro-Mechanics\0",	5000,		1},
								{	2002,					1,			"RC0603FR-0710KL\0",	"Yageo\0",						250,		2},
								{	2001,					2,			"RC0603FR-071KL\0",		"Yageo\0",						1000,		2},
						}		
		},
		{ 2, 			{	
								/* Internal Part Number		BOM Index	MPN						MFG								Quantity	Type */
								{	2001,					0,			"RC0603FR-071KL\0",		"Yageo\0",						1000,		2},
								{	2002,					1,			"RC0603FR-0710KL\0",	"Yageo\0",						250,		2},
								{	2003,					2,			"RC0603FR-074K7L\0",	"Yageo\0",						500,		2},
						}		
		},
		{ 3, 			{	
								/* Internal Part Number		BOM Index	MPN						MFG								Quantity	Type */
								{	1000,					0,			"CL10B104KB8NNNC\0",	"Samsung Electro-Mechanics\0",	5000,		1},
								{	2002,					1,			"RC0603FR-0710KL\0",	"Yageo\0",						250,		2},
						}		
		}

	};
#endif

	/* Setup window */
	glfwSetErrorCallback(glfw_error_callback);
	if( !glfwInit() ){
		return 1;
	}

	/* Determine GL+GLSL versions. For now, just force GL3.3 */
	const char* glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	/* Create window with graphics content */
	GLFWwindow* window = glfwCreateWindow(DEFAULT_ROOT_W, DEFAULT_ROOT_H, "POP:In", NULL, NULL);
	if( window == NULL ){
		return 1;
	}

	glfwMakeContextCurrent(window);

	/* Enable vsync */
	glfwSwapInterval(1);

	/* Setup ImGui Context */
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
//	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	/* Set ImGui color style */
	ImGui::StyleColorsLight();
	/* Dark colors */
//	ImGui::StyleColorsDark();
	/* Classic colors */
	//ImGui::StyleColorsClassic();
	
	/* Setup Backends */
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	/* Load fonts if possible  */
	ImFont* font_hack = io.Fonts->AddFontFromFileTTF( "/usr/share/fonts/TTF/Hack-Regular.ttf", 14.0f);
	IM_ASSERT( font_hack != NULL );
	if( font_hack == NULL ){
		io.Fonts->AddFontDefault();
	}

	/* Setup colors */
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	/* Main viewport */
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();


	/* Getting display width and height */
	int display_w = DEFAULT_ROOT_W, display_h = DEFAULT_ROOT_H;

	bool bool_root_window = true;
	ImGuiWindowFlags root_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
									   | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;

	if( redis_connect( NULL, 0 ) ){ /* Use defaults of localhost and default port */
		/* Failed to init database connection, so quit */
		run_flag = 0;
	}
	
#if 0
	/* Get a part to see if it works */
	struct part_t* test_part = get_part_from_pn("part:GRM188R61E106KA73D");

	/* Print out part number, mfg, internal part number */
	printf("Redis database part:\n pn:\t%s, mfg:\t%s, internal#:\t%d \n", test_part->mpn, test_part->mfg, test_part->ipn);
	printf("Info:\n");
	for( unsigned int i = 0; i < test_part->info_len; i++ ){
		printf("\t %s:\t%s\n", test_part->info[i].key, test_part->info[i].val );
	}


	/* Done with part, free it */
	free_part_t( test_part );
#endif

	/* Get projects from database; has to start from 1 */
	for( int i = 1; i <= 4; i++ ){
		projects[i-1] = get_proj_from_ipn(i);
		if( NULL == projects[i-1] ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not get index project" );
		}
		else{
			y_log_message( Y_LOG_LEVEL_DEBUG, "Read project %d from database", i);
		}
	}

	/* Use first project as first selected node */
	selected_prj = projects[0];

	/* Main application loop */
	while( !glfwWindowShouldClose(window) ) {

		/* Poll and handle events */
		glfwPollEvents();

		if( !run_flag ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "Quit button pressed");
			glfwSetWindowShouldClose(window, true);
		}
		/* Start ImGui frame */
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		/* Setup root window. Keep size dynamic to maximum area */
		ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y));
		ImGui::SetNextWindowSize(ImVec2(display_w, display_h));
		ImGui::Begin("Root", &bool_root_window, root_window_flags);
		if( show_new_part_window ){
			show_new_part_popup();
		}
		if( show_import_parts_window ){
			import_parts_window();
		}
		show_root_window(projects, boms);
		ImGui::End();

		/* End Projects view creation */


		/* Rendering section */
		ImGui::Render();
		glfwGetFramebufferSize( window, &display_w, &display_h);
		glViewport(0, 0, display_w, display_h);
		glClearColor( clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear( GL_COLOR_BUFFER_BIT );
		ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData());

		glfwSwapBuffers(window);

	}

	/* Disconnect from database */
	redis_disconnect();

	/* Cleanup projects */
	for( int i = 0; i < 4; i++ ){
		free_proj_t(projects[i]);
	}

	/* Application cleanup */
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();


}



static void show_project_select_window( struct proj_t **projects ){
	
	int open_action = -1;
	ImGui::Text("Projects");

	const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;

	if( open_action != -1 ){
		ImGui::SetNextItemOpen(open_action != 0);
	}

	/* Flags for Table */
	static ImGuiTableFlags flags = ImGuiTableFlags_BordersV | \
								   ImGuiTableFlags_BordersOuterH | \
								   ImGuiTableFlags_Resizable | \
								   ImGuiTableFlags_RowBg | \
								   ImGuiTableFlags_NoBordersInBody | \
								   ImGuiTreeNodeFlags_OpenOnArrow | \
								   ImGuiTreeNodeFlags_OpenOnDoubleClick | \
								   ImGuiTreeNodeFlags_SpanFullWidth;

	/* Set colums, items in table */
	if( ImGui::BeginTable("projects", 4, flags)){
		/* First column will use default _WidthStretch when ScrollX is
		 * off and _WidthFixed when ScrollX is on */
		ImGui::TableSetupColumn("Name",   	ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Date",   	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Version",	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Author", 	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();


//		projects[0].ProjectNode::DisplayNode( &projects[0], projects );
		for( int i = 0; i < 4; i++ ){
			DisplayNode( projects[i] );
		}
		ImGui::EndTable();
	}

}

static void show_new_part_popup( void ){
	static struct part_t part;

	/* Buffers for text input. Can also be used for santizing inputs */
	static char quantity[128] = {};
	static char internal_pn[256] = {};
	static char type[256] = {};
	static char mfg[512] = {};
	static char mpn[512] = {};
	static int selection_idx = -1;
	const char* status_options[3] = {
			"Production",
			"NRND",
			"Obsolete"
	};
	static const char* selection_str = NULL;

	if( selection_idx >= 0 && selection_idx < 3 ){
		selection_str = status_options[selection_idx];
	}

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("New Part Popup",&show_new_part_window, flags ) ){
		ImGui::Text("Enter part details below");
		ImGui::Separator();

		/* Text entry fields */
		ImGui::InputText("Internal Part Number", internal_pn, 255, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsDecimal );
		ImGui::InputText("Part Type", type, 255, ImGuiInputTextFlags_CharsNoBlank);
		ImGui::InputText("Part Number", mpn, 511, ImGuiInputTextFlags_CharsNoBlank);	
		ImGui::InputText("Manufacturer", mfg, 511);
		if( ImGui::BeginCombo("new-part-status", selection_str, 0 ) ){
			for( int i = 0; i < IM_ARRAYSIZE( status_options ); i++){
				const bool is_selected = ( selection_idx == i );
				if( ImGui::Selectable(status_options[i], is_selected ) ){
					selection_idx = i;
				}
				if( is_selected ){

					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		ImGui::InputText("Local Stock", quantity, 127, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsDecimal );

		/* Check if valid to copy */

		/* No checks because I'll totally do it later (hopefully) */
		part.q = atoi( quantity );
		part.ipn = atoi( internal_pn ); 
		part.mpn = mpn;
		part.mfg = mfg;
		part.type = type;
		if( 0 == selection_idx ){
			part.status = pstat_prod;
		}
		else if( 1 == selection_idx ){
			part.status = pstat_nrnd;
		}
		else if( 2 == selection_idx ){
			part.status = pstat_obsolete;
		}
		else {
			part.status = pstat_unknown;
		}

		/* Figure out what the hell to do with the info section */

		/* Save and cancel buttons */
		if( ImGui::Button("Save", ImVec2(0,0)) ){

			/* Check if can save first */

			/* Perform the write */
			redis_write_part( &part );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Data written to database");
			show_new_part_window = false;
			
			/* Clear all input data */
			part.q = 0;
			part.ipn = 0;
			part.type = NULL;
			part.mpn = NULL;
			part.mfg = NULL;
	
			memset( quantity, 0, 127);
			memset( internal_pn, 0, 255);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, finished writing");
//			show_new_part_window = false;

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			show_new_part_window = false;
			/* Clear all input data */
			part.q = 0;
			part.ipn = 0;
			part.type = NULL;
			part.mpn = NULL;
			part.mfg = NULL;
			memset( quantity, 0, 127);
			memset( internal_pn, 0, 255);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, exiting window");
		}

		ImGui::End();
	}

}

static void import_parts_window( void ){
	static char filepath[(uint16_t)-1] = {}; /* Need to make sure this is large enough for some insane paths */
	static char* file_dialog_buffer = nullptr;	

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("Import Parts", &show_import_parts_window, flags) ){
		
		ImGui::Text("Select parts file to import");	
		ImGui::Separator();

		ImGui::InputText("##import_filepath", filepath, sizeof( filepath ) );
		ImGui::SameLine();

		/* Button to open filepicker dialog */
		if( ImGui::Button("Browse##import_filepath") ){
			file_dialog_buffer = filepath;
			FileDialog::file_dialog_open = true; /* Need this to start the dialog */
			FileDialog::file_dialog_open_type = FileDialog::FileDialogType::OpenFile;
		}

		/* Actual Dialog */
		if( FileDialog::file_dialog_open ){
			FileDialog::ShowFileDialog( &FileDialog::file_dialog_open, file_dialog_buffer, sizeof( file_dialog_buffer ), FileDialog::FileDialogType::OpenFile);
		}

		/* Button to start import */
		if( ImGui::Button("Import") ){
			/* Start importing */
			y_log_message(Y_LOG_LEVEL_DEBUG, "Path received: %s", filepath);

			redis_import_part_file( filepath );

			/* Cleanup path */
			memset( filepath, 0, sizeof( filepath ) );
			/* End of window */
			show_import_parts_window = false;
		}
		ImGui::SameLine();

		/* Button to cancel operation */
		if( ImGui::Button("Cancel") ){
			/* Cleanup and exit window */
			/* End of window */
			show_import_parts_window = false;
		}

		ImGui::End();

	}

}

/* Menu items */
static void show_menu_bar( void ){

	if( ImGui::BeginMenuBar() ){
		/* File Menu */
		if( ImGui::BeginMenu("File") ) {
			if( ImGui::MenuItem("New Project") ){
				
			}
			else if( ImGui::MenuItem("New Part") ){
				/* Create part_t, then provide result to redis_write_part */
				y_log_message(Y_LOG_LEVEL_DEBUG, "New Part menu clicked");
				show_new_part_window = true;
			}
			else if( ImGui::MenuItem("Import Parts") ){
				/* Read file from selection and add to database from selection */
				y_log_message(Y_LOG_LEVEL_DEBUG, "Import Parts menu clicked");
				show_import_parts_window = true;
			}
			else if( ImGui::MenuItem("Export Project") ){
				
			}
			else if( ImGui::MenuItem("Quit") ){
				run_flag = false;		
			}
			ImGui::EndMenu();
		}

		/* Edit Menu */
		if( ImGui::BeginMenu("Edit") ){
			if( ImGui::MenuItem("Edit Project") ){
			
			}
			else if( ImGui::MenuItem("Edit Part") ){

			}
			else if( ImGui::MenuItem("Application Settings")){

			}
			ImGui::EndMenu();
		}

		/* Project Menu */
		if( ImGui::BeginMenu("Project") ){
			if( ImGui::MenuItem("Generate Report") ){
				
			}
			else if( ImGui::MenuItem("Manage Project BOM")){

			}
			else if( ImGui::MenuItem("Project Relations")){

			}
			ImGui::EndMenu();
		}

		/* BOM Menu */
		if( ImGui::BeginMenu("BOM") ){
			if( ImGui::MenuItem("Export BOM") ){
				
			}
			else if( ImGui::MenuItem("Import BOM")){

			}
			else if( ImGui::MenuItem("Export Part Library")){

			}		
			else if( ImGui::MenuItem("Import Part Library")){

			}		
			ImGui::EndMenu();
		}

		/* Network Menu */
		if( ImGui::BeginMenu("Network") ){
			if( ImGui::MenuItem("Connect") ){
				
			}
			else if( ImGui::MenuItem("Server Settings")){

			}
			else if( ImGui::MenuItem("Server Status")){

			}		
			ImGui::EndMenu();
		}

		/* Help Menu */
		if( ImGui::BeginMenu("Help") ){
			if( ImGui::MenuItem("About") ){
				
			}
			else if( ImGui::MenuItem("Documentation")){

			}
			else if( ImGui::MenuItem("Contact Support")){

			}		
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}



}

static void show_bom_window( bom_t* bom ){

	/* Show BOM/Information View */
	ImGuiTabBarFlags tabbar_flags = ImGuiTabBarFlags_None;
	if( ImGui::BeginTabBar("Project Info", tabbar_flags ) ){
		if( ImGui::BeginTabItem("Info") ){
			ImGui::Text("Project Information Tab");
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem("BOM") ){
			ImGui::Text("Project BOM Tab");

			/* BOM Specific table view */

			static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp | \
												 ImGuiTableFlags_Resizable | \
												 ImGuiTableFlags_NoSavedSettings | \
												 ImGuiTableFlags_BordersOuter | \
												 ImGuiTableFlags_Sortable | \
												 ImGuiTableFlags_SortMulti | \
												 ImGuiTableFlags_ScrollY | \
												 ImGuiTableFlags_BordersV | \
												 ImGuiTableFlags_Reorderable | \
												 ImGuiTableFlags_ContextMenuInBody;

			if( ImGui::BeginTable("BOM", 5, table_flags ) ){
				ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_DefaultSort);
				ImGui::TableSetupColumn("P/N", ImGuiTableColumnFlags_PreferSortAscending);
				ImGui::TableSetupColumn("Manufacturer", ImGuiTableColumnFlags_PreferSortAscending);
				ImGui::TableSetupColumn("Quantitiy", ImGuiTableColumnFlags_PreferSortAscending);
				ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_PreferSortAscending);

				ImGui::TableHeadersRow();

#if 0 
				/* Sort the bom items list based off of what is currently
				 * selected for sorting */
				if(ImGuiTableSortSpecs* bom_sort_specs = ImGui::TableGetSortSpecs()){
					/* Criteria changed; need to resort */
					if( bom_sort_specs->SpecsDirty ){
						if( bom.item.size() > 1 ){
							//qsort( &bom.item[0], (size_t)bom.item.size(), sizeof( bom.item[0] ) );	
						}
						bom_sort_specs->SpecsDirty = false;
					}
				}
#endif
				
				/* Used for selecting specific item in BOM */
				bool item_sel[ bom->nitems ] = {};
				char line_item_label[64]; /* May need to change size at some point */
				static part_t *selected_item = NULL;	
				for( int i = 0; i < bom->nitems; i++){
					ImGui::TableNextRow();

					/* BOM Line item */
					ImGui::TableSetColumnIndex(0);
					/* Selectable line item number */
					snprintf(line_item_label, 64, "%d", i);
					ImGui::Selectable(line_item_label, &item_sel[i], ImGuiSelectableFlags_SpanAllColumns);

					/* Part number */
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%s", bom->parts[i]->mpn );

					/* Manufacturer */
					ImGui::TableSetColumnIndex(2);
					ImGui::Text("%s", bom->parts[i]->mfg );

					/* Quantity */
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%d", bom->line[i].q );

					/* Type */
					ImGui::TableSetColumnIndex(4);
					ImGui::Text("%s", bom->parts[i]->type);

					if( item_sel[i] ){
						/* Open popup for part info */
						ImGui::OpenPopup("PartInfo");
						y_log_message(Y_LOG_LEVEL_DEBUG, "%s was selected", bom->parts[i]->mpn);
						selected_item = bom->parts[i];
					}

				}
				/* Popup window for Part info */
				ImVec2 center = ImGui::GetMainViewport()->GetCenter();
				ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
				if( ImGui::BeginPopupModal("PartInfo", NULL, ImGuiWindowFlags_AlwaysAutoResize) ){
					ImGui::Text("Part Info");
					ImGui::Separator();
					
					if( NULL != selected_item ){
						ImGui::Text("Part Number: %s", selected_item->mpn);
						ImGui::Text("Price Breaks:");
						for( unsigned int i = 0; i <  selected_item->price_len; i++ ){
							ImGui::Text("%ld:\t$%0.6lf", selected_item->price[i].quantity, selected_item->price[i].price );	
						}
					}
					else{
						ImGui::Text("Part Number not found");
					}

					ImGui::Separator();
			
					if( ImGui::Button("OK", ImVec2(120,0))){
						ImGui::CloseCurrentPopup();
					}
					ImGui::SetItemDefaultFocus();
					
					ImGui::EndPopup();
				}

				ImGui::EndTable();
			}

			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}



}

/* Setup root window, child windows */
static void show_root_window( struct proj_t** projects, struct bom_t** boms ){

	/* Create menu items */
	show_menu_bar();
	
	/* Put the different items into columns*/
	static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp | \
										 ImGuiTableFlags_Resizable | \
										 ImGuiTableFlags_BordersOuter | \
										 ImGuiTableFlags_BordersV | \
										 ImGuiTableFlags_Reorderable | \
										 ImGuiTableFlags_ContextMenuInBody;

	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		/* Project view on the left */
		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Project Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		show_project_select_window(projects);
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Project Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected project */
		if( NULL != selected_prj ){
			show_bom_window( selected_prj->boms[0].bom );
		} else {
			/* No projects selected, use empty bom */
//			show_bom_window(empty_default_bom);
		}
		ImGui::EndChild();
		
		ImGui::EndTable();	
	}

}

void DisplayNode( struct proj_t* node ){

	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
									ImGuiTreeNodeFlags_OpenOnDoubleClick | \
									ImGuiTreeNodeFlags_SpanFullWidth; 

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	
	/* Check if project contains subprojects */
	if( node->nsub > 0 ){

		/* Check if selected, display if selected */
		if( node->selected && (node == selected_prj ) ){
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}
		else {
			node_flags &= ~ImGuiTreeNodeFlags_Selected;		
		}

		bool open = ImGui::TreeNodeEx(node->name, node_flags);
		
		/* Check if item has been clicked */
		if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
			selected_prj = node;
			node->selected = true;
		}


		ImGui::TableNextColumn();
		ImGui::Text( asctime( localtime(&node->time_created)) );
		ImGui::TableNextColumn();
		ImGui::Text(node->ver);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(node->author);
		
		/* Display subprojects if node is open */
		if( open ){
			for( int i = 0; i < node->nsub; i++ ){
				DisplayNode( node->sub[i].prj );
			}
			ImGui::TreePop();
		}
	}
	else {

		node_flags = ImGuiTreeNodeFlags_Leaf | \
					 ImGuiTreeNodeFlags_NoTreePushOnOpen | \
					 ImGuiTreeNodeFlags_SpanFullWidth;

		if( node->selected && (node == selected_prj) ){
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::TreeNodeEx(node->name, node_flags );

		/* Check if item has been clicked */
		if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
			selected_prj = node;	
			node->selected = true;
		}

		ImGui::TableNextColumn();
		ImGui::Text( asctime( localtime(&node->time_created)) ); /* Convert time_t to localtime */
		ImGui::TableNextColumn();
		ImGui::Text(node->ver);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(node->author);

	}

#if 0
	/* Check if something was clicked */
	if( selected_prj != NULL && node == selected_prj ){
		y_log_message(Y_LOG_LEVEL_DEBUG, "Project %s has been clicked", selected_prj->name);
		/* toggle state */
		node->selected = !(node->selected);
		y_log_message(Y_LOG_LEVEL_DEBUG, "Selection state: %d", selected_prj->selected);

		if( selected_prj->selected ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "Node %s is highlighted", node->name);
		}
		else {
			/* Don't show anything if there is no highlight; second
			 * thought, maybe not a good idea?  */
			//selected_prj = -1;	
		}
	}
#endif

}

