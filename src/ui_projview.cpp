#include <ui_projview.h>
#include <dbstat_def.h>

#define PARTINFO_SPACING	200

static void show_project_select_window( int* db_stat, bool show_all_projects, class Prjcache* cache );
static void proj_data_window( struct dbinfo_t** info, class Prjcache* cache );
static void proj_info_tab( struct dbinfo_t** info, struct proj_t* prj, int* bom_index );
static void proj_bom_tab( struct dbinfo_t** info, struct bom_t* bom );

void show_project_view( int * db_stat, struct dbinfo_t** info, bool show_all_projects, class Prjcache* cache, ImGuiTableFlags table_flags ){
	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		ImGui::TableNextRow();

		/* Project view on the left */
		ImGui::TableSetColumnIndex(0);
		/* Show project view */
		ImGui::BeginChild("Project Selector", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y * 0.95f ));
		if( (nullptr != cache) && DB_STAT_DISCONNECTED != *db_stat ){
			show_project_select_window( db_stat, show_all_projects, cache);
		}
		ImGui::EndChild();

		/* Info view on the right */
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("Project Information", ImVec2(ImGui::GetContentRegionAvail().x * 0.95f, ImGui::GetContentRegionAvail().y*0.95f));

		/* Get selected project */
		if( nullptr != cache->get_selected() ){
			proj_data_window( info, cache );
		} 

		ImGui::EndChild();
		ImGui::EndTable();	
	}

}


static void show_project_select_window( int* db_stat, bool show_all_projects, class Prjcache* cache ){
	
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


		if( DB_STAT_DISCONNECTED != *db_stat ){
			cache->display_projects(show_all_projects);
		}
		ImGui::EndTable();
	}

}


static void proj_data_window( struct dbinfo_t** info, class Prjcache* cache ){
	static int bom_index = 0;
	static bom_t* bom = nullptr;
	/* Show BOM/Information View */
	ImGuiTabBarFlags tabbar_flags = ImGuiTabBarFlags_None;
	if( ImGui::BeginTabBar("Project Info", tabbar_flags ) ){
		if( ImGui::BeginTabItem("Info") ){
			proj_info_tab( info, cache->get_selected(), &bom_index );
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
			proj_bom_tab( info, bom );
			ImGui::EndTabItem();
		}
		
		ImGui::EndTabBar();
	}

}


static void proj_info_tab( struct dbinfo_t** info, struct proj_t* prj, int* bom_index ){

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

static void proj_bom_tab( struct dbinfo_t** info, struct bom_t* bom ){

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

			partinfo_window( info, selected_item);
			
		}
		else{
			y_log_message(Y_LOG_LEVEL_ERROR, "Issue getting bom for project bom tab");
		}
		ImGui::EndTable();
	}

}

void partinfo_window( struct dbinfo_t** info, struct part_t* selected_item){
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
				ImGui::Text("%s", (*info)->invs[selected_item->inv[i].loc].name);	
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
