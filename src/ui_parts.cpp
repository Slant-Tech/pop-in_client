#include <ui_parts.h>

#define NEWPART_SPACING		250

void new_part_window( bool* show, struct dbinfo_t** info ){
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
	const char* status_options[7] = {
			"Unknown",
			"Production",
			"Low Stock",
			"Unavailable",
			"NRND",
			"Last Time Buy",
			"Obsolete"
	};
	static const char* selection_str = NULL;

	if( !(selection_idx >= 0 && selection_idx < pstat_total) ){
		selection_idx = 0;
	}
	selection_str = status_options[selection_idx];

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("New Part Popup", show, flags ) ){
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

			ImGui::InputInt(price_q_ident.c_str(), &(*price_q_itr));
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputDouble(price_cost_ident.c_str(), &(*price_cost_itr));

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
			ImGui::InputInt(inv_amount_ident.c_str(), (int*)&(*inv_amount_itr));

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
				part.status = (enum part_status_t)selection_idx;


				/* Store variable data information */
				part.info_len = ninfo;
				part.info = (struct part_info_t *)calloc( part.info_len, sizeof( struct part_info_t ) );
				if( nullptr == part.info ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info array in new part");
					free_part_addr_t( &part );
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.info_len; i++ ){
					part.info[i].key = (char*)calloc( info_key[i].size()+1, sizeof( char ) );
					if( nullptr == part.info[i].key ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info key in new part");
						free_part_addr_t( &part );
						*show = false;
					}
					strncpy(part.info[i].key, info_key[i].c_str(), info_key[i].size() );
					
					part.info[i].val = (char*)calloc( info_val[i].size()+1, sizeof( char ) );
					if( nullptr == part.info[i].val ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info val in new part");
						free_part_addr_t( &part );
						*show = false;
					}
					strncpy(part.info[i].val, info_val[i].c_str(), info_val[i].size() );
				}

				part.price_len = nprice;

				part.price= (struct part_price_t*)calloc( part.price_len, sizeof( struct part_price_t ) );
				if( nullptr == part.price ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_addr_t( &part );
					*show = false;
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
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part.dist_len; i++ ){
					part.dist[i].name = (char*)calloc( dist_name[i].size()+1, sizeof( char ) );
					if( nullptr == part.dist[i].name ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor name in new part");
						free_part_addr_t( &part );
						*show = false;
					}
					strncpy(part.dist[i].name, dist_name[i].c_str(), dist_name[i].size() );
					
					part.dist[i].pn = (char*)calloc( dist_pn[i].size()+1, sizeof( char ) );
					if( nullptr == part.dist[i].pn ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor part number in new part");
						free_part_addr_t( &part );
						*show = false;
					}
					strncpy(part.dist[i].pn, dist_pn[i].c_str(), dist_pn[i].size() );
				}

				part.inv_len = ninv; 
				part.inv = (struct part_inv_t*)calloc( part.inv_len, sizeof( struct part_inv_t ) );
				if( nullptr == part.inv ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_addr_t( &part );
					*show = false;
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
				unsigned int ptype_idx = dbinfo_get_ptype_index( info, type );
				struct dbinfo_ptype_t* ptype = nullptr;
				if( ptype_idx != (unsigned int)(-1) ){
					mutex_spin_lock_dbinfo();
					ptype = &((*info)->ptypes[ptype_idx]);
					mutex_unlock_dbinfo();
				}
	
				if( nullptr == ptype ){
					/* Type doesn't exist in database, need to add the new type */
					y_log_message(Y_LOG_LEVEL_INFO, "Type: %s not found in database. Adding type to database", type);

					/* Should break this out into function */
					mutex_spin_lock_dbinfo();
#if defined(_WIN32) || defined(__APPLE__)

					struct dbinfo_ptype_t* tmp = (struct dbinfo_ptype_t*)realloc( (*info)->ptypes, ((*info)->nptype + 1) *  sizeof(struct dbinfo_ptype_t) );
#else
					struct dbinfo_ptype_t* tmp = (struct dbinfo_ptype_t*)reallocarray( (*info)->ptypes, (*info)->nptype + 1, sizeof(struct dbinfo_ptype_t) );
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
			*show = false;
			
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
			*show = false;
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

void edit_part_window( bool* show, struct part_t* part_in, struct dbinfo_t** info ){
	static struct part_t* part;
	static bool first_run = true;
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
	const char* status_options[7] = {
			"Unknown",
			"Production",
			"Low Stock",
			"Unavailable",
			"NRND",
			"Last Time Buy",
			"Obsolete"
	};
	static const char* selection_str = NULL;

	if( NULL == part_in ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not edit part; null pointer passed" );
		return;
	}

	/* Copy part */
	part = copy_part_t( part_in );

	if( first_run ){

		ninfo = part->info_len;
		nprice = part->price_len;
		ndist = part->dist_len;
		ninv = part->inv_len;

		/* Copy static strings */
		strncpy( mpn, part->mpn, 511 );
		strncpy( mfg, part->mfg, 511 );
		strncpy( type, part->type, 256 );

		/* Free the arrays to make copying later easier */
		free( part->mpn );
		free( part->type );
		free( part->mfg );

		selection_idx = (int)part->status;
	
		/* Populate vectors with existing data */
		for( unsigned int i = 0; i < ninfo; i++ ){
			info_key.push_back( (part->info[i].key) );
			info_val.push_back( std::string(part_in->info[i].val) );
		}
		for( unsigned int i = 0; i < ndist; i++ ){
			dist_name.push_back( (part->dist[i].name) );
			dist_pn.push_back( std::string(part_in->dist[i].pn) );
		}
		for( unsigned int i = 0; i < nprice; i++ ){
			price_q.push_back( (part->price[i].quantity) );
			price_cost.push_back( part_in->price[i].price );
		}
		for( unsigned int i = 0; i < ninv; i++ ){
			inv_loc.push_back( std::string((*info)->invs[ (part_in->inv[i].loc ) ].name) );
			inv_amount.push_back( part->inv[i].q );
		}
		
		first_run = false;
	}

	if( !(selection_idx >= 0 && selection_idx < pstat_total) ){
		selection_idx = 0;
	}
	selection_str = status_options[selection_idx];

	/* Ensure popup is in the center */
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2( 0.5f, 0.5f));

	ImGuiWindowFlags flags =   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse \
							 | ImGuiWindowFlags_NoSavedSettings;
	if( ImGui::Begin("New Part Popup", show, flags ) ){
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

			ImGui::InputInt(price_q_ident.c_str(), &(*price_q_itr));
			ImGui::SameLine(NEWPART_SPACING);
			ImGui::InputDouble(price_cost_ident.c_str(), &(*price_cost_itr));

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
			ImGui::InputInt(inv_amount_ident.c_str(), (int*)&(*inv_amount_itr));

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
            	part->q = atoi( quantity );

				part->mpn = (char *)calloc( 512, sizeof( char ) );
				if( nullptr == part->mpn ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part mpn");
					free_part_t( part );
					*show = false;
				}
				strncpy( part->mpn, mpn, 511 );

				part->mfg = (char *)calloc( 512, sizeof( char ) );
				if( nullptr == part->mfg ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part mfg");
					free_part_t( part );
					*show = false;
				}
				strncpy( part->mfg, mfg, 511 );

				part->type = (char *)calloc( 512, sizeof( char ) );
				if( nullptr == part->type ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part type");
					free_part_t( part );
					*show = false;
				}
				strncpy( part->type, type, 256 );

            	part->status = (enum part_status_t)selection_idx;


				/* Store variable data information */
				part->info_len = ninfo;
				part->info = (struct part_info_t *)calloc( part->info_len, sizeof( struct part_info_t ) );
				if( nullptr == part->info ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info array in new part");
					free_part_t( part );
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part->info_len; i++ ){
					part->info[i].key = (char*)calloc( info_key[i].size()+1, sizeof( char ) );
					if( nullptr == part->info[i].key ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info key in new part");
						free_part_t( part );
						*show = false;
					}
					strncpy(part->info[i].key, info_key[i].c_str(), info_key[i].size() );
					
					part->info[i].val = (char*)calloc( info_val[i].size()+1, sizeof( char ) );
					if( nullptr == part->info[i].val ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info val in new part");
						free_part_t( part );
						*show = false;
					}
					strncpy(part->info[i].val, info_val[i].c_str(), info_val[i].size() );
				}

				part->price_len = nprice;

				part->price= (struct part_price_t*)calloc( part->price_len, sizeof( struct part_price_t ) );
				if( nullptr == part->price ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_t( part );
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part->price_len; i++ ){
					part->price[i].quantity = price_q[i];
					part->price[i].price = price_cost[i];
				}


				part->dist_len = ndist;
				part->dist = (struct part_dist_t*)calloc( part->dist_len, sizeof( struct part_dist_t ) );
				if( nullptr == part->dist ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor array in new part");
					free_part_t( part );
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part->dist_len; i++ ){
					part->dist[i].name = (char*)calloc( dist_name[i].size()+1, sizeof( char ) );
					if( nullptr == part->dist[i].name ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor name in new part");
						free_part_t( part );
						*show = false;
					}
					strncpy(part->dist[i].name, dist_name[i].c_str(), dist_name[i].size() );
					
					part->dist[i].pn = (char*)calloc( dist_pn[i].size()+1, sizeof( char ) );
					if( nullptr == part->dist[i].pn ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor part number in new part");
						free_part_t( part );
						*show = false;
					}
					strncpy(part->dist[i].pn, dist_pn[i].c_str(), dist_pn[i].size() );
				}

				part->inv_len = ninv; 
				part->inv = (struct part_inv_t*)calloc( part->inv_len, sizeof( struct part_inv_t ) );
				if( nullptr == part->inv ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price array in new part");
					free_part_t( part );
					*show = false;
				}
				/* copy data over */
				for( unsigned int i = 0; i < part->inv_len; i++ ){
					part->inv[i].q = inv_amount[i]; 
					part->inv[i].loc = dbinv_str_to_loc( info, inv_loc[i].c_str(), inv_loc[i].size() ); 
					if( part->inv[i].loc == -1 ){
						y_log_message(Y_LOG_LEVEL_ERROR, "Could not find inventory location; defaulting to location '0' ");
						part->inv[i].loc = 0;
					}
				}
				unsigned int ptype_idx = dbinfo_get_ptype_index( info, type );
				struct dbinfo_ptype_t* ptype = nullptr;
				if( ptype_idx != (unsigned int)(-1) ){
					mutex_spin_lock_dbinfo();
					ptype = &((*info)->ptypes[ptype_idx]);
					mutex_unlock_dbinfo();
				}
	
				if( nullptr == ptype ){
					/* Type doesn't exist in database, something wrong happened */
					y_log_message(Y_LOG_LEVEL_INFO, "Type: %s not found in database. Adding type to database", type);
				}
				if( !err_flg ) {

					/* Perform the write */
					redis_write_part( part );
					y_log_message(Y_LOG_LEVEL_DEBUG, "Data written to database");
				}
			}
			else {
				y_log_message(Y_LOG_LEVEL_ERROR, "Could not write new part to database");
			}
			*show = false;
			
			/* Clear all input data */
			ninfo = 0;
			nprice = 0;
			ndist = 0;

			info_key.clear();
			info_val.clear();
			dist_name.clear();
			dist_pn.clear();
			price_q.clear();
			price_cost.clear();
			inv_loc.clear();
			inv_amount.clear();

			free_part_t( part );
			part = nullptr;
	
			memset( quantity, 0, 127);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			selection_idx = 0;
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, finished writing");
			first_run = true;

		}
		ImGui::SetItemDefaultFocus();
		ImGui::SameLine();
		if ( ImGui::Button("Cancel", ImVec2(0, 0) )){
			*show = false;
			/* Clear all input data */
			ninfo = 0;
			nprice = 0;
			ndist = 0;

			info_key.clear();
			info_val.clear();
			dist_name.clear();
			dist_pn.clear();
			price_q.clear();
			price_cost.clear();
			inv_loc.clear();
			inv_amount.clear();

			free_part_t( part );
			part = nullptr;
	
			memset( quantity, 0, 127);
			memset( type, 0, 255 );
			memset( mfg, 0, 511 );
			memset( mpn, 0, 511 );
			selection_idx = 0;
			y_log_message(Y_LOG_LEVEL_DEBUG, "Cleared out part, exiting window");
			first_run = true;
		}

		ImGui::End();
	}

}
