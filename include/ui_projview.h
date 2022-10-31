#ifndef UI_PROJVIEW_H
#define UI_PROJVIEW_H

#include <time.h>
#include <vector>
#include <imgui.h>
#include <yder.h>
#include <db_handle.h>
#include <misc/cpp/imgui_stdlib.h>
#include <prjcache.h>
#include <proj_funct.h>

void show_project_view( int * db_stat, struct dbinfo_t** info, bool show_all_projects, class Prjcache* cache, ImGuiTableFlags table_flags );
void partinfo_window( struct dbinfo_t** info, struct part_t* selected_item);

#endif /* UI_PROJVIEW_H */
