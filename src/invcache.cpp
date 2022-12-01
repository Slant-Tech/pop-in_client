#include <invcache.h>
#include <imgui.h>
#include <string>
#include <cstring>
/* Private functions for operations; NOT THREAD SAVE. USE MUTEX IN CALLED
 * FUNCTION */

int Invcache::_write( struct part_t * p, unsigned int index ){
	if( index > cache.size()-1 ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Cache index %d is larger than the cache size", index);
		return -1;
	}
	else{
		/* Write part pointer to cache */

		/* Check if overwritting data vs new allocation */
		if( nullptr != cache[index] ){
			/* Data exists, should free */
			free_part_t( cache[index] );
			cache[index] = nullptr;
		}
		cache[index] = p;
		
		y_log_message( Y_LOG_LEVEL_DEBUG, "Wrote part ipn %d to cache at index %d", p->ipn, index );

		return 0;
	}
}

struct part_t* Invcache::_read( unsigned int index ){
	if( index > cache.size()-1 ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Cache index %d is larger than the cache size", index);
		return nullptr;
	}
	else {
		return cache[index];
	}
}

int Invcache::_clean( void ){
	const std::lock_guard<std::mutex> lock(clean_mtx);
	/* Clear out selected poitner */
	if( nullptr != selected ){
		/* Will be freed soon, just need to make sure the pointer goes nowhere */
		selected = nullptr;
	}
	for( unsigned int i = 0; i < cache.size(); i++ ){
		if( nullptr != cache[i] ){
			free_part_t( cache[i] );
			cache[i] = nullptr;
		}
	}
	/* Empty the vector, close it out to 0 elements */
	cache.clear();


	return 0;
}

int Invcache::_insert( struct part_t * p, unsigned int index ){
	if( index > cache.size()+1 ){
		y_log_message(Y_LOG_LEVEL_WARNING, "Adding element to part cache that is further than one index from current size");
	}	
	auto it = cache.emplace( cache.begin() + index, p );
	y_log_message(Y_LOG_LEVEL_DEBUG, "Inserted part:%d to position %d in cache", p->ipn, index);
	return 0;
}

int Invcache::_insert_ipn( unsigned int ipn, unsigned int index ){
	struct part_t* p = nullptr;
	p = get_part_from_ipn(type.c_str(), ipn);
	if( nullptr == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not add part:%d to cache; database error", ipn);
		return -1;
	}
	else {
		return _insert(p, index);
	}
}

int Invcache::_append( struct part_t * p ){
	cache.push_back(p);
	y_log_message(Y_LOG_LEVEL_DEBUG, "Appended part:%d in cache", p->ipn );
	return 0;
}

int Invcache::_append_ipn( unsigned int ipn ){
	struct part_t* p = nullptr;
	p = get_part_from_ipn(type.c_str(), ipn);
	if( nullptr == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not add part:%d to cache; database error", ipn);
		return -1;
	}
	else {
		return _append(p);
	}
}

int Invcache::_remove( unsigned int index ){
	if( nullptr != cache[index] ){
		free_part_t( cache[index] );
		cache[index] = nullptr;
	}
	cache.erase( cache.begin() + index);
	return 0;
}


/* Constructor; make sure cache is created for specific size; don't allocate
 * memory, but ensure each item is NULL */
Invcache::Invcache( unsigned int size, std::string init_type ){
	cmtx.lock();
	/* save the type */
	type = init_type;

	cache.assign(size, nullptr);
	selected = nullptr;
	cmtx.unlock();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Created part cache");
}

/* Destructor; check for non null pointers, and clear them out */
Invcache::~Invcache(){
	cmtx.lock();
	_clean();
	cmtx.unlock();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Freed memory for part cache");
}

/* Return number of items in cache */
unsigned int Invcache::items(void){
	unsigned int len = 0;
	cmtx.lock();
	len = cache.size();	
	cmtx.unlock();
	return len;
}

/* Update part cache from database */
int Invcache::update( struct dbinfo_t** info ){
	cmtx.lock();
	/* Save current selected part index to use later */
	unsigned int selected_idx = (unsigned int)-1;
	/* Look through cache to find where pointer matches in vector */
	if( nullptr != selected ){
		for( unsigned int i = 0; i < cache.size(); i++ ){
			if( selected == cache[i] ){
				y_log_message( Y_LOG_LEVEL_DEBUG, "Found selected part index in cache at %d", i );
				selected_idx = i;
				break;
			}
		}
		if( selected_idx == (unsigned int)-1){
			y_log_message( Y_LOG_LEVEL_WARNING, "Could not find selected index from part pointer during update. Could mean that selected part is a subpart." );
			cmtx.unlock();
			return -1;
		}
	}
	/* At this point, the selected part can be updated to be the correct one
	 * based off of what was previously selected before */

	unsigned int nprj = 0;
	/* Get updated database information */

	if( mutex_lock_dbinfo() == 0 ){
		if( nullptr != (*info) ){
			/* free existing data */
			free_dbinfo_t( (*info) );
			free( *info );
		}
		(*info) = redis_read_dbinfo();
		if( nullptr != (*info) ){
			nprj = (*info)->nprj;

			/* Clear out cache since can't guarantee movement of parts, changing
			 * ipns, which parts were removed, etc. */
			_clean();
			/* Insert new elements to cache; start from index of 1 */
			for( unsigned int i = 1; i <= nprj; i++ ){
				/* Internal part numbers should be contiguous... but not sure if
				 * there is a better way. */
				_append_ipn( i );
			}

			/* Recreate selected part */
			if( (unsigned int)-1 != selected_idx ){
				selected = cache[selected_idx];
			}
		}
		
		mutex_unlock_dbinfo();
	}

	else {
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not read database info" );
		selected = nullptr;
		return -1;
	}


	cmtx.unlock();
	return 0;
}

int Invcache::write( struct part_t * p, unsigned int index ){
	int retval = -1;
	/* Check if in bounds first */
	cmtx.lock();
	retval = _write( p, index );
	cmtx.unlock();
	return retval;
}

/* Get pointer from cache; should be careful as this could have unintended
 * sideeffects if data is not copied. Will attempt without copying data first */
struct part_t* Invcache::read( unsigned int index ){
	struct part_t * p = nullptr;
	cmtx.lock();
	p = _read( index );
	cmtx.unlock();
	return p;
}

/* Add new item to cache at specified index */
int Invcache::insert( struct part_t* p, unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _insert( p, index );	
	cmtx.unlock();
	return retval;
}

/* Add new item to cache at specified index, while querying the database for
 * a specific part number */
int Invcache::insert_ipn( unsigned int ipn, unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _insert_ipn( ipn, index );
	cmtx.unlock();
	return retval;
}

int Invcache::append( struct part_t* p ){
	int retval = -1;
	cmtx.lock();
	retval = _append(p);
	cmtx.unlock();
	return retval;
}

int Invcache::append_ipn( unsigned int ipn ){
	int retval = -1;
	cmtx.lock();
	retval = _append_ipn( ipn );
	cmtx.unlock();
	return retval;
}

int Invcache::remove( unsigned int index ){
	int retval = -1;
	cmtx.lock();
	retval = _remove(index);
	cmtx.unlock();
	return retval;
}

/* Select from index or pointer */
int Invcache::select( unsigned int index ){
	cmtx.lock();

	/* Clear out selected poitner */
	if( nullptr != selected ){
		free_part_t( selected );
	}	

	if( index > cache.size() ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Project selection index %d is outside of bounds of cache", index );
		selected = nullptr;
		cmtx.unlock();
		return -1;
	}
	else {
#if 0
		selected = (struct part_t *)calloc( 1, sizeof( struct part_t ) );
		if( nullptr == selected ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for selected part" );
			selected = nullptr;
			cmtx.unlock();
			return -1;
		}

		memcpy( selected, cache[index], sizeof( *cache[index] ) );
		y_log_message( Y_LOG_LEVEL_DEBUG, "Copied part data to selected part");
#else 
		selected = cache[index];
#endif
		cmtx.unlock();
		return 0;
	}
}

int Invcache::select_ptr( struct part_t* p ){
	unsigned int selected_idx = (unsigned int)-1;
	cmtx.lock();
	/* Look through cache to find where pointer matches in vector */
	for( unsigned int i = 0; i < cache.size(); i++ ){
		if( p == cache[i] ){
			y_log_message( Y_LOG_LEVEL_DEBUG, "Found part index in cache at %d", i );
			selected_idx = i;
			break;
		}
	}
	if( selected_idx == (unsigned int)-1){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not find selected index from part pointer" );
		return -1;
		cmtx.unlock();
	}

#if 0
	selected = (struct part_t *)calloc( 1, sizeof( struct part_t ) );
	if( nullptr == selected ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for selected part" );
		selected = nullptr;
		cmtx.unlock();
		return -1;
	}

	memcpy( selected, cache[selected_idx], sizeof( *cache[selected_idx] ) );
	y_log_message( Y_LOG_LEVEL_DEBUG, "Copied part data to selected part");
#else
	selected = cache[selected_idx];
#endif
	cmtx.unlock();
	return 0;
}

struct part_t* Invcache::get_selected( void ){
	struct part_t* p;
	cmtx.lock();
	p = selected;
	cmtx.unlock();
	return p;
}

void Invcache::display_parts( void ){

	cmtx.lock();

	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
									ImGuiTreeNodeFlags_OpenOnDoubleClick | \
									ImGuiTreeNodeFlags_SpanFullWidth; 
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	/* Cache header */

	bool open = ImGui::TreeNodeEx(type.c_str(), node_flags);
	
	/* Check if item has been clicked */
	if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
		y_log_message(Y_LOG_LEVEL_DEBUG, "Part cache: type %s clicked", type.c_str());
	}
	
	/* Display parts if node is open */
	if( open ){
		for( unsigned int i = 0; i < cache.size(); i++ ){
			_DisplayNode( cache[i] );
		}
		ImGui::TreePop();
	}

	cmtx.unlock();
}

void Invcache::_DisplayNode( struct part_t* node ){

	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | \
				 ImGuiTreeNodeFlags_NoTreePushOnOpen | \
				 ImGuiTreeNodeFlags_SpanFullWidth;
	
	if( node == selected ){
		node_flags |= ImGuiTreeNodeFlags_Selected;
	}
	
	ImGui::TreeNodeEx(node->mpn, node_flags );
	
	/* Check if item has been clicked */
	if( ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen() ){
		y_log_message(Y_LOG_LEVEL_DEBUG, "Part cache: Node %s clicked", node->mpn);
		selected = node;
	}
	
	unsigned int total = 0;
	for( unsigned int i = 0; i < node->inv_len; i++ ){
		total += node->inv[i].q;	
	}

	ImGui::TableNextColumn();

	/* Get text strings for part status */
	switch ( node->status ) {
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


	ImGui::TableNextColumn();
//	ImGui::TextUnformatted("%d", total);
	ImGui::Text("%d", total);


}

