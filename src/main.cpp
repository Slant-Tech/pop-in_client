#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <iterator>
#include <GLFW/glfw3.h>
#include <mutex>
#include <thread>
#include <string>
#include <yder.h>
#include <db_handle.h>
#include <L2DFileDialog.h>
#include <prjcache.h>
#include <partcache.h>
//#include <invcache.h>
#include <proj_funct.h>
#include <ctype.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(x)
#else
#include <unistd.h>
#endif

#define PARTINFO_SPACING	200
#define NEWPART_SPACING		250
/* Project display state machine */
#define PRJDISP_IDLE  		0
#define PRJDISP_DISPLAYING  1
static unsigned int prj_disp_stat = PRJDISP_IDLE;

struct db_settings_t {
	char* hostname;
	int port;
};

static struct db_settings_t db_set = {NULL, 0};
static struct dbinfo_t* dbinfo = nullptr;

static void glfw_error_callback(int error, const char* description){
	y_log_message(Y_LOG_LEVEL_ERROR, "GLFW Error %d: %s", error, description);
}

/* Pass structure of projects and number of projects inside of struct */
static void show_project_select_window( class Prjcache* cache );
static void show_part_select_window( class Partcache* cache, int* i );
static void show_new_part_popup( struct dbinfo_t** info );
static void new_proj_window( struct dbinfo_t** info );
static void show_root_window( class Prjcache* prj_cache, std::vector< Partcache*>* part_cache );
static void import_parts_window( void );
static void db_settings_window( struct db_settings_t * set );

#define DB_STAT_DISCONNECTED  0
#define DB_STAT_CONNECTED  1

static int db_stat = DB_STAT_DISCONNECTED;

/* Auto refresh once every 15 seconds. 
 * Issue is longer times leads to longer quit times. 
 * Need to figure out better way to do this*/
#ifndef _WIN32
#define DB_REFRESH_SEC	(5) 
#else
#define DB_REFRESH_SEC (5 * 1000)
#endif

enum view_type {
	project_view,
	part_view,
	inventory_view,
};

static enum view_type current_view = project_view;

bool show_new_part_window = false;
bool show_new_proj_window = false;
bool show_import_parts_window = false;
bool show_db_settings_window = false;

#define DEFAULT_ROOT_W	1280
#define DEFAULT_ROOT_H	720

/* Variable to continue running */
static bool run_flag = true;

static int thread_db_connection( Prjcache* prj_cache, std::vector<Partcache*>* part_cache ) {
	
	/* Just need some buffer before beginning */
	sleep( DB_REFRESH_SEC );

	y_log_message( Y_LOG_LEVEL_INFO, "Started thread_db_connection" );
	
	/* Constantly run, until told to stop */
	while( run_flag ){
		/* Check flags for projects and handle them */
		if( db_stat == DB_STAT_CONNECTED && mutex_lock_dbinfo() ){
			prj_cache->update( &dbinfo );
			for( unsigned int i = 0; i < dbinfo->nptype; i++ ){
				(*part_cache)[i]->update( &dbinfo );
			}
		}
		mutex_unlock_dbinfo();

		/* sleep for some period of time until refresh */
		sleep( DB_REFRESH_SEC );
		y_log_message( Y_LOG_LEVEL_DEBUG, "Finished sleeping for %d seconds in thread_db", DB_REFRESH_SEC );
	
	}

	y_log_message( Y_LOG_LEVEL_INFO, "Exit thread_db_connection" );
	return 0;
}

static int thread_ui( class Prjcache* prj_cache, std::vector<Partcache *>* part_cache  ) {

	y_log_message( Y_LOG_LEVEL_INFO, "Start thread_ui" );

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
#ifndef _WIN32
	ImFont* font_hack = io.Fonts->AddFontFromFileTTF( "/usr/share/fonts/TTF/Hack-Regular.ttf", 14.0f);
#else
	ImFont* font_hack = io.Fonts->AddFontFromFileTTF( "Hack-Regular.ttf", 14.0f);
#endif
	IM_ASSERT( nullptr != font_hack );
	if( nullptr == font_hack ){
		y_log_message(Y_LOG_LEVEL_WARNING, "Could not find Hack-Regular.ttf font");
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


	/* Use first project as first selected node */
	prj_cache->select(0);

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
			show_new_part_popup( &dbinfo );
		}
		if( show_new_proj_window ){
			new_proj_window( &dbinfo );
		}
		if( show_import_parts_window ){
			import_parts_window();
		}
		if( show_db_settings_window ){
			db_settings_window(&db_set );
		}
		show_root_window(prj_cache, part_cache);
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

	/* Application cleanup */
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();

	glfwDestroyWindow(window);
	glfwTerminate();

	y_log_message( Y_LOG_LEVEL_INFO, "Exit thread_ui" );

	return 0;
}

int open_db( struct db_settings_t* set, struct dbinfo_t** info, class Prjcache* prjcache, std::vector< Partcache *>* partcaches){
	
	/* Wait for finish with displaying data */
	while( PRJDISP_DISPLAYING == prj_disp_stat );
	
	/* Check if database connection was already made; may be switching
	 * databases */
	if( DB_STAT_DISCONNECTED != db_stat ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database connection is already open");
		return -1;
	}
	y_log_message( Y_LOG_LEVEL_INFO, "Ready to connect to databse");
	if( redis_connect( db_set.hostname, db_set.port ) ){ /* Use defaults of localhost and default port */
		y_log_message( Y_LOG_LEVEL_WARNING, "Could not connect to database on request");
		db_stat = DB_STAT_DISCONNECTED;
		return -1;
	}
	else {
		y_log_message( Y_LOG_LEVEL_INFO, "Successfully connected to database");

		int retval = prjcache->update( info );

		/* Ensure that the vector is the correct size for the part types */
		if( (*info)->nptype != partcaches->size() ){
			/* Fix the size */
			partcaches->assign((*info)->nptype, nullptr);
		}

		/* For all the part types, update the list of part caches */
		for( unsigned int i = 0; i < (*info)->nptype; i++ ){
			/* If cache already exists, then erase it */
			if( nullptr != (*partcaches)[i] ){
				delete (*partcaches)[i];
			}
			/* Add new type to cache */
			(*partcaches)[i] = new Partcache((*info)->ptypes[i].npart, (*info)->ptypes[i].name);
			(*partcaches)[i]->update( &dbinfo );
		}

		db_stat = DB_STAT_CONNECTED;
		return retval;
	}
}

int main( int, char** ){
	Prjcache* prjcache = new Prjcache(1);
	std::vector< Partcache*> partcache;
	/* Initialize logging */
	y_init_logs("Pop:In", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Pop:In Inventory Management");

	
	if( open_db( &db_set, &dbinfo, prjcache, &partcache ) ){ /* Use defaults of localhost and default port */
		/* Failed to init database connection */
		y_log_message( Y_LOG_LEVEL_WARNING, "Database connection failed on startup");
	}

	/* Start database connection thread */
	std::thread db( thread_db_connection, prjcache, &partcache );

	/* Start UI thread */
	std::thread ui( thread_ui, prjcache, &partcache );

	/* Join threads */
	ui.join();

	/* Make sure that if the UI is closed, subsequent threads also close too */
	run_flag = false;
	
	db.join();

	/* Cleanup */
	db_stat = DB_STAT_DISCONNECTED;

	/* Disconnect from database */
	redis_disconnect();

	mutex_lock_dbinfo();
	if( nullptr != dbinfo ){
		free_dbinfo_t( dbinfo );
	}
	mutex_lock_dbinfo();
	free( db_set.hostname );
	delete prjcache;
	for( unsigned int i = 0; i < partcache.size(); i++ ){
		/* Clear memory inside of part cache vector */
		delete (partcache[i]);
	}
	partcache.clear();
	y_close_logs();

	return 0;

}

static void show_part_select_window( std::vector< Partcache*>* cache, int* selected ){
	
	int open_action = -1;
	ImGui::Text("Parts");

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
	if( ImGui::BeginTable("parts", 3, flags)){
		/* First column will use default _WidthStretch when ScrollX is
		 * off and _WidthFixed when ScrollX is on */
		ImGui::TableSetupColumn("Type/Part Number",   	ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Status",				ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Quantity",				ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();

		ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
										ImGuiTreeNodeFlags_OpenOnDoubleClick | \
										ImGuiTreeNodeFlags_SpanFullWidth; 
	
		/* Cache header */
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		mutex_lock_dbinfo();
		bool node_clicked[dbinfo->nptype] = {false};
		if( DB_STAT_DISCONNECTED != db_stat ){
			for( unsigned int i = 0; i < dbinfo->nptype; i++ ){
				/* Check if item has been clicked */
				(*cache)[i]->display_parts(&(node_clicked[i]));
				if( node_clicked[i] ){
					y_log_message( Y_LOG_LEVEL_DEBUG, "Part type %s is open", (*cache)[i]->type.c_str() );
					*selected = i;
				}
			}
		}
		mutex_unlock_dbinfo();
		prj_disp_stat = PRJDISP_IDLE;
		ImGui::EndTable();
	}

}


static void show_project_select_window( class Prjcache* cache ){
	
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


		if( DB_STAT_DISCONNECTED != db_stat ){
			cache->display_projects();
		}
		prj_disp_stat = PRJDISP_IDLE;
		ImGui::EndTable();
	}

}

static void show_new_part_popup( struct dbinfo_t** info ){
	static struct part_t part;
	int err_flg = 0;
	/* For entering in dynamic fields */
	static unsigned int ninfo = 0;
	static unsigned int nprice = 0;
	static unsigned int ndist = 0;
	static unsigned int ninv = 0;

	/* Info fields */
	static std::vector<std::string> info_key;
	static std::vector<std::string> info_val;

	/* Distributor info */
	static std::vector<std::string> dist_name;
	static std::vector<std::string> dist_pn;

	/* Pricing info */
	static std::vector<int> price_q;
	static std::vector<double> price_cost;

	/* Inventory info */
	static std::vector<std::string> inv_loc;
	static std::vector<unsigned int> inv_amount;


	/* Buffers for text input. Can also be used for santizing inputs */
	static char quantity[128] = {};
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
		ImGui::Text("Part Type");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputText("##new_part_type", type, 255, ImGuiInputTextFlags_CharsNoBlank);
		/* Make sure that entry is lowercase */
		for( unsigned int i = 0; i < strnlen( type, sizeof( type )); i++){
			char tmp = tolower(type[i]);	
			type[i] = tmp;
		}

		ImGui::Text("Part Number");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputText("##new_part_pn", mpn, 511, ImGuiInputTextFlags_CharsNoBlank);	

		ImGui::Text("Manufacturer");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputText("##new_part_mfg", mfg, 511);

		ImGui::Text("Part Status");
		ImGui::SameLine(NEWPART_SPACING);
		if( ImGui::BeginCombo("##new-part-status", selection_str, 0 ) ){
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

//		ImGui::Text("Local Stock");
//		ImGui::SameLine(NEWPART_SPACING);
//		ImGui::InputText("##newpart_stock", quantity, 127, ImGuiInputTextFlags_CharsNoBlank | ImGuiInputTextFlags_CharsDecimal );

		ImGui::Spacing();

		/* Inputs for fields */
		ImGui::Text("Number of custom part fields");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputInt("##newpart_ninfo", (int*)&ninfo);

		ImGui::Text("Number of Distributors");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputInt("##newpart_ndist", (int*)&ndist);

		ImGui::Text("Number of price breaks");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputInt("##newpart_nprice", (int*)&nprice);

		ImGui::Text("Number of inventory locations");
		ImGui::SameLine(NEWPART_SPACING);
		ImGui::InputInt("##newpart_ninv", (int*)&ninv);

		/* Resize vectors as needed */

		/* Info field entries */


		ImGui::Spacing();

		if( ninfo > 0 ){
			if( info_key.size() != ninfo ){
				info_key.resize( ninfo );
			}
			if( info_val.size() != ninfo ){
				info_val.resize( ninfo );
			}
			ImGui::Text("Info Fields");
			ImGui::Text("Field Name"); ImGui::SameLine(NEWPART_SPACING); ImGui::Text("Value");
		}

		std::vector<std::string>::iterator info_key_itr;
		info_key_itr = info_key.begin();
		std::vector<std::string>::iterator info_val_itr;
		info_val_itr = info_val.begin();

		std::string info_key_ident;
		std::string info_val_ident;

		for( unsigned int i = 0; i < ninfo; i++ ){
			info_key_ident = "##info_key" + std::to_string(i);
			info_val_ident = "##info_val" + std::to_string(i);

			ImGui::InputText(info_key_ident.c_str(), &(*info_key_itr), ImGuiInputTextFlags_CharsNoBlank);
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputText(info_val_ident.c_str(), &(*info_val_itr), ImGuiInputTextFlags_CharsNoBlank );

			info_key_itr++;
			info_val_itr++;
		}

		ImGui::Spacing();

		/* Distributor entries */
		if( ndist > 0 ){
			if( dist_name.size() != ndist ){
				dist_name.resize( ndist );
			}
			if( dist_pn.size() != ndist ){
				dist_pn.resize( ndist );
			}
			ImGui::Text("Distributor Information");
			ImGui::Text("Name"); ImGui::SameLine(NEWPART_SPACING); ImGui::Text("Part Number");
		}
		std::vector<std::string>::iterator dist_name_itr;
		dist_name_itr = dist_name.begin();
		std::vector<std::string>::iterator dist_pn_itr;
		dist_pn_itr = dist_pn.begin();

		std::string dist_name_ident;
		std::string dist_pn_ident;

		for( unsigned int i = 0; i < ndist; i++ ){
			dist_name_ident = "##dist_name" + std::to_string(i);
			dist_pn_ident = "##dist_pn" + std::to_string(i);

			ImGui::InputText(dist_name_ident.c_str(), &(*dist_name_itr), ImGuiInputTextFlags_CharsNoBlank );
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputText(dist_pn_ident.c_str(), &(*dist_pn_itr), ImGuiInputTextFlags_CharsNoBlank );

			dist_name_itr++;
			dist_pn_itr++;
		}

		ImGui::Spacing();

		if( nprice > 0 ){
			if( price_q.size() != nprice ){
				price_q.resize( nprice );
			}
			if( price_cost.size() != nprice ){
				price_cost.resize( nprice );
			}
			ImGui::Text("Price Break Information");
			ImGui::Text("Quantity"); ImGui::SameLine(NEWPART_SPACING); ImGui::Text("Price");
		}

		/* Price break entries */
		std::vector<int>::iterator price_q_itr;
		price_q_itr = price_q.begin();
		std::vector<double>::iterator price_cost_itr;
		price_cost_itr = price_cost.begin();

		std::string price_q_ident;
		std::string price_cost_ident;


		for( unsigned int i = 0; i < nprice; i++ ){
			price_q_ident = "##price_q" + std::to_string(i);
			price_cost_ident = "##price_cost" + std::to_string(i);

			ImGui::InputInt(price_q_ident.c_str(), &(*price_q_itr), ImGuiInputTextFlags_CharsNoBlank );
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputDouble(price_cost_ident.c_str(), &(*price_cost_itr), ImGuiInputTextFlags_CharsNoBlank );

			price_q_itr++;
			price_cost_itr++;
		}


		/* Inventory locations */
		if( ninv > 0 ){
			if( inv_loc.size() != ninv){
				inv_loc.resize( ninv );
			}
			if( inv_amount.size() != ninv ){
				inv_amount.resize( ninv );
			}
			ImGui::Text("Inventory Locations");
			ImGui::Text("Location"); ImGui::SameLine(NEWPART_SPACING); ImGui::Text("Quantitiy");
		}
		/* Inventory entries */
		std::vector<std::string>::iterator inv_loc_itr;
		inv_loc_itr = inv_loc.begin();
		std::vector<unsigned int>::iterator inv_amount_itr;
		inv_amount_itr = inv_amount.begin();

		std::string inv_loc_ident;
		std::string inv_amount_ident;

		for( unsigned int i = 0; i < ninv; i++ ){
			inv_loc_ident = "##inv_loc" + std::to_string(i);
			inv_amount_ident = "##inv_amount" + std::to_string(i);

			ImGui::InputText(inv_loc_ident.c_str(), &(*inv_loc_itr), ImGuiInputTextFlags_CharsNoBlank );
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputInt(inv_amount_ident.c_str(), (int*)&(*inv_amount_itr), ImGuiInputTextFlags_CharsNoBlank );

			inv_loc_itr++;
			inv_amount_itr++;
		}



		/* Save and cancel buttons */
		if( ImGui::Button("Save", ImVec2(0,0)) ){

			/* Check if can save first */
			if( nullptr != (*info) ){
				/* free existing data */
				free_dbinfo_t( (*info) );
				free( *info );
			}
			*info = redis_read_dbinfo();
			if( nullptr != *info ){
                                                                         
            	/* Need to do simple checks here at some point */
            	part.q = atoi( quantity );
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


				/* Store variable data information */
				part.info_len = ninfo;
				part.info = (struct part_info_t *)calloc( part.info_len, sizeof( struct part_info_t ) );
				if( nullptr == part.info ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info array in new part");
					free_part_addr_t( &part );
					show_new_part_window = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.info_len; i++ ){
					part.info[i].key = (char*)calloc( info_key[i].size()+1, sizeof( char ) );
					if( nullptr == part.info[i].key ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info key in new part");
						free_part_addr_t( &part );
						show_new_part_window = false;
					}
					strncpy(part.info[i].key, info_key[i].c_str(), info_key[i].size() );
					
					part.info[i].val = (char*)calloc( info_val[i].size()+1, sizeof( char ) );
					if( nullptr == part.info[i].val ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info val in new part");
						free_part_addr_t( &part );
						show_new_part_window = false;
					}
					strncpy(part.info[i].val, info_val[i].c_str(), info_val[i].size() );
				}

				part.price_len = nprice;

				part.price= (struct part_price_t*)calloc( part.price_len, sizeof( struct part_price_t ) );
				if( nullptr == part.price ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_addr_t( &part );
					show_new_part_window = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.price_len; i++ ){
					part.price[i].quantity = price_q[i];
					part.price[i].price = price_cost[i];
				}


				part.dist_len = ndist;
				part.dist = (struct part_dist_t*)calloc( part.dist_len, sizeof( struct part_dist_t ) );
				if( nullptr == part.dist ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor array in new part");
					free_part_addr_t( &part );
					show_new_part_window = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.dist_len; i++ ){
					part.dist[i].name = (char*)calloc( dist_name[i].size()+1, sizeof( char ) );
					if( nullptr == part.dist[i].name ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor name in new part");
						free_part_addr_t( &part );
						show_new_part_window = false;
					}
					strncpy(part.dist[i].name, dist_name[i].c_str(), dist_name[i].size() );
					
					part.dist[i].pn = (char*)calloc( dist_pn[i].size()+1, sizeof( char ) );
					if( nullptr == part.dist[i].pn ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor part number in new part");
						free_part_addr_t( &part );
						show_new_part_window = false;
					}
					strncpy(part.dist[i].pn, dist_pn[i].c_str(), dist_pn[i].size() );
				}

				part.inv_len = ninv; 
				part.inv = (struct part_inv_t*)calloc( part.inv_len, sizeof( struct part_inv_t ) );
				if( nullptr == part.inv ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_addr_t( &part );
					show_new_part_window = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.inv_len; i++ ){
					part.inv[i].q = inv_amount[i]; 
					part.inv[i].loc = dbinv_str_to_loc( info, inv_loc[i].c_str(), inv_loc[i].size() ); 
					if( part.inv[i].loc == -1 ){
						y_log_message(Y_LOG_LEVEL_ERROR, "Could not find inventory location; defaulting to location '0' ");
						part.inv[i].loc = 0;
					}
				}

				struct dbinfo_ptype_t* ptype = NULL;
				/* Find existing part type in database info */
				mutex_spin_lock_dbinfo();
				y_log_message(Y_LOG_LEVEL_DEBUG, "Current part type: %s", type );
				for( unsigned int i = 0; i < (*info)->nptype; i++){
					y_log_message(Y_LOG_LEVEL_DEBUG, "dbinfo ptype[%u]:%s", i, (*info)->ptypes[i].name );
					if( !strncmp( (*info)->ptypes[i].name, type, sizeof( type ) ) ){
						y_log_message( Y_LOG_LEVEL_DEBUG, "Found matching part type" );
						ptype = &((*info)->ptypes[i]);
					}
						
				}
				mutex_unlock_dbinfo();
	
				if( nullptr == ptype ){
					/* Type doesn't exist in database, need to add the new type */
					y_log_message(Y_LOG_LEVEL_INFO, "Type: %s not found in database. Adding type to database", type);

					/* Should break this out into function */
					mutex_spin_lock_dbinfo();
#ifndef _WIN32
					struct dbinfo_ptype_t* tmp = (struct dbinfo_ptype_t*)reallocarray( (*info)->ptypes, (*info)->nptype + 1, sizeof(struct dbinfo_ptype_t) );
#else
					struct dbinfo_ptype_t* tmp = (struct dbinfo_ptype_t*)realloc( (*info)->ptypes, ((*info)->nptype + 1) *  sizeof(struct dbinfo_ptype_t) );
#endif
					mutex_unlock_dbinfo();
					if( nullptr == tmp ){
						y_log_message(Y_LOG_LEVEL_ERROR, "Could not reallocate memory for new part type");
						err_flg = 2;
					}
					else {
						mutex_spin_lock_dbinfo();
						/* Able to reallocate, so continue adding to database  */
						(*info)->nptype++;	
						(*info)->ptypes = tmp;

						/* Initialize dbinfo_ptype_t */
						(*info)->ptypes[(*info)->nptype - 1].name = (char *)calloc( strnlen( type, sizeof( type )) + 1, sizeof( char ) );
						if( nullptr == (*info)->ptypes[(*info)->nptype - 1].name ){
							y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for new database part type");							
							err_flg = 3;
							mutex_unlock_dbinfo();
						}
						else {
							/* Copy string */
							strncpy( (*info)->ptypes[(*info)->nptype - 1].name, type, strnlen( type, sizeof( type )) );	

							/* Ensure that the number of parts is equal to zero */
							(*info)->ptypes[(*info)->nptype - 1].npart = 0;

							/* Now make sure that ptype is pointed towards new
							 * index */
							ptype = &((*info)->ptypes[(*info)->nptype - 1]);

							/* Ensure that database is updated, even if the
							 * write part fails */
							redis_write_dbinfo( *info );
						}

						mutex_unlock_dbinfo();
					}

				}
				if( !err_flg ) {
					/* Increment part in dbinfo */
					ptype->npart++;
					part.ipn = ptype->npart;

					/* Perform the write */
					redis_write_part( &part );
					y_log_message(Y_LOG_LEVEL_DEBUG, "Data written to database");


					redis_write_dbinfo( *info );
				}
			}
			else {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not write new part to database");
			}
			show_new_part_window = false;
			
			/* Clear all input data */
#if 0
			part.q = 0;
			part.ipn = 0;
			part.type = NULL;
			part.mpn = NULL;
			part.mfg = NULL;
#endif
			info_key.clear();
			info_val.clear();
			dist_name.clear();
			dist_pn.clear();
			price_q.clear();
			price_cost.clear();

			free_part_addr_t( &part );
	
			memset( quantity, 0, 127);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, finished writing");

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			show_new_part_window = false;
			/* Clear all input data */
#if 0
			part.q = 0;
			part.ipn = 0;
			part.type = NULL;
			part.mpn = NULL;
			part.mfg = NULL;
#endif

			info_key.clear();
			info_val.clear();
			dist_name.clear();
			dist_pn.clear();
			price_q.clear();
			price_cost.clear();
			free_part_addr_t( &part );
			memset( quantity, 0, 127);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, exiting window");
		}

		ImGui::End();
	}

}

static void new_proj_window( struct dbinfo_t** info ){
	static struct proj_t * prj = NULL;

	/* Buffers for text input. Can also be used for santizing inputs */
	static char version[256] = {};
	static char author[512] = {};
	static char pn[512] = {};
	static char name[1024] = {};
	static unsigned int nsubprj = 0;
	static unsigned int nboms = 0;

	static std::vector<unsigned int> bom_ipns;
	static std::vector<std::string> bom_versions;
	static std::vector<unsigned int> subprj_ipns;
	static std::vector<std::string> subprj_versions;

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("New Project Window",&show_new_proj_window, flags ) ){
		ImGui::Text("Enter part details below");
		ImGui::Separator();

		/* Text entry fields */
		ImGui::Text("Project Name");
		ImGui::SameLine();
		ImGui::InputText("##newprj_name", name, (sizeof(name) - 1));

		ImGui::Text("Part Number");
		ImGui::SameLine();
		ImGui::InputText("##newprj_pn", pn, sizeof(pn) - 1);

		ImGui::Text("Author");
		ImGui::SameLine();
		ImGui::InputText("##newprj_author", author, sizeof( author ) - 1);

		ImGui::Text("Version");
		ImGui::SameLine();
		ImGui::InputText("##newprj_version", version, sizeof( version) - 1, ImGuiInputTextFlags_CharsNoBlank);

		ImGui::Text("Number of BOMs");
		ImGui::SameLine();
		ImGui::InputInt("##newprj_nboms", (int *)&nboms);

		ImGui::Text("Number of subprojects");
		ImGui::SameLine();
		ImGui::InputInt("##newprj_nsubprj", (int *)&nsubprj);


		/* Figure out what size the vectors should be */
		if( nsubprj > 0){
			subprj_ipns.resize(nsubprj);
			subprj_versions.resize(nsubprj);
		}
		if( nboms > 0){
			bom_ipns.resize(nboms);
			bom_versions.resize(nboms);
		}

		/* Subprojects entries */
		std::vector<std::string>::iterator subprj_ver_itr;
		subprj_ver_itr = subprj_versions.begin();
		std::vector<unsigned int>::iterator subprj_ipn_itr;
		subprj_ipn_itr = subprj_ipns.begin();

		std::string sub_ipn_ident;
		std::string sub_ver_ident;

		for( unsigned int i = 0; i < nsubprj; i++ ){
			sub_ipn_ident = "##sub_ipn" + std::to_string(i);
			sub_ver_ident = "##sub_ver" + std::to_string(i);

			ImGui::Text("Subproject %d", i+1);
			ImGui::Text("Internal Part Number");
			ImGui::SameLine();
			ImGui::InputInt(sub_ipn_ident.c_str(), (int *)&(*subprj_ipn_itr) );

			ImGui::Text("Version");
			ImGui::SameLine();
			ImGui::InputText(sub_ver_ident.c_str(), (char *)(*subprj_ver_itr).c_str(), 255 );

			subprj_ver_itr++;
			subprj_ipn_itr++;

		}

		/* For BOM entries */
		std::vector<std::string>::iterator bom_ver_itr;
		bom_ver_itr = bom_versions.begin();
		std::vector<unsigned int>::iterator bom_ipn_itr;
		bom_ipn_itr = bom_ipns.begin();

		std::string bom_ipn_ident;
		std::string bom_ver_ident;

		for( unsigned int i = 0; i < nboms; i++ ){
			bom_ipn_ident = "##bom_ipn" + std::to_string(i);
			bom_ver_ident = "##bom_ver" + std::to_string(i);

			ImGui::Text("BOM %d", i+1);
			ImGui::Text("Internal Part Number");
			ImGui::SameLine();
			ImGui::InputInt(bom_ipn_ident.c_str(), (int *)&(*bom_ipn_itr) );

			ImGui::Text("Version");
			ImGui::SameLine();
			ImGui::InputText(bom_ver_ident.c_str(), (char *)(*bom_ver_itr).c_str(), 255 );

			bom_ver_itr++;
			bom_ipn_itr++;

		}



		/* Save and cancel buttons */
		if( ImGui::Button("Save", ImVec2(0,0)) ){

			/* Check if valid to copy */
			prj = (struct proj_t *)calloc(1, sizeof(struct proj_t));
			if( nullptr == prj ){
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for saving new project");
				show_new_proj_window = false;
				return;
			}
			/* No checks because I'll totally do it later (hopefully) */
			prj->flags = 0;
			prj->selected = 0;

			prj->nboms = nboms;
			prj->nsub = nsubprj;

			/* Add time created and modified, which they are both the same since the project is new */
			prj->time_created = time(NULL);
			prj->time_mod = prj->time_created;

			prj->pn = (char *)calloc( strnlen(pn, sizeof(pn)) + 1, sizeof(char) );
			strcpy( prj->pn, pn );
			printf("String length pn: %d\n", strnlen(pn, sizeof(pn)));
			printf("String length prj->pn: %d\n", strlen(prj->pn));
			printf("Sizeof pn: %d\n", sizeof(prj->pn) );

			prj->name = (char *)calloc( strnlen(name, sizeof(name)) + 1, sizeof(char) );
			strcpy( prj->name, name );

			prj->author = (char *)calloc( strnlen(author, sizeof(author)) + 1 , sizeof( char ) );
			strcpy( prj->author, author );

			prj->ver = (char *)calloc( strnlen(version, sizeof(version)) + 1 , sizeof( char ) );
			strcpy( prj->ver, version );

			/* Create new bom and subproject arrays */
			prj->boms = (struct proj_bom_ver_t*)calloc( nboms, sizeof( struct proj_bom_ver_t) );
			if( nullptr == prj->boms) {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for new project bom array");
				show_new_proj_window = false;
				return;
			}
			for( unsigned int i = 0; i < nboms; i++){
				prj->boms[i].ver = (char *)calloc( strlen( (char *)bom_versions[i].c_str() ) + 1, sizeof( char ) );
				strcpy( prj->boms[i].ver, (char *)bom_versions[i].c_str());
				prj->boms[i].bom = (struct bom_t*)calloc( 1, sizeof( struct bom_t ) );
				prj->boms[i].bom->ipn = bom_ipns[i];
			}

			prj->sub = (struct proj_subprj_ver_t*)calloc( nsubprj, sizeof( struct proj_subprj_ver_t) );
			if( nullptr == prj->sub) {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for new project subproject array");
				show_new_proj_window = false;
				return;
			}
			for( unsigned int i = 0; i < nsubprj; i++){
				prj->sub[i].ver = (char *)calloc( strlen( (char *)subprj_versions[i].c_str() ) + 1, sizeof( char ) );
				strcpy( prj->sub[i].ver, (char *)subprj_versions[i].c_str());
				prj->sub[i].prj = (struct proj_t*)calloc( 1, sizeof( struct proj_t ) );
				prj->sub[i].prj->ipn = subprj_ipns[i];

			}

			/* Increment database information */
			mutex_spin_lock_dbinfo();
			(*info)->nprj++;
			prj->ipn = (*info)->nprj;
			/* Perform the write */
			redis_write_proj( prj );
			y_log_message(Y_LOG_LEVEL_DEBUG, "Data written to database");
			show_new_proj_window = false;


			redis_write_dbinfo( *info );

			/* Make sure to read back the same data incase something went wrong */
			if( nullptr != (*info) ){
				/* free existing data */
				free_dbinfo_t( (*info) );
				free( *info );
			}
			(*info) = redis_read_dbinfo();

			mutex_unlock_dbinfo();

			/* Clear all input data */
			free_proj_t( prj );
			prj = NULL;

			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out project, finished writing");

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			show_new_proj_window = false;
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

static void db_settings_window( struct db_settings_t * set ){
	static char hostname[1024] = "localhost";
	static int port = 6379;

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("Database Settings", &show_db_settings_window, flags) ){
		
		ImGui::Text("Hostname ");
		ImGui::SameLine();
		ImGui::InputText("##database_hostname", hostname, sizeof( hostname ) );

		ImGui::Text("Port ");
		ImGui::SameLine();
		ImGui::InputInt("##database_port", &port);

		/* Button to save settings */
		if( ImGui::Button("Save") ){
			/* Start importing */
			y_log_message(Y_LOG_LEVEL_DEBUG, "New database connection: %s:%d", hostname, port);

			/* Clear out previous setting */
			if( NULL != set->hostname ){
				memset( set->hostname, 0, sizeof( set->hostname ) );
				set->hostname = NULL;
			}

			/* Copy new setting; make sure to not copy more than the size of
			 * the input */
			set->hostname = (char *)calloc( strnlen( hostname ,sizeof(hostname)) + 1, sizeof( char ) );
			if( nullptr == set->hostname ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for database hostname setting");
			}
			else{
				/* No issues, so copy data over */
				strncpy( set->hostname, hostname, strnlen( hostname ,sizeof(hostname)) );
				y_log_message(Y_LOG_LEVEL_DEBUG, "Copied hostname: %s", set->hostname);
				set->port = port;

				/* Reset inputs to defaults */
				memset( hostname, 0, sizeof( hostname ) );
				strncpy( hostname, "localhost", sizeof( hostname ) );	
				port = 6379;

			}
			/* End of window */
			show_db_settings_window = false;
		}
		ImGui::SameLine();

		/* Button to cancel operation */
		if( ImGui::Button("Cancel") ){
			/* Cleanup and exit window */
			/* End of window */
			show_db_settings_window = false;
		}

		ImGui::End();

	}

}


/* Menu items */
static void show_menu_bar( class Prjcache* prjcache, std::vector<Partcache*>* partcache ){

	if( ImGui::BeginMenuBar() ){
		/* File Menu */
		if( ImGui::BeginMenu("File") ) {
			if( ImGui::MenuItem("New Project") ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "New Project menu clicked");
				show_new_proj_window = true;			
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

		/* View Menu */
		if( ImGui::BeginMenu("View") ){
			if( ImGui::MenuItem("Project View") ){
				current_view = project_view;	
			}
			else if( ImGui::MenuItem("Part View") ){
				current_view = part_view;
			}
			else if( ImGui::MenuItem("Inventory View") ){
				current_view = inventory_view;
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
				y_log_message(Y_LOG_LEVEL_DEBUG, "Clicked Network->connect");
				if( open_db( &db_set, &dbinfo, prjcache, partcache ) ){ /* Use defaults of localhost and default port */
					/* Failed to init database connection */
					y_log_message( Y_LOG_LEVEL_WARNING, "Database connection failed on startup");
				}
				else{
					prjcache->select(0);
				}
			}
			else if( ImGui::MenuItem("Server Settings")){
				show_db_settings_window = true;
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

static void proj_bom_tab( struct bom_t* bom  ){

	static part_t *selected_item = NULL;	


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
			
		if( nullptr != bom ){
			/* Used for selecting specific item in BOM */
			bool item_sel[ bom->nitems ] = {};
			char line_item_label[64]; /* May need to change size at some point */

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
#if 0
					if( nullptr != selected_item ){
						free_part_t( selected_item );
						selected_item = nullptr;
					}
					selected_item = copy_part_t(bom->parts[i]);
#endif
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
					ImGui::Text("Part Number:"); 
					ImGui::SameLine(PARTINFO_SPACING); 
					ImGui::Text("%s", selected_item->mpn);

					ImGui::Text("Part Status:");
					ImGui::SameLine(PARTINFO_SPACING); 
					switch( selected_item->status ){
						case pstat_prod:
							ImGui::Text("In Production");
							break;
						case pstat_low_stock:
							ImGui::Text("Low Stock Available");
							break;
						case pstat_unavailable:
							ImGui::Text("Unavailable");
							break;
						case pstat_nrnd:
							ImGui::Text("Not Recommended for New Designs");
							break;
						case pstat_obsolete:
							ImGui::Text("Obsolete");
							break;
						case pstat_unknown:
						default:
							/* FALLTHRU */
							ImGui::Text("Unknown");
							break;
					}
					ImGui::Text("Part Fields:");
					ImGui::Indent();
					for( unsigned int i = 0 ; i < selected_item->info_len; i++ ){
						ImGui::Text("%s", selected_item->info[i].key );
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%s", selected_item->info[i].val);
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Inventory");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->inv_len; i++ ){
						mutex_lock_dbinfo();
						ImGui::Text("%s", dbinfo->invs[selected_item->inv[i].loc].name);	
						mutex_unlock_dbinfo();
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%ld", selected_item->inv[i].q  );
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Distributor P/Ns:");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->dist_len; i++ ){
						ImGui::Text("%s", selected_item->dist[i].name);	
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%s", selected_item->dist[i].pn  );
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Price Breaks:");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->price_len; i++ ){
						ImGui::Text("%ld:", selected_item->price[i].quantity);	
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("$%0.6lf", selected_item->price[i].price  );
					}
					ImGui::Unindent();
					ImGui::Spacing();
				}
				else{
					ImGui::Text("Part Number not found");
				}

				ImGui::Separator();
	
				if( ImGui::Button("OK", ImVec2(120,0))){
					if( nullptr != selected_item ){
						selected_item = nullptr;
					}
					
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				
				ImGui::EndPopup();
			}
		}
		else{
			y_log_message(Y_LOG_LEVEL_ERROR, "Issue getting bom for project bom tab");
		}
		ImGui::EndTable();
	}

}

static void proj_info_tab( struct proj_t* prj, int* bom_index ){

	static unsigned int nitems = 0;
	static unsigned int last_prjipn = 0;

	/* Check if already retrieved data from this project */
	if( last_prjipn != prj->ipn ){
		nitems = get_num_all_proj_items( prj );
		last_prjipn = prj->ipn;
	}

	/* Show information about project with selections for versions etc */
	ImGui::Text("Project Information Tab");
	ImGui::Separator();
	
	ImGui::Text("Project Part Number: %s", prj->pn);

	/* Project Version */
	ImGui::Text("Project Version: %s", prj->ver);
	
	ImGui::Spacing();
	ImGui::Text("Project Bill of Materials ");
	/* BOM Selection */
	const char* default_bom_ver = prj->boms[*bom_index].ver;
	if( ImGui::BeginCombo("##selprj_bom", default_bom_ver ) ){
		for( unsigned int i = 0; i < prj->nboms; i++ ){
			const bool is_selected = (*bom_index == i);
			if( ImGui::Selectable( prj->boms[i].ver, is_selected ) ){
				*bom_index = i;
			}
			if( is_selected ){
				ImGui::SetItemDefaultFocus();
			}
		}	

		ImGui::EndCombo();
	}

	ImGui::Spacing();

	ImGui::Text("Number of parts in BOM: %d", nitems);

}

static void proj_data_window( class Prjcache* cache ){
	static int bom_index = 0;
	static bom_t* bom = nullptr;
	/* Show BOM/Information View */
	ImGuiTabBarFlags tabbar_flags = ImGuiTabBarFlags_None;
	if( ImGui::BeginTabBar("Project Info", tabbar_flags ) ){
		if( ImGui::BeginTabItem("Info") ){
			proj_info_tab( cache->get_selected(), &bom_index );
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem("BOM") ){
			/* Copy BOM since it could be corrupted otherwise */
			if( nullptr != bom  && bom->ipn != cache->get_selected()->boms[bom_index].bom->ipn ){
				/* Only free the bom copy if a new selection has been made */
				free_bom_t( bom );
				bom = nullptr;
			}
			if( nullptr == bom ){
				/* Simpler handling since regardless if different selection or
				 * never initialized, can now copy data */
				bom = copy_bom_t(cache->get_selected()->boms[bom_index].bom);
			}
			proj_bom_tab( bom );
			ImGui::EndTabItem();
		}
		
		ImGui::EndTabBar();
	}

}

static void show_project_view( class Prjcache* cache, ImGuiTableFlags table_flags ){
	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		/* Project view on the left */
		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Project Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		if( nullptr != cache && DB_STAT_DISCONNECTED != db_stat ){
			show_project_select_window(cache);
		}
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Project Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected project */
		if( nullptr != cache->get_selected() ){
			proj_data_window( cache );
		} 

		ImGui::EndChild();
		ImGui::EndTable();	
	}

}




static void part_info_tab( class Partcache* cache ){

	static part_t *selected_item = NULL;	


	ImGui::Text("Project BOM Tab");

	/* Part Specific table view */

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

	if( ImGui::BeginTable("Part", 5, table_flags ) ){
		ImGui::TableSetupColumn("P/N", ImGuiTableColumnFlags_PreferSortAscending);
		ImGui::TableSetupColumn("Manufacturer", ImGuiTableColumnFlags_PreferSortAscending);
		ImGui::TableSetupColumn("Quantitiy", ImGuiTableColumnFlags_PreferSortAscending);
		ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_PreferSortAscending);
		ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_PreferSortAscending);

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
			
		if( nullptr != cache ){
			/* Used for selecting specific item in BOM */
			bool item_sel[ cache->items() ] = {};
			char part_mpn_label[64]; /* May need to change size at some point */
			struct part_t* part = nullptr;
			struct part_t* tmp = nullptr;
			for( int i = 0; i < cache->items(); i++){
				ImGui::TableNextRow();
				if( nullptr != part ){
					free_part_t( part );
					part = nullptr;
				}
				tmp = cache->read(i);
				/* Copy part to temporary storage */
				part = copy_part_t( tmp );

				/* Part number */
				ImGui::TableSetColumnIndex(0);
				snprintf(part_mpn_label, 64, "%s", part->mpn);
				ImGui::Selectable(part_mpn_label, &item_sel[i], ImGuiSelectableFlags_SpanAllColumns);

				/* Manufacturer */
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%s", part->mfg );

				/* Quantity */
				ImGui::TableSetColumnIndex(2);
				unsigned int total = 0;
				for( unsigned int i = 0; i < part->inv_len; i++ ){
					total += part->inv[i].q;	
				}
				ImGui::Text("%d", total );

				/* Type */
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%s", part->type);

				/* Status */
				ImGui::TableSetColumnIndex(4);
				switch ( part->status ) {
					case pstat_prod:
						ImGui::Text("Production");
						break;
					case pstat_low_stock:
						ImGui::TextColored(ImVec4(1.0f, 0.8117647f, 0.0f, 1.0f),"Low Stock");
						break;
					case pstat_unavailable:
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),"Unavailable");
						break;
					case pstat_nrnd:
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),"Not Recommended for New Designs");
						break;
					case pstat_lasttimebuy:
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),"Last Time Buy");
						break;
					case pstat_obsolete:
						ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),"Obsolete");
						break;
					case pstat_unknown:
					default:
						/* FALLTHRU */
						ImGui::TextColored(ImVec4(1.0f, 0.8117647f, 0.0f, 1.0f),"Unknown");
						break;
				}


				if( item_sel[i] ){
					/* Open popup for part info */
					ImGui::OpenPopup("PartInfo");
					y_log_message(Y_LOG_LEVEL_DEBUG, "%s was selected", part->mpn);
					if( nullptr != selected_item ){
						free_part_t( selected_item );
						selected_item = nullptr;
					}
					selected_item = copy_part_t(part);
				}

				if( nullptr != part ){
					free_part_t( part );
					part = nullptr;
				}

			}
			/* Popup window for Part info */
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			if( ImGui::BeginPopupModal("PartInfo", NULL, ImGuiWindowFlags_AlwaysAutoResize) ){
				ImGui::Text("Part Info");
				ImGui::Separator();
				
				if( NULL != selected_item ){
					ImGui::Text("Part Number:"); 
					ImGui::SameLine(PARTINFO_SPACING); 
					ImGui::Text("%s", selected_item->mpn);

					ImGui::Text("Part Status:");
					ImGui::SameLine(PARTINFO_SPACING); 
					switch( selected_item->status ){
						case pstat_prod:
							ImGui::Text("In Production");
							break;
						case pstat_low_stock:
							ImGui::Text("Low Stock Available");
							break;
						case pstat_unavailable:
							ImGui::Text("Unavailable");
							break;
						case pstat_nrnd:
							ImGui::Text("Not Recommended for New Designs");
							break;
						case pstat_obsolete:
							ImGui::Text("Obsolete");
							break;
						case pstat_unknown:
						default:
							/* FALLTHRU */
							ImGui::Text("Unknown");
							break;
					}
					ImGui::Text("Part Fields:");
					ImGui::Indent();
					for( unsigned int i = 0 ; i < selected_item->info_len; i++ ){
						ImGui::Text("%s", selected_item->info[i].key );
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%s", selected_item->info[i].val);
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Inventory");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->inv_len; i++ ){
						mutex_lock_dbinfo();
						ImGui::Text("%s", dbinfo->invs[selected_item->inv[i].loc].name);	
						mutex_unlock_dbinfo();
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%ld", selected_item->inv[i].q  );
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Distributor P/Ns:");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->dist_len; i++ ){
						ImGui::Text("%s", selected_item->dist[i].name);	
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("%s", selected_item->dist[i].pn  );
					}
					ImGui::Unindent();
					ImGui::Spacing();

					ImGui::Text("Price Breaks:");
					ImGui::Indent();
					for( unsigned int i = 0; i <  selected_item->price_len; i++ ){
						ImGui::Text("%ld:", selected_item->price[i].quantity);	
						ImGui::SameLine(PARTINFO_SPACING); 
						ImGui::Text("$%0.6lf", selected_item->price[i].price  );
					}
					ImGui::Unindent();
					ImGui::Spacing();
				}
				else{
					ImGui::Text("Part Number not found");
				}

				ImGui::Separator();
	
				if( ImGui::Button("OK", ImVec2(120,0))){
					if( nullptr != selected_item ){
						selected_item = nullptr;
					}
					
					ImGui::CloseCurrentPopup();
				}
				ImGui::SetItemDefaultFocus();
				
				ImGui::EndPopup();
			}
		}
		else{
			y_log_message(Y_LOG_LEVEL_ERROR, "Issue getting selected item for part type info tab");
		}
		ImGui::EndTable();
	}

}

static void part_analytic_tab( class Partcache* cache ){

	static std::string last_type = "";

	/* Check if cache is valid */
	if( nullptr != cache ){
		/* Check if already retrieved data from this project */
		if( last_type != cache->type ){
			/* Grab new data */

			/* Save last ipn */
			last_type = cache->type;
		}

		/* Show information about project with selections for versions etc */
		ImGui::Text("Part Analytics Tab");
		ImGui::Separator();
		
		ImGui::Text("Type: %s", cache->type.c_str());
		ImGui::Text("Number of unique parts: %d", cache->items());

		ImGui::Spacing();
	}
	else {
		ImGui::Text("Part Type cache is invalid");
	}

}

static void part_data_window( std::vector< Partcache*>* cache, int index ){
	static part_t* p = nullptr;
	static class Partcache* selected = (*cache)[index];
	/* Show BOM/Information View */
	ImGuiTabBarFlags tabbar_flags = ImGuiTabBarFlags_None;

	/* Update index */
	if( index > 0 || index <= (*cache).size() ){
		selected = (*cache)[index];	
	}

	if( ImGui::BeginTabBar("Part Info", tabbar_flags ) ){
		if( ImGui::BeginTabItem("Analytics") ){
			part_analytic_tab( selected );
			ImGui::EndTabItem();
		}
		if( ImGui::BeginTabItem("Info") ){
			part_info_tab( selected );
			ImGui::EndTabItem();
		}
		
		ImGui::EndTabBar();
	}

}

static void show_part_view( std::vector< Partcache*>* cache, ImGuiTableFlags table_flags ){
	static int selected_cache = 0;

	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Part Type Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		if( nullptr != cache && DB_STAT_DISCONNECTED != db_stat ){
			show_part_select_window(cache, &selected_cache);
		}
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Part Type Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected part type cache info */
		if( nullptr != cache ){
			part_data_window( cache, selected_cache );
		} 

		ImGui::EndChild();
		ImGui::EndTable();	
	}

}

#if 0
static show_inventory_view( class InvCache* cache, static ImGuiTableFlags table_flags ){
	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		/* Project view on the left */
		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Project Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		if( nullptr != cache && DB_STAT_DISCONNECTED != db_stat ){
			show_project_select_window(cache);
		}
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Project Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected project */
		if( nullptr != cache->get_selected() ){
			proj_data_window( cache );
		} 

		ImGui::EndChild();
		ImGui::EndTable();	
	}

}

#endif

/* Setup root window, child windows */
static void show_root_window( class Prjcache* prj_cache, std::vector< Partcache*>* part_cache ){

	/* Create menu items */
	show_menu_bar( prj_cache, part_cache );
	
	/* Put the different items into columns*/
	static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingStretchProp | \
										 ImGuiTableFlags_Resizable | \
										 ImGuiTableFlags_BordersOuter | \
										 ImGuiTableFlags_BordersV | \
										 ImGuiTableFlags_Reorderable | \
										 ImGuiTableFlags_ContextMenuInBody;
	switch( current_view ){

		case part_view:
			show_part_view( part_cache, table_flags );
			break;
		case inventory_view:
			break;
		case project_view:
		default:
			show_project_view( prj_cache, table_flags );
			break;
	}


	/* Show status of database connection*/
	switch(db_stat){
		case DB_STAT_DISCONNECTED:
			ImGui::Text("Database Disconnected");
			break;
		case DB_STAT_CONNECTED:
			ImGui::Text("Database Connected");
			break;
		default:
			ImGui::Text("Unknown Database Error");
			break;
	}

	ImGui::Spacing();

}

