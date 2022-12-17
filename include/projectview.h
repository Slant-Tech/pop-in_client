/*
 * 	SPDX-FileCopyrightText: 2022 Slant Tech, Dylan Wadler <dwadler@slant.tech>
 *  
 * 	SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef POPIN_PROJECTVIEW_H
#define POPIN_PROJECTVIEW_H

#include <imgui.h>
#include <stdio.h>
#include <vector>
#include <iterator>
#include <yder.h>

extern int selected_prj;

/* Project structure */
/* Dummy file system for projects */
struct ProjectNode {
	const char* name;
	const char* date;
	const char* version;
	const char* author;
	int			bomid;
	int 		childidx;
	int			childcount;
	bool		selected;

	void DisplayNode( ProjectNode* node, ProjectNode* all_nodes ){

		/* Getting index for node; useful for selecting only one project at a
		 * time */
		int index = std::distance( all_nodes, node );

		ProjectNode* node_clicked = NULL; /* Node that has been clicked */
		ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
								   		ImGuiTreeNodeFlags_OpenOnDoubleClick | \
								   		ImGuiTreeNodeFlags_SpanFullWidth; 

		ImGui::TableNextRow();
		ImGui::TableNextColumn();
		
		/* Check if project contains subprojects */
		if( node->childcount > 0 ){

			/* Check if selected, display if selected */
			if( node->selected && (index == selected_prj ) ){
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}

			bool open = ImGui::TreeNodeEx(node->name, node_flags);
			
			/* Check if item has been clicked */
			if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
				node_clicked = node;	
				selected_prj = index;
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

			if( node->selected && (index == selected_prj) ){
				node_flags |= ImGuiTreeNodeFlags_Selected;
			}

			ImGui::TreeNodeEx(node->name, node_flags );

			/* Check if item has been clicked */
			if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
				node_clicked = node;	
				selected_prj = index;
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
			y_log_message(Y_LOG_LEVEL_DEBUG, "Project %s has been clicked", node_clicked->name);
			/* toggle state */
			node_clicked->selected = !(node_clicked->selected);
			y_log_message(Y_LOG_LEVEL_DEBUG, "Selection state: %d", node_clicked->selected);

			if( node_clicked->selected ){
				y_log_message(Y_LOG_LEVEL_DEBUG, "Node %s is highlighted", node->name);
			}
			else {
				/* Don't show anything if there is no highlight; second
				 * thought, maybe not a good idea?  */
				//selected_prj = -1;	
			}
		}

	}
};



#endif /* POPIN_PROJECTVIEW_H */
