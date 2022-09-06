#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stdio.h>
#include <iterator>
#include <GLFW/glfw3.h>


/* Project structure */
/* Dummy file system for projects */
struct ProjectNode {
	const char* name;
	const char* date;
	const char* version;
	const char* author;
	int 		childidx;
	int			childcount;
	bool		selected;

	static void DisplayNode( ProjectNode* node, ProjectNode* all_nodes ){

		/* Getting index for node; useful for selecting only one project at a
		 * time */
		int index = std::distance( all_nodes, node );
		static int selected = 0; /* index that has been selected */

		ProjectNode* node_clicked = NULL; /* Node that has been clicked */
		ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
								   		ImGuiTreeNodeFlags_OpenOnDoubleClick | \
								   		ImGuiTreeNodeFlags_SpanFullWidth; 

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		
		/* Check if project contains subprojects */
		if( node->childcount > 0 ){

			/* Check if selected, display if selected */
			if( node->selected && (index == selected ) ){
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}

			bool open = ImGui::TreeNodeEx(node->name, node_flags);
			
			/* Check if item has been clicked */
			if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
				printf("In display: Node %s clicked\n", node->name);
				node_clicked = node;	
				selected = index;
			}


			ImGui::TableNextColumn();
			ImGui::Text(node->date);
			ImGui::TableNextColumn();
			ImGui::Text(node->version);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(node->author);
			
			/* Display subprojects if node is open */
			if( open ){
				for( int child_n = 0; child_n < node->childcount; child_n ++ ){
					DisplayNode( &all_nodes[node->childidx + child_n], all_nodes );
				}

				ImGui::TreePop();
			}

		}
		else {

			node_flags = ImGuiTreeNodeFlags_Leaf | \
						 ImGuiTreeNodeFlags_NoTreePushOnOpen | \
						 ImGuiTreeNodeFlags_SpanFullWidth;

			if( node->selected && (index == selected) ){
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::TreeNodeEx(node->name, node_flags );

			/* Check if item has been clicked */
			if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
				printf("In display: Node %s clicked\n", node->name);
				node_clicked = node;	
				selected = index;
			}

			ImGui::TableNextColumn();
			ImGui::Text(node->date);
			ImGui::TableNextColumn();
			ImGui::Text(node->version);
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(node->author);

		}

		/* Check if something was clicked */
		if( node_clicked != NULL ){
			printf("Project %s has been clicked\n", node_clicked->name);
			/* toggle state */
			node_clicked->selected = !(node_clicked->selected);
			printf("Selection state: %d\n", node_clicked->selected);
		}

	}
};

static void glfw_error_callback(int error, const char* description){
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

/* Pass structure of projects and number of projects inside of struct */
static void show_project_select_window( ProjectNode *projects);

static void show_root_window( ProjectNode *projects );

#define DEFAULT_ROOT_W	1280
#define DEFAULT_ROOT_H	720

/* Variable to continue running */
static bool run_flag = true;

int main( int, char** ){

	static ProjectNode projects[] = {
		/* Name						Date			Version	 Author		Child Index		Child Count  	Selected */
		{ "Project Top", 			"2022-05-01", 	"1.2.3", "Dylan",	1,				2,				true},
		{ "Subproject 1", 			"2022-05-02", 	"2.3.4", "Dylan",	3,				1,				false},
		{ "Subproject 2", 			"2022-05-03", 	"3.4.5", "Dylan",	1,				-1,				false},
		{ "subsubproject 1",		"2022-05-04", 	"4.5.6", "Dylan",	-1,				-1,				false}
	};

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
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	/* Set ImGui color style */
	ImGui::StyleColorsLight();
	/* Dark colors */
	ImGui::StyleColorsDark();
	/* Classic colors */
	//ImGui::StyleColorsClassic();
	
	/* Setup Backends */
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	/* Load fonts if possible  */
	ImFont* font_hack = io.Fonts->AddFontFromFileTTF( "/usr/share/fonts/TTF/Hack-Regular.ttf", 11.0f);
	IM_ASSERT( font_hack != NULL );
	if( font_hack == NULL ){
		io.Fonts->AddFontDefault();
	}

	/* Setup colors */
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	/* Main viewport */
	const ImGuiViewport* main_viewport = ImGui::GetMainViewport();

	bool bool_root_window = true;
	ImGuiWindowFlags root_window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize
									   | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar;
	/* Main application loop */
	while( !glfwWindowShouldClose(window) ) {

		/* Poll and handle events */
		glfwPollEvents();

		if( !run_flag ){
			printf("Quit button pressed\n");
			glfwSetWindowShouldClose(window, true);
		}
		/* Start ImGui frame */
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		/* Setup root window */
		ImGui::SetNextWindowPos(ImVec2(main_viewport->WorkPos.x, main_viewport->WorkPos.y));
		ImGui::SetNextWindowSize(ImVec2(DEFAULT_ROOT_W, DEFAULT_ROOT_H));
		ImGui::Begin("Root", &bool_root_window, root_window_flags);
		show_root_window(projects);
		ImGui::End();

		/* End Projects view creation */


		/* Rendering section */
		ImGui::Render();
		int display_w, display_h;
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


}



static void show_project_select_window( ProjectNode *projects){
	
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
		/* First column will use default _WidthStrecth when ScrollX is
		 * off and _WidthFixed when ScrollX is on */
		ImGui::TableSetupColumn("Name",   	ImGuiTableColumnFlags_NoHide);
		ImGui::TableSetupColumn("Date",   	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Version",	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 12.0f);
		ImGui::TableSetupColumn("Author", 	ImGuiTableColumnFlags_WidthFixed, TEXT_BASE_WIDTH * 18.0f);
		ImGui::TableHeadersRow();


		ProjectNode::DisplayNode( &projects[0], projects );
		ImGui::EndTable();
	}

}

/* Setup root window, child windows */
static void show_root_window( ProjectNode *projects ){
	
	/* Create menu items */
	if( ImGui::BeginMenuBar() ){
		/* File Menu */
		if( ImGui::BeginMenu("File") ) {
			if( ImGui::MenuItem("New Project") ){
				
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
	/* Show project view */
	ImGui::BeginChild("Project Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.3f, ImGui::GetContentRegionAvail().y * 0.8f ));
	show_project_select_window(projects);
	ImGui::EndChild();

}
