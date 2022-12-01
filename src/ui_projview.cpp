#include <ui_projview.h>
#include <dbstat_def.h>
#include <implot.h>

#define PARTINFO_SPACING	200

static void show_project_select_window( int* db_stat, bool show_all_projects, class Prjcache* cache );
static void proj_data_window( struct dbinfo_t** info, class Prjcache* cache );
static void proj_info_tab( struct dbinfo_t** info, struct proj_t* prj, int* bom_index );
static void proj_bom_tab( struct dbinfo_t** info, struct bom_t* bom );

void show_project_view( int * db_stat, struct dbinfo_t** info, bool show_all_projects, class Prjcache* cache, ImGuiTableFlags table_flags ){
	if( ImGui::BeginTable("view_split", 2, table_flags) ){
		/* get lock for project cache */
		cache->getmutex(true);
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
		cache->releasemutex();
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
		if(( cache->get_selected()->nboms > 0) && ImGui::BeginTabItem("BOM") ){
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
	static unsigned int nunique = 0;
	static unsigned int nunits = 1;
	static unsigned int last_nunits = 0;
	static double cost = 0.0;
	static double optimal_cost = 0.0;
	static double optimal_cost_per = 0.0;
	static double savings = 0.0;
	static double savings_per = 0.0;
	static unsigned int last_prjipn = 0;
	static part_t** limiting_parts = nullptr;
	static int part_status_count[pstat_total] = {0};
	static const double part_status_positions[pstat_total] = {
														pstat_unknown,
														pstat_prod,
														pstat_low_stock,
														pstat_unavailable,
														pstat_nrnd,
														pstat_lasttimebuy,
														pstat_obsolete
													};
	static const char* part_status_str[pstat_total] = { "Unknown",
														"Production",
														"Low Stock",
														"Unavailable",
														"NRND",
														"Last Time Buy",
														"Obsolete"
													};

	/* Check if already retrieved data from this project */
	if( last_prjipn != prj->ipn ){
//		printf("Different project than last time; update values\n");
		y_log_message( Y_LOG_LEVEL_DEBUG, "Different project than last time, update values" );
		nitems = get_num_all_proj_items( prj );
		nunique = get_num_all_uniq_proj_items( prj );
		/* Force update of price calculation */
		last_nunits = 0;
		nunits = 1;
#if 0
		if( limiting_parts != nullptr ){
			free( limiting_parts );
			limiting_parts = nullptr;
		}
		limiting_parts = get_proj_prod_lim_factor( prj );
#endif
		last_prjipn = prj->ipn;
	}
	if( nunits != last_nunits ){
		y_log_message( Y_LOG_LEVEL_DEBUG, "Updating cost" );
//		printf("Updating cost\n");
		optimal_cost = get_optimal_project_cost( prj, nunits );
		cost = get_exact_project_cost( prj, nunits );
		savings = cost - optimal_cost;
		optimal_cost_per = optimal_cost / (double)nunits;
		savings_per = (cost / (double)(nunits) ) - optimal_cost_per;

		for( unsigned int i = 0; i < (unsigned int)pstat_total; i++ ){
			part_status_count[i] = get_num_proj_partstatus( prj, (enum part_status_t)i  );
			printf("Part status count for %s: %u\n", part_status_str[i], part_status_count[i] );
		}

		last_nunits = nunits;
	}

	/* Show information about project with selections for versions etc */
	ImGui::Text("Project Information Tab");
	ImGui::Separator();
	
	ImGui::Text("Project Part Number: %s", prj->pn);

	/* Project Version */
	ImGui::Text("Project Version: %s", prj->ver);

	/* Number of units to get */
	ImGui::Text("Number of units");
	ImGui::SameLine();
	ImGui::InputInt("##project_units", (int *)&nunits );
	
	ImGui::Spacing();
	ImGui::Text("Project Bill of Materials ");
	/* BOM Selection */
	if( prj->nboms > 0 ){
		const char* default_bom_name = prj->boms[*bom_index].bom->name;
		if( ImGui::BeginCombo("##selprj_bom", default_bom_name ) ){
			for( unsigned int i = 0; i < prj->nboms; i++ ){
				const bool is_selected = (*bom_index == i);
				if( ImGui::Selectable( prj->boms[i].bom->name, is_selected ) ){
					*bom_index = i;
				}
				if( is_selected ){
					ImGui::SetItemDefaultFocus();
				}
			}	

			ImGui::EndCombo();
		}
	}
    static ImPlotPieChartFlags pie_flags = 0;
	ImGui::Spacing();
	ImGui::Text("Part Status Distribution");
	if( ImPlot::BeginPlot("##part_status_piechart", ImVec2(300, 150),ImPlotFlags_NoMouseText)){
//		ImPlot::SetupLegend(ImPlotLocation_West, ImPlotLegendFlags_Outside);
//		ImPlot::SetupAxes( NULL, NULL, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_NoDecorations);
//		ImPlot::SetupAxesLimits(0, 1, 0, 1);
//		ImPlot::PlotPieChart( part_status_str, part_status_count, pstat_total, 0.5, 0.5, 0.4, "%.0f", 90, pie_flags);
		ImPlot::SetupAxes("#","Status",ImPlotAxisFlags_AutoFit,ImPlotAxisFlags_AutoFit);
		ImPlot::SetupAxisTicks(ImAxis_Y1, part_status_positions, (int)pstat_total, part_status_str);
		ImPlot::PlotBars("##part_status_plot", part_status_count, (int)pstat_total, 0.4, 0, ImPlotBarsFlags_Horizontal );
		ImPlot::EndPlot();
	}



	ImGui::Text("Number of unique parts in Project: %d", nunique);
	ImGui::Text("Number of total parts in Project: %d", nitems);
	ImGui::Text("Total Optimal Cost for %d units: %.2lf", nunits, optimal_cost);
	ImGui::Text("Total Cost Optimization Savings: %.2lf", savings);

	ImGui::Text("Optimal Cost Per Unit: %.4lf", optimal_cost_per );
	ImGui::Text("Cost Optimization Savings Per Unit: %.4lf", savings_per );

#if 0
	if( nullptr != limiting_parts ){
		ImGui::Text("Number of parts with insufficient inventory: %d", sizeof( limiting_parts ) / sizeof( struct part_t * ) );
	}
#endif
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
#if defined(__APPLE__)
			bool* item_sel = (bool*) calloc( bom->nitems, sizeof( bool ) );
#else
			bool item_sel[ bom->nitems ] = {};
#endif
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
				case pstat_lasttimebuy:
					ImGui::Text("Last Time Buy");
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
