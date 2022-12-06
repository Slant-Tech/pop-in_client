#include <partcache.h>
#include <imgui.h>
#include <string>
#include <cstring>
/* Private functions for operations; NOT THREAD SAVE. USE MUTEX IN CALLED
 * FUNCTION */

int Partcache::_write( struct part_t * p, unsigned int index ){
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

struct part_t* Partcache::_read( unsigned int index ){
	if( index > cache.size()-1 ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Cache index %d is larger than the cache size", index);
		return nullptr;
	}
	else {
		return cache[index];
	}
}

int Partcache::_clean( void ){
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

int Partcache::_insert( struct part_t * p, unsigned int index ){
	if( index > cache.size()+1 ){
		y_log_message(Y_LOG_LEVEL_WARNING, "Adding element to part cache that is further than one index from current size");
	}	
	auto it = cache.emplace( cache.begin() + index, p );
	y_log_message(Y_LOG_LEVEL_DEBUG, "Inserted part:%d to position %d in cache", p->ipn, index);
	return 0;
}

int Partcache::_insert_ipn( unsigned int ipn, unsigned int index ){
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

int Partcache::_append( struct part_t * p ){
	cache.push_back(p);
	y_log_message(Y_LOG_LEVEL_DEBUG, "Appended part:%s:%d in cache", type.c_str(), p->ipn );
	return 0;
}

int Partcache::_append_ipn( unsigned int ipn ){
	struct part_t* p = nullptr;
	p = get_part_from_ipn(type.c_str(), ipn);
	if( nullptr == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not add part:%s:%d to cache; database error", type.c_str(), ipn);
		return -1;
	}
	else {
		return _append(p);
	}
}

int Partcache::_remove( unsigned int index ){
	if( nullptr != cache[index] ){
		free_part_t( cache[index] );
		cache[index] = nullptr;
	}
	cache.erase( cache.begin() + index);
	return 0;
}


/* Constructor; make sure cache is created for specific size; don't allocate
 * memory, but ensure each item is NULL */
Partcache::Partcache( unsigned int size, std::string init_type ){
	const std::lock_guard<std::mutex> lock(cmtx);
	/* save the type */
	type = init_type;

	cache.assign(size, nullptr);
	selected = nullptr;
	y_log_message(Y_LOG_LEVEL_DEBUG, "Created part cache for %s", type.c_str());
}

/* Destructor; check for non null pointers, and clear them out */
Partcache::~Partcache(){
	const std::lock_guard<std::mutex> lock(cmtx);
	_clean();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Freed memory for part cache");
}

/* Return number of items in cache */
unsigned int Partcache::items(void){
	unsigned int len = 0;
	const std::lock_guard<std::mutex> lock(cmtx);
	len = cache.size();	
	return len;
}

/* Update part cache from database */
int Partcache::update( struct dbinfo_t** info ){
	const std::lock_guard<std::mutex> lock(cmtx);
//	if( cmtx.try_lock() ){
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
//				cmtx.unlock();
				return -1;
			}
		}
		/* At this point, the selected part can be updated to be the correct one
		 * based off of what was previously selected before */


		if( nullptr != (*info) ){
			unsigned int npart = 0;
			for( unsigned int i = 0; i < (*info)->nptype; i++){
				if( !strncmp( type.c_str(), (*info)->ptypes[i].name, type.size() ) ){
					npart = (*info)->ptypes[i].npart;
					y_log_message(Y_LOG_LEVEL_DEBUG, "Number of parts for type %s:%u", type.c_str(), npart);
					break;
				}
			}
			if( npart == 0 ){
//				cmtx.unlock();
				y_log_message(Y_LOG_LEVEL_WARNING, "No parts found for this (%s) part type", type.c_str());
				return -1;
			}

			/* Clear out cache since can't guarantee movement of parts, changing
			 * ipns, which parts were removed, etc. */
			_clean();
			/* Insert new elements to cache; start from index of 1 */
			for( unsigned int i = 1; i <= npart; i++ ){
				/* Internal part numbers should be contiguous... but not sure if
				 * there is a better way. */
				_append_ipn( i );
			}

			/* Recreate selected part */
			if( (unsigned int)-1 != selected_idx ){
				selected = cache[selected_idx];
			}
		}

//		cmtx.unlock();
//	}
//	else {
//		y_log_message(Y_LOG_LEVEL_ERROR, "Could not lock mutex on update");
//	}
	return 0;
}

int Partcache::write( struct part_t * p, unsigned int index ){
	const std::lock_guard<std::mutex> lock(cmtx);
	int retval = -1;
	/* Check if in bounds first */
	retval = _write( p, index );
	return retval;
}

/* Get pointer from cache; should be careful as this could have unintended
 * sideeffects if data is not copied. Will attempt without copying data first */
struct part_t* Partcache::read( unsigned int index ){
	struct part_t * p = nullptr;
	const std::lock_guard<std::mutex> lock(cmtx);
	p = _read( index );
	return p;
}

/* Add new item to cache at specified index */
int Partcache::insert( struct part_t* p, unsigned int index ){
	int retval = -1;
	const std::lock_guard<std::mutex> lock(cmtx);
	retval = _insert( p, index );	
	return retval;
}

/* Add new item to cache at specified index, while querying the database for
 * a specific part number */
int Partcache::insert_ipn( unsigned int ipn, unsigned int index ){
	int retval = -1;
	const std::lock_guard<std::mutex> lock(cmtx);
	retval = _insert_ipn( ipn, index );
	return retval;
}

int Partcache::append( struct part_t* p ){
	int retval = -1;
	const std::lock_guard<std::mutex> lock(cmtx);
	retval = _append(p);
	return retval;
}

int Partcache::append_ipn( unsigned int ipn ){
	int retval = -1;
	const std::lock_guard<std::mutex> lock(cmtx);
	retval = _append_ipn( ipn );
	return retval;
}

int Partcache::remove( unsigned int index ){
	int retval = -1;
	const std::lock_guard<std::mutex> lock(cmtx);
	retval = _remove(index);
	return retval;
}

/* Select from index or pointer */
int Partcache::select( unsigned int index ){
	const std::lock_guard<std::mutex> lock(cmtx);

	/* Clear out selected poitner */
	if( nullptr != selected ){
		free_part_t( selected );
	}	

	if( index > cache.size() ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Project selection index %d is outside of bounds of cache", index );
		selected = nullptr;
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
		return 0;
	}
}

int Partcache::select_ptr( struct part_t* p ){
	unsigned int selected_idx = (unsigned int)-1;
	const std::lock_guard<std::mutex> lock(cmtx);
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
	return 0;
}

struct part_t* Partcache::get_selected( void ){
	struct part_t* p;
	const std::lock_guard<std::mutex> lock(cmtx);
	p = selected;
	return p;
}

void Partcache::display_parts( bool* clicked ){
//	const std::lock_guard<std::mutex> lock(cmtx);

//	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow | \
									ImGuiTreeNodeFlags_OpenOnDoubleClick | \
									ImGuiTreeNodeFlags_SpanFullWidth; 

	ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_Leaf | \
				 ImGuiTreeNodeFlags_NoTreePushOnOpen | \
				 ImGuiTreeNodeFlags_SpanFullWidth;

	if( *clicked ){
		node_flags |= ImGuiTreeNodeFlags_Selected;
	}

	/* Cache header */
	ImGui::TableNextRow();
	ImGui::TableNextColumn();

	printf("type: %s\n", type.c_str());

	bool open = ImGui::TreeNodeEx(type.c_str(), node_flags);
	
	/* Check if item has been clicked */
	if( ImGui::IsItemClicked() ){
		y_log_message(Y_LOG_LEVEL_DEBUG, "Part cache: type %s clicked", type.c_str());
		*clicked = true;
	}
	else {
		*clicked = false;
	}

	ImGui::TableNextColumn();
	ImGui::Text("%d", cache.size() );
	
#if 0
	/* Display parts if node is open */
	if( open ){
		for( unsigned int i = 0; i < cache.size(); i++ ){
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			if( nullptr != cache[i]) {
				_DisplayNode( cache[i] );
			}
		}
		ImGui::TreePop();
	}

#endif
}

void Partcache::_DisplayNode( struct part_t* node ){

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
	ImGui::Text("%d", total);
}

bool Partcache::getmutex( bool blocking ) {
	if( !blocking ){
		return cmtx.try_lock();
	}
	else {
		cmtx.lock();
	}
	return true;
}

void Partcache::releasemutex( void ){
	cmtx.unlock();
}
