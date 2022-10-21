#include <prjcache.h>
#include <imgui.h>
#include <cstring>
/* Private functions for operations; NOT THREAD SAVE. USE MUTEX IN CALLED
 * FUNCTION */

int Prjcache::_write( struct proj_t * p, unsigned int index ){
	if( index > cache.size()-1 ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Cache index %d is larger than the cache size", index);
		return -1;
	}
	else{
		/* Write project pointer to cache */

		/* Check if overwritting data vs new allocation */
		if( nullptr != cache[index] ){
			/* Data exists, should free */
			free_proj_t( cache[index] );
			cache[index] = nullptr;
		}
		cache[index] = p;
		
		y_log_message( Y_LOG_LEVEL_DEBUG, "Wrote project ipn %d to cache at index %d", p->ipn, index );

		return 0;
	}
}

struct proj_t* Prjcache::_read( unsigned int index ){
	if( index > cache.size()-1 ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Cache index %d is larger than the cache size", index);
		return nullptr;
	}
	else {
		return cache[index];
	}
}

int Prjcache::_clean( void ){
	const std::lock_guard<std::mutex> lock(clean_mtx);
	/* Clear out selected poitner */
	if( nullptr != selected ){
		/* Will be freed soon, just need to make sure the pointer goes nowhere */
		selected = nullptr;
	}
	for( unsigned int i = 0; i < cache.size(); i++ ){
		if( nullptr != cache[i] ){
			free_proj_t( cache[i] );
			cache[i] = nullptr;
		}
	}
	/* Empty the vector, close it out to 0 elements */
	cache.clear();


	return 0;
}

int Prjcache::_insert( struct proj_t * p, unsigned int index ){
	if( index > cache.size()+1 ){
		y_log_message(Y_LOG_LEVEL_WARNING, "Adding element to project cache that is further than one index from current size");
	}	
	auto it = cache.emplace( cache.begin() + index, p );
	y_log_message(Y_LOG_LEVEL_DEBUG, "Inserted project:%d to position %d in cache", p->ipn, index);
	return 0;
}

int Prjcache::_insert_ipn( unsigned int ipn, unsigned int index ){
	struct proj_t* p = nullptr;
	p = get_proj_from_ipn(ipn);
	if( nullptr == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not add project:%d to cache; database error", ipn);
		return -1;
	}
	else {
		return _insert(p, index);
	}
}

int Prjcache::_append( struct proj_t * p ){
	cache.push_back(p);
	y_log_message(Y_LOG_LEVEL_DEBUG, "Appended project:%d in cache", p->ipn );
	return 0;
}

int Prjcache::_append_ipn( unsigned int ipn ){
	struct proj_t* p = nullptr;
	p = get_proj_from_ipn(ipn);
	if( nullptr == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not add project:%d to cache; database error", ipn);
		return -1;
	}
	/* Don't add cache for non top level, as it will make things more difficult */
	else if( p->nsub == 0 ){
		/* Free allocated project */
		free_proj_t( p );
		return 0;
	}
	else {
		return _append(p);
	}
}

int Prjcache::_remove( unsigned int index ){
	if( nullptr != cache[index] ){
		free_proj_t( cache[index] );
		cache[index] = nullptr;
	}
	cache.erase( cache.begin() + index);
	return 0;
}


/* Constructor; make sure cache is created for specific size; don't allocate
 * memory, but ensure each item is NULL */
Prjcache::Prjcache( unsigned int size ){
	cmtx.lock();
	cache.assign(size, nullptr);
	selected = nullptr;
	cmtx.unlock();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Created project cache");
}

/* Destructor; check for non null pointers, and clear them out */
Prjcache::~Prjcache(){
	cmtx.lock();
	_clean();
	cmtx.unlock();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Freed memory for project cache");
}

/* Return number of items in cache */
unsigned int Prjcache::items(void){
	unsigned int len = 0;
	cmtx.lock();
	len = cache.size();	
	cmtx.unlock();
	return len;
}

/* Update project cache from database */
int Prjcache::update( struct dbinfo_t* info ){
	cmtx.lock();
	/* Save current selected project index to use later */
	unsigned int selected_idx = (unsigned int)-1;
	/* Look through cache to find where pointer matches in vector */
	if( nullptr != selected ){
		for( unsigned int i = 0; i < cache.size(); i++ ){
			if( selected == cache[i] ){
				y_log_message( Y_LOG_LEVEL_DEBUG, "Found selected project index in cache at %d", i );
				selected_idx = i;
				break;
			}
		}
		if( selected_idx == (unsigned int)-1){
			y_log_message( Y_LOG_LEVEL_WARNING, "Could not find selected index from project pointer during update. Could mean that selected project is a subproject." );
			cmtx.unlock();
			return -1;
		}
	}
	/* At this point, the selected project can be updated to be the correct one
	 * based off of what was previously selected before */

	unsigned int nprj = 0;
	/* Get updated database information */
	if( !redis_read_dbinfo( info ) ){
		nprj = info->nprj;

		/* Clear out cache since can't guarantee movement of projects, changing
		 * ipns, which projects were removed, etc. */
		_clean();
		/* Insert new elements to cache; start from index of 1 */
		for( unsigned int i = 1; i <= nprj; i++ ){
			/* Internal part numbers should be contiguous... but not sure if
			 * there is a better way. */
			_append_ipn( i );
		}

		/* Recreate selected project */
		if( (unsigned int)-1 != selected_idx ){
			selected = cache[selected_idx];
			selected->selected = true;
		}
		
	}

	else {
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not read database info" );
		selected = nullptr;
		return -1;
	}


	cmtx.unlock();
	return 0;
}

int Prjcache::write( struct proj_t * p, unsigned int index ){
	int retval = -1;
	/* Check if in bounds first */
	cmtx.lock();
	retval = _write( p, index );
	cmtx.unlock();
	return retval;
}

/* Get pointer from cache; should be careful as this could have unintended
 * sideeffects if data is not copied. Will attempt without copying data first */
struct proj_t* Prjcache::read( unsigned int index ){
	struct proj_t * p = nullptr;
	cmtx.lock();
	p = _read( index );
	cmtx.unlock();
	return p;
}

/* Add new item to cache at specified index */
int Prjcache::insert( struct proj_t* p, unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _insert( p, index );	
	cmtx.unlock();
	return retval;
}

/* Add new item to cache at specified index, while querying the database for
 * a specific part number */
int Prjcache::insert_ipn( unsigned int ipn, unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _insert_ipn( ipn, index );
	cmtx.unlock();
	return retval;
}

int Prjcache::append( struct proj_t* p ){
	int retval = -1;
	cmtx.lock();
	retval = _append(p);
	cmtx.unlock();
	return retval;
}

int Prjcache::append_ipn( unsigned int ipn ){
	int retval = -1;
	cmtx.lock();
	retval = _append_ipn( ipn );
	cmtx.unlock();
	return retval;
}

int Prjcache::remove( unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _remove(index);
	cmtx.unlock();
	return retval;
}

/* Select from index or pointer */
int Prjcache::select( unsigned int index ){
	cmtx.lock();

	/* Clear out selected poitner */
	if( nullptr != selected ){
		free_proj_t( selected );
	}	

	if( index > cache.size() ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Project selection index %d is outside of bounds of cache", index );
		selected = nullptr;
		cmtx.unlock();
		return -1;
	}
	else {
#if 0
		selected = (struct proj_t *)calloc( 1, sizeof( struct proj_t ) );
		if( nullptr == selected ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for selected project" );
			selected = nullptr;
			cmtx.unlock();
			return -1;
		}

		memcpy( selected, cache[index], sizeof( *cache[index] ) );
		y_log_message( Y_LOG_LEVEL_DEBUG, "Copied project data to selected project");
#else 
		selected = cache[index];
#endif
		cmtx.unlock();
		return 0;
	}
}

int Prjcache::select_ptr( struct proj_t* p ){
	unsigned int selected_idx = (unsigned int)-1;
	cmtx.lock();
	/* Look through cache to find where pointer matches in vector */
	for( unsigned int i = 0; i < cache.size(); i++ ){
		if( p == cache[i] ){
			y_log_message( Y_LOG_LEVEL_DEBUG, "Found project index in cache at %d", i );
			selected_idx = i;
			break;
		}
	}
	if( selected_idx == (unsigned int)-1){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not find selected index from project pointer" );
		return -1;
		cmtx.unlock();
	}

#if 0
	selected = (struct proj_t *)calloc( 1, sizeof( struct proj_t ) );
	if( nullptr == selected ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for selected project" );
		selected = nullptr;
		cmtx.unlock();
		return -1;
	}

	memcpy( selected, cache[selected_idx], sizeof( *cache[selected_idx] ) );
	y_log_message( Y_LOG_LEVEL_DEBUG, "Copied project data to selected project");
#else
	selected = cache[selected_idx];
#endif
	cmtx.unlock();
	return 0;
}

struct proj_t* Prjcache::get_selected( void ){
	struct proj_t* p;
	cmtx.lock();
	p = selected;
	cmtx.unlock();
	return p;
}

void Prjcache::display_projects( void ){
	cmtx.lock();
	for( unsigned int i = 0; i < cache.size(); i++ ){
//		y_log_message(Y_LOG_LEVEL_DEBUG, "Displaying top project %d", i);
		_DisplayNode( cache[i] );
	}

	cmtx.unlock();
}

void Prjcache::_DisplayNode( struct proj_t* node ){
	
	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
									ImGuiTreeNodeFlags_OpenOnDoubleClick | \
									ImGuiTreeNodeFlags_SpanFullWidth; 

	ImGui::TableNextRow();
	ImGui::TableNextColumn();
	
	/* Check if project contains subprojects */
	if( node->nsub > 0 ){

		/* Check if selected, display if selected */
		if( node->selected && (node == selected ) ){
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}
		else {
			node_flags &= ~ImGuiTreeNodeFlags_Selected;		
		}

		bool open = ImGui::TreeNodeEx(node->name, node_flags);
		
		/* Check if item has been clicked */
		if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
			selected = node;
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
				/* If node is stale, wait for it to be fixed first before
				 * displaying */
				_DisplayNode( node->sub[i].prj );
			}
			ImGui::TreePop();
		}
	}
	else {

		node_flags = ImGuiTreeNodeFlags_Leaf | \
					 ImGuiTreeNodeFlags_NoTreePushOnOpen | \
					 ImGuiTreeNodeFlags_SpanFullWidth;

		if( node->selected && (node == selected) ){
			node_flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::TreeNodeEx(node->name, node_flags );

		/* Check if item has been clicked */
		if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "In display: Node %s clicked", node->name);
			selected = node;
			node->selected = true;
		}

		ImGui::TableNextColumn();
		ImGui::Text( asctime( localtime(&node->time_created)) ); /* Convert time_t to localtime */
		ImGui::TableNextColumn();
		ImGui::Text(node->ver);
		ImGui::TableNextColumn();
		ImGui::TextUnformatted(node->author);

	}

}

