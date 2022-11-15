#include <imgui.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <time.h>
#include <vector>
#include <iterator>
#include <atomic>
#include <sstream>
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
#include <ui_projview.h>
#include <ui_parts.h>
#include <proj_funct.h>
#include <ctype.h>
#include <dbstat_def.h>

#ifdef _WIN32
#include <windows.h>
#define sleep(x) Sleep(x)
#else
#include <unistd.h>
#endif

/* Project display state machine */
#define PRJDISP_IDLE  		0
#define PRJDISP_DISPLAYING  1
static unsigned int prj_disp_stat = PRJDISP_IDLE;

static part_t* gselected_part = nullptr;

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

static void show_part_select_window( struct dbinfo_t ** info, std::vector< Partcache*>* cache, int* selected );

static void new_proj_window( struct dbinfo_t** info );
static void new_bom_window( struct dbinfo_t** info );
static void show_root_window( struct dbinfo_t** info, class Prjcache* prj_cache, std::vector< Partcache*>* part_cache );
static void import_parts_window( void );
static void db_settings_window( struct db_settings_t * set );


static int db_stat = DB_STAT_DISCONNECTED;

/* All project view: will show projects that do or do not have a subproject */
static bool show_all_projects = false;


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
bool show_edit_part_window = false;
bool show_new_proj_window = false;
bool show_new_bom_window = false;
bool show_import_parts_window = false;
bool show_db_settings_window = false;

#define DEFAULT_ROOT_W	1280
#define DEFAULT_ROOT_H	720

/* Variable to continue running */
static std::atomic<bool> run_flag = true;

static int thread_db_connection( Prjcache* prj_cache, std::vector<Partcache*>* part_cache ) {
	
	/* Just need some buffer before beginning */
	sleep( DB_REFRESH_SEC );

	y_log_message( Y_LOG_LEVEL_INFO, "Started thread_db_connection" );
	
	/* Constantly run, until told to stop */
	while( run_flag ){
		/* Check flags for projects and handle them */
		if( db_stat == DB_STAT_CONNECTED ){

			/* update dbinfo */
			if( mutex_lock_dbinfo() == 0 ){
				/* acquired lock, update dbinfo */
				if( nullptr != dbinfo ){
					free_dbinfo_t( dbinfo );
					free( dbinfo );
					dbinfo = nullptr;
				}
				dbinfo = redis_read_dbinfo();
				if( nullptr == dbinfo ){
					/* this is a really bad situation */
					y_log_message( Y_LOG_LEVEL_ERROR, "Database information could not be allocated, program may crash");
					/* Unlock dbinfo */
					mutex_unlock_dbinfo();
				}
				else {
					/* Unlock dbinfo */
					mutex_unlock_dbinfo();

					/* Update caches */
					prj_cache->update( &dbinfo );

					/* Ensure that the vector is the correct size for the part types */
					if( dbinfo->nptype > part_cache->size() ){
						/* Data is out of date, critical to update */
						mutex_spin_lock_dbinfo();
						/* Fix the size */
						unsigned int old_size = part_cache->size();

						part_cache->assign(dbinfo->nptype, nullptr);
						for( unsigned int i = old_size; i < part_cache->size(); i++){
							(*part_cache)[i] = new Partcache(dbinfo->ptypes[i].npart, dbinfo->ptypes[i].name);
							if( (*part_cache)[i]->update( &dbinfo ) ){
								y_log_message( Y_LOG_LEVEL_ERROR,"Could not update part cache: %s", (*part_cache)[i]->type.c_str()); 
							}
						}
						/* Unlock dbinfo */
						mutex_unlock_dbinfo();
						y_log_message( Y_LOG_LEVEL_DEBUG, "Resized partcache to %d", dbinfo->nptype);
					}
					else if( dbinfo->nptype < part_cache->size() ){
						/* Data is out of date, critical to update */
						mutex_spin_lock_dbinfo();
						unsigned int old_size = part_cache->size();
						/* Delete extra caches */
						for( unsigned int i = dbinfo->nptype; i < old_size ; i++ ){
							delete ((*part_cache)[i]);
						}
						part_cache->resize(dbinfo->nptype);
						/* Unlock dbinfo */
						mutex_unlock_dbinfo();
					}
					for( unsigned int i = 0; i < part_cache->size() ; i++ ){
						if( nullptr != (*part_cache)[i] ){
							if( (*part_cache)[i]->update( &dbinfo ) ){
								y_log_message( Y_LOG_LEVEL_ERROR,"Could not update part cache: %s", (*part_cache)[i]->type.c_str()); 
							}
						}
					}
				}
			}
		}
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
	ImPlot::CreateContext();
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
			new_part_window( &show_new_part_window, &dbinfo );
		}
		if( show_edit_part_window ){
			switch( current_view ){
				case part_view:
					edit_part_window( &show_edit_part_window, gselected_part, &dbinfo );
					break;
				case project_view:
				default:
					/* FALLTHRU */
					y_log_message( Y_LOG_LEVEL_ERROR, "Edit part window not supported in this view" );
					show_edit_part_window = false;
					break;
			}
		}
		if( show_new_proj_window ){
			new_proj_window( &dbinfo );
		}
		if( show_new_bom_window ){
			new_bom_window( &dbinfo );
		}
		if( show_import_parts_window ){
			import_parts_window();
		}
		if( show_db_settings_window ){
			db_settings_window(&db_set );
		}
		show_root_window( &dbinfo, prj_cache, part_cache);
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
	int retval = -1;
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


		if( mutex_lock_dbinfo() == 0 ){
			/* acquired lock, update dbinfo */
			if( nullptr != dbinfo ){
				free_dbinfo_t( dbinfo );
				free( dbinfo );
				dbinfo = nullptr;
			}
			dbinfo = redis_read_dbinfo();
			if( nullptr == (*info) ){
				/* this is a really bad situation */
				y_log_message( Y_LOG_LEVEL_ERROR, "Database information could not be allocated, program may crash");
			}
			/* Unlock dbinfo */
			mutex_unlock_dbinfo();
		}

		if( nullptr != info ){

			retval = prjcache->update( info );

			/* Ensure that the vector is the correct size for the part types */
			if( (*info)->nptype != partcaches->size() ){
				/* Fix the size */
				partcaches->assign((*info)->nptype, nullptr);
				y_log_message( Y_LOG_LEVEL_DEBUG, "Resized partcache to %d", (*info)->nptype);
			}

			/* For all the part types, update the list of part caches */
			for( unsigned int i = 0; i < (*info)->nptype; i++ ){
				/* If cache already exists, then erase it */
				if( nullptr != (*partcaches)[i] ){
					delete (*partcaches)[i];
				}
				/* Add new type to cache */
				(*partcaches)[i] = new Partcache((*info)->ptypes[i].npart, (*info)->ptypes[i].name);
				if( (*partcaches)[i]->update( &dbinfo ) ){
					y_log_message( Y_LOG_LEVEL_ERROR,"Could not update part cache: %s", (*partcaches)[i]->type.c_str());
				}
			}

			db_stat = DB_STAT_CONNECTED;
		}
		return retval;
	}
}

int main( int, char** ){
	Prjcache* prjcache = new Prjcache(1);
	std::vector< Partcache*> partcache;
	/* Initialize logging */
	y_init_logs("Pop:In", Y_LOG_MODE_FILE, Y_LOG_LEVEL_DEBUG, "./popin.log", "Pop:In Inventory Management");

	
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

	mutex_spin_lock_dbinfo();
	if( nullptr != dbinfo ){
		free_dbinfo_t( dbinfo );
	}
	mutex_unlock_dbinfo();
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

static void show_part_select_window( struct dbinfo_t ** info, std::vector< Partcache*>* cache, int* selected ){
	
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
	if( ImGui::BeginTable("parts", 2, flags)){
		/* First column will use default _WidthStretch when ScrollX is
		 * off and _WidthFixed when ScrollX is on */
		ImGui::TableSetupColumn("Type/Part Number",   	ImGuiTableColumnFlags_NoHide);
//		ImGui::TableSetupColumn("Status",				ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Quantity",				ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();

		ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
										ImGuiTreeNodeFlags_OpenOnDoubleClick | \
										ImGuiTreeNodeFlags_SpanFullWidth; 
	
		/* Cache header */
		ImGui::TableNextRow();
		ImGui::TableNextColumn();
//		mutex_spin_lock_dbinfo();
		//if( nullptr != info && nullptr != (*info) && cache->size() > 0 ){
		bool is_selected = false;
		if( cache->size() > 0 ){
			bool node_clicked[cache->size()] = {false};
			if( DB_STAT_DISCONNECTED != db_stat ){
				for( unsigned int i = 0; i < cache->size(); i++ ){
					/* Check if item has been clicked */
					node_clicked[i] = (*selected == i);
					(*cache)[i]->display_parts(&(node_clicked[i]));
					if( node_clicked[i] ){
						y_log_message( Y_LOG_LEVEL_DEBUG, "Part type %s is open", (*cache)[i]->type.c_str() );
						*selected = i;
					}
				}
			}
		}
//		mutex_unlock_dbinfo();
		prj_disp_stat = PRJDISP_IDLE;
		ImGui::EndTable();
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

		ImGui::Text("Number of BOMs: ");
		ImGui::SameLine();
		ImGui::Text("%u", nboms);

		ImGui::Text("Number of subprojects");
		ImGui::SameLine();
		ImGui::InputInt("##newprj_nsubprj", (int *)&nsubprj);


		/* Figure out what size the vectors should be */
		if( nsubprj > 0){
			subprj_ipns.resize(nsubprj);
			subprj_versions.resize(nsubprj);
		}

#if 0
		if( nboms > 0){
			bom_ipns.resize(nboms);
			bom_versions.resize(nboms);
		}
#endif
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
			ImGui::InputText(sub_ver_ident.c_str(), &(*subprj_ver_itr), ImGuiInputTextFlags_CharsNoBlank);

			subprj_ver_itr++;
			subprj_ipn_itr++;

		}

#if 0
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
			ImGui::InputInt(bom_ipn_ident.c_str(), (int *)&(*bom_ipn_itr));

			ImGui::Text("Version");
			ImGui::SameLine();
			ImGui::InputText(bom_ver_ident.c_str(), &(*bom_ver_itr) );

			bom_ver_itr++;
			bom_ipn_itr++;

		}
#endif
		/* Add new BOM */
		static std::string bom_name = ""; /* Search string for bom name */
		static std::string old_bom_name = ""; /* Used for checking if need to update */
		static std::string bom_selection; /* Selection string for dropdown */
		static unsigned int bom_selection_idx = 0; /* Selection index */
		static unsigned int nbom_strs = 0; /* Number of boms used */
		static char** boms = nullptr;	/* strings for bom names */
		ImGui::Text("BOM Name");
		ImGui::SameLine();
		ImGui::InputText("##newprj_bom_name_search", &bom_name);
		/* If text is available, and different than old, then search for items */
		if( bom_name != old_bom_name ){

			/* Free strings if already allocated */
			if( nullptr != boms ){
				for( unsigned int i = 0; i < nbom_strs; i++ ){
					free( boms[i] );
					boms[i] = nullptr;
				}
				nbom_strs = 0;
				boms = nullptr;
			}

			/* Get info from database */
			boms = search_bom_name( bom_name.c_str(), &nbom_strs );
			printf("Number of items returned: %d\n", nbom_strs);

			if( nullptr == boms ){
				y_log_message( Y_LOG_LEVEL_INFO, "Could not find related bom names in database" );
			}
			/* Overwrite old name */
			old_bom_name = bom_name;
		}

		/* Display dropdown selection for BOM to use */
		if( ImGui::BeginCombo("##newprj_bom_select", bom_selection.c_str() ) ){
			for( unsigned int i = 0; i < nbom_strs; i++ ){
				const bool is_selected = ( bom_selection_idx == i );
				if( ImGui::Selectable( boms[i], is_selected ) ){
					bom_selection_idx = i;	
				}
				if( is_selected ){
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if( nullptr != boms ){
			bom_selection = boms[bom_selection_idx];
		}
		else {
			bom_selection = "No Results";
		}


		/* Button to add new BOM */
		if( ImGui::Button("Add BOM", ImVec2(0,0)) ){
			if( nullptr != boms ) {
				/* Split strings */	
				char * bom_ptr = nullptr;
				bom_ptr = strtok( (char*)bom_selection.c_str(), ":" );
				printf("Strtok 1: %s\n", bom_ptr);

				/* Add BOM to vectors */
				bom_ptr = strtok( NULL, ":");
				printf("Strtok 2: %s\n", bom_ptr);
				bom_ipns.push_back( atoi(bom_ptr) );

				bom_ptr = strtok( NULL, ":" );
				printf("Strtok 3: %s\n", bom_ptr);
				std::string version_tmp;
				version_tmp.assign(bom_ptr);
				printf("Copied string: %s\n", version_tmp.c_str());
				bom_versions.push_back(version_tmp);

				nboms++;
				printf("nboms in project: %u\n", nboms);
				printf("BOM ipn appended at %d: bom:%u:%s\n", nboms,  bom_ipns.back(), bom_versions.back().c_str());

			}
			/* Free strings if already allocated */
			if( nullptr != boms ){
				for( unsigned int i = 0; i < nbom_strs; i++ ){
					free( boms[i] );
					boms[i] = nullptr;
				}
				nbom_strs = 0;
				boms = nullptr;
			}
			old_bom_name = "";
		}



		/* Save and cancel buttons */
		if( ImGui::Button("Save", ImVec2(0,0)) ){

			/* Clean up some memory */
			if( nullptr != boms ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "Cleanup for bom strings");
				for( unsigned int i = 0; i < nbom_strs; i++ ){
					free( boms[i] );
					boms[i] = nullptr;
				}
				nbom_strs = 0;
				boms = nullptr;
			}

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
			redis_write_proj( prj ); /* NOTE: Segfault due to data race occurred; need to investigate */
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

			/* Clear vectors */
			bom_versions.clear();
			bom_ipns.clear();
			subprj_versions.clear();
			subprj_ipns.clear();
			nboms = 0;

			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out project, finished writing");

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			if( nullptr != boms ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "Cleanup for bom strings");
				for( unsigned int i = 0; i < nbom_strs; i++ ){
					free( boms[i] );
					boms[i] = nullptr;
				}
				nbom_strs = 0;
				boms = nullptr;
			}
			show_new_proj_window = false;
			/* Clear vectors */
			bom_versions.clear();
			bom_ipns.clear();
			subprj_versions.clear();
			subprj_ipns.clear();
			nboms = 0;
		}

		ImGui::End();
	}

}

static void new_bom_window( struct dbinfo_t** info ){
	static struct bom_t * bom = NULL;

	/* Buffers for text input. Can also be used for santizing inputs */
	static char version[256] = {};
	static char name[1024] = {};
	static unsigned int nparts = 0;

	static std::vector<std::string> line_name;
	static std::vector<unsigned int> line_q;


	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("New BOM Window",&show_new_bom_window, flags ) ){
		ImGui::Text("Enter part details below");
		ImGui::Separator();

		/* Text entry fields */
		ImGui::Text("BOM Name");
		ImGui::SameLine();
		ImGui::InputText("##newbom_name", name, (sizeof(name) - 1));

		ImGui::Text("Version");
		ImGui::SameLine();
		ImGui::InputText( "##newbom_version", version, sizeof(version) - 1, ImGuiInputTextFlags_CharsNoBlank );

		ImGui::Text("Number of Line Items");
		ImGui::SameLine();
		ImGui::InputInt("##newbom_lineitems", (int *)&nparts);

		/* Figure out what size the vectors should be */
		if( nparts > 0){
			line_name.resize(nparts);
			line_q.resize(nparts);
		}

		/* Subprojects entries */
		std::vector<std::string>::iterator line_name_itr;
		std::vector<unsigned int>::iterator line_q_itr;
		line_name_itr = line_name.begin();
		line_q_itr = line_q.begin();

		/* Type will be filled out when searching for part */
		std::string line_pn_ident;
		std::string line_q_ident;

		for( unsigned int i = 0; i < nparts; i++ ){
			line_pn_ident = "##line_pn" + std::to_string(i);
			line_q_ident = "##line_q" + std::to_string(i);

			ImGui::Text("#%d", i+1);
			ImGui::Text("Manufacturer Part Number");
			ImGui::SameLine();
			ImGui::InputText(line_pn_ident.c_str(), &(*line_name_itr), ImGuiInputTextFlags_CharsNoBlank  );

			ImGui::Text("Quantity");
			ImGui::SameLine();
			ImGui::InputInt(line_q_ident.c_str(), (int *)&(*line_q_itr));

			line_name_itr++;
			line_q_itr++;

		}

		


		/* Save and cancel buttons */
		if( ImGui::Button("Save", ImVec2(0,0)) ){

			/* Check if valid to copy */
			bom = (struct bom_t *)calloc(1, sizeof(struct bom_t));
			if( nullptr == bom ){
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for saving new bom");
				show_new_bom_window = false;
				return;
			}
			/* No checks because I'll totally do it later (hopefully) */
			bom->nitems = nparts;

			bom->name = (char *)calloc( strnlen(name, sizeof(name)) + 1, sizeof(char) );
			strncpy( bom->name, name, strnlen(name, sizeof(name)) + 1 );

			bom->ver = (char *)calloc( strnlen(version, sizeof(version)) + 1 , sizeof( char ) );
			strncpy( bom->ver, version, strnlen(version, sizeof(version)) + 1 );

			/* Create new part and line item arrays */

			/* Parts */
			bom->parts = (struct part_t**)calloc( bom->nitems, sizeof( struct part_t*) );
			if( nullptr == bom->parts) {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for new bom part array");
				show_new_proj_window = false;
				return;
			}
			for( unsigned int i = 0; i < bom->nitems; i++){
				y_log_message(Y_LOG_LEVEL_DEBUG, "Get part number: %s", line_name[i].c_str());
				bom->parts[i] = get_part_from_pn( line_name[i].c_str() );
			}

			/* Line items */
			bom->line = (struct bom_line_t*)calloc( bom->nitems, sizeof( struct bom_line_t) );
			if( nullptr == bom->line) {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not allocate memory for new bom line item array");
				show_new_proj_window = false;
				return;
			}
			for( unsigned int i = 0; i < bom->nitems; i++){
				bom->line[i].type = (char *)calloc( strnlen(bom->parts[i]->type, 1024) + 1, sizeof( char ) );
				strncpy( bom->line[i].type, bom->parts[i]->type, strnlen(bom->parts[i]->type, 1024));

				bom->line[i].q = line_q[i];
				bom->line[i].ipn = bom->parts[i]->ipn;

			}

			/* Increment database information */
			mutex_spin_lock_dbinfo();
			(*info)->nbom++;
			bom->ipn = (*info)->nbom;
			/* Perform the write */
			redis_write_bom( bom );
			y_log_message(Y_LOG_LEVEL_DEBUG, "New BOM written to database");
			show_new_bom_window = false;


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
			free_bom_t( bom );
			bom = NULL;

			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out bom, finished writing");

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			show_new_bom_window = false;
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
		ImGui::InputText("##database_hostname", hostname, sizeof( hostname ), ImGuiInputTextFlags_CharsNoBlank );

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
			else if( ImGui::MenuItem("New BOM") ){
				/* Create part_t, then provide result to redis_write_part */
				y_log_message(Y_LOG_LEVEL_DEBUG, "New BOM menu clicked");
				show_new_bom_window = true;
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
				y_log_message(Y_LOG_LEVEL_DEBUG, "Edit Part menu clicked");
				show_edit_part_window = true;
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
			else if( !show_all_projects && ImGui::MenuItem("Show All Projects") ){
				show_all_projects = true;
			}
			else if( show_all_projects && ImGui::MenuItem("Hide All Projects") ){
				show_all_projects = false;
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


static void part_info_tab( struct dbinfo_t** info, class Partcache* cache ){

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
#if 0
				if( nullptr != part ){
					free_part_t( part );
					part = nullptr;
				}
				tmp = cache->read(i);
				/* Copy part to temporary storage */
				part = copy_part_t( tmp );
#endif
				part = cache->read(i);
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
					gselected_part = selected_item;
				}

//				if( nullptr != part ){
//					free_part_t( part );
//					part = nullptr;
//				}

			}
			/* Popup window for Part info */
			partinfo_window( info, selected_item );
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

static void part_data_window( struct dbinfo_t** info,  std::vector< Partcache*>* cache, int index ){
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
			part_info_tab( info, selected );
			ImGui::EndTabItem();
		}
		
		ImGui::EndTabBar();
	}

}

static void show_part_view( struct dbinfo_t** info,  std::vector< Partcache*>* cache, ImGuiTableFlags table_flags ){
	static int selected_cache = 0;

	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Part Type Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		if( nullptr != cache && DB_STAT_DISCONNECTED != db_stat ){
			show_part_select_window(info, cache, &selected_cache);
		}
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Part Type Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected part type cache info */
		if( nullptr != cache ){
			part_data_window( info, cache, selected_cache );
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
static void show_root_window( struct dbinfo_t** info, class Prjcache* prj_cache, std::vector< Partcache*>* part_cache ){

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
			show_part_view( info, part_cache, table_flags );
			break;
		case inventory_view:
			break;
		case project_view:
		default:
			show_project_view( &db_stat, info, show_all_projects, prj_cache, table_flags );
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

