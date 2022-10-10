#include <redis-wrapper/redis-wrapper.h>
#include <redis-wrapper/redis-json.h>
#include <json-c/json.h>
#include <string.h>
#include <yder.h>
#include <db_handle.h>
#include <unistd.h>
	

/* Redis Context for handling in database */
static redisContext *rc;

/* Parse part json object to part_t */
static int parse_json_part( struct part_t * part, struct json_object* restrict jpart ){

	/* Temporary string length variable */
	size_t jstrlen = 0;

	/* Need to create a bunch of json structs to parse everything out */
	json_object *jipn, *jq, *jtype, *jmfg, *jmpn, *jinfo;
	json_object *jprice, *jdist, *jstatus;
	json_object *jitr, *jkey, *jval;
	
	jipn    = json_object_object_get( jpart, "ipn" );
	jq      = json_object_object_get( jpart, "q" );
	jtype   = json_object_object_get( jpart, "type" );
	jmfg    = json_object_object_get( jpart, "mfg" );
	jmpn    = json_object_object_get( jpart, "mpn" );
	jstatus = json_object_object_get( jpart, "status" );
	jinfo   = json_object_object_get( jpart, "info" );
	jprice  = json_object_object_get( jpart, "price" ); 
	jdist   = json_object_object_get( jpart, "dist" );

	/* Copy data from json objects */

	/* Internal part number */
	part->ipn = json_object_get_int64( jipn );

	/* Internal Stock */
	part->q = json_object_get_int64( jq );

	/* Part Type */
	jstrlen = strlen( json_object_get_string( jtype ) );
	part->type = calloc( jstrlen + 1,sizeof( char ) );
	if( NULL == part->type ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part type");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->type, json_object_get_string( jtype ), jstrlen );

	/* Manufacturer */
	jstrlen = strlen( json_object_get_string( jmfg ) );
	part->mfg = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == part->mfg ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part manufacturer");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->mfg, json_object_get_string( jmfg ), jstrlen );

	/* Manufacturer part number */
	jstrlen = strlen( json_object_get_string( jmpn ) );
	part->mpn = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == part->mpn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for manufacturer part number");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->mpn, json_object_get_string( jmpn ), jstrlen );

	/* Part status */
	part->status = (enum part_status_t)json_object_get_int64( jstatus );

	/* Get number of elements in info key value array */
	part->info_len = json_object_object_length( jinfo );

	/* Allocate memory for information of part */
	part->info = calloc( part->info_len, sizeof( struct part_info_t ) );
	if( NULL == part->info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info of part");
		free_part_t( part );
		part = NULL;
		return -1;
	}

	/* Following may be dangerous without checking for null pointers */

	int count = 0;
	/* Iterate through objects to get the string data */
	json_object_object_foreach( jinfo, key, val ){

		/* copy key; which is easier for some reason? */
		part->info[count].key = calloc( (strlen( key ) + 1 ), sizeof( char ) );
		strcpy( part->info[count].key, key );

		jstrlen = strlen( json_object_get_string( val ) ) + 1;
		part->info[count].val = calloc( jstrlen, sizeof( char ) );
		strcpy( part->info[count].val, json_object_get_string( val ) );

		count++;
	}

	/* Get distributor information from array */
	part->dist_len = (unsigned int)json_object_array_length( jdist );
	if( part->dist_len > 0 ){
		part->dist = calloc( part->dist_len, sizeof( struct part_dist_t ) );	
		if( NULL == part->dist ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for distributor structure for part: %s", part->mpn );
			free_part_t( part );
			part = NULL;
			return -1;
		}
		for( unsigned int i = 0; i < part->dist_len; i++){
			/* Retrieve price object */
			jitr = json_object_array_get_idx( jdist, (int)i );

			/* Parse it out to get the required data */
			jkey = json_object_object_get( jitr, "name" );
			jval = json_object_object_get( jitr, "pn" );

			/* Distributor Name  */
			jstrlen = strlen( json_object_get_string( jkey ) );
			part->dist[i].name = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == part->dist[i].name ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in distributor name" );
				free_part_t( part );
				part = NULL;
				return -1;
			}
			strcpy( part->dist[i].name, json_object_get_string( jkey ) );

			/* Distributor part number */
			jstrlen = strlen( json_object_get_string( jval ) );
			part->dist[i].pn = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == part->dist[i].pn ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in price break" );
				free_part_t( part );
				part = NULL;
				return -1;
			}
			strcpy( part->dist[i].pn, json_object_get_string( jval ) );

		}
	}

	/* Get price information from array */
	part->price_len = (unsigned int)json_object_array_length( jprice );
	if( part->price_len > 0 ){
		part->price = calloc( part->price_len, sizeof( struct part_price_t ) );	
		if( NULL == part->price ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for price structure for part: %s", part->mpn );
			free_part_t( part );
			part = NULL;
			return -1;
		}
		for( unsigned int i = 0; i < part->price_len; i++){
			/* Retrieve price object */
			jitr = json_object_array_get_idx( jprice, (int)i );

			/* Parse it out to get the required data */
			jkey = json_object_object_get( jitr, "break" );
			jval = json_object_object_get( jitr, "price" );

			/* price break quantity  */
			part->price[i].quantity = json_object_get_int64( jkey );

			/* price break price (sounds weird, but can't think of a better
			 * name */
			part->price[i].price = json_object_get_double( jval );
		}
	}

	return 0;
}

/* Parse bom json object to bom_t */
static int parse_json_bom( struct bom_t * bom, struct json_object* restrict jbom ){

	/* Temporary string length variable */
	size_t jstrlen = 0;

	/* Need to create a bunch of json structs to parse everything out */
	json_object* jipn;
	json_object* jname;
	json_object* jversion;
	json_object* jlines;
	json_object* jline_itr;
	json_object* jkey;
	json_object* jval;
	
	jipn       = json_object_object_get( jbom, "ipn" );
	jname      = json_object_object_get( jbom, "name" ); 
	jversion   = json_object_object_get( jbom, "ver" ); 
	jlines     = json_object_object_get( jbom, "items" );

	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object passed for bom:\n%s\n", json_object_to_json_string_ext(jbom, JSON_C_TO_STRING_PRETTY));
	
	/* Copy data from json objects */

	/* Internal part number */
	bom->ipn = json_object_get_int64( jipn );

	/* BOM Version */
	jstrlen = strlen( json_object_get_string( jversion ) );
	bom->ver = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == bom->ver ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom version");
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}
	strncpy( bom->ver, json_object_get_string( jversion ), jstrlen );

	/* Get number of elements in info key value array */
	bom->nitems = json_object_array_length( jlines );

	/* Get bom line information from array */
	if( bom->nitems > 0 ){
		/* Allocate memory for information of part */
		bom->line = calloc( bom->nitems, sizeof( struct bom_line_t ) );
		if( NULL == bom->line ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part number array in bom");
			free_bom_t( bom );
			bom = NULL;
			return -1;
		}

		for( unsigned int i = 0; i < bom->nitems; i++ ){
			/* Retrieve line object */
			jline_itr = json_object_array_get_idx( jlines, (int)i );

			/* Parse it out to get the required data */
			jkey = json_object_object_get( jline_itr, "ipn" );
			jval = json_object_object_get( jline_itr, "q" );

			/* part ipn */
			bom->line[i].ipn = json_object_get_int64( jkey );

			/* quantity of part in bom */
			bom->line[i].q = json_object_get_double( jval );
		}
	}

	/* Initialize array of parts to be cached later */
	bom->parts = calloc( bom->nitems,  sizeof( struct part_t * ));
	if( NULL == bom->parts ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom part array" );
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}
	for( int i = 0; i< bom->nitems; i++ ){
		bom->parts[i] = get_part_from_ipn( bom->line[i].ipn );
		if( NULL == bom->parts[i] ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom part array" );
			free_bom_t( bom );
			bom = NULL;
			return -1;
		}

	}

	return 0;
}

/* Parse project json object to proj_t */
static int parse_json_proj( struct proj_t * prj, struct json_object* restrict jprj ){

	/* Temporary string length variable */
	size_t jstrlen = 0;

	/* Need to create a bunch of json structs to parse everything out */
	json_object* jipn;
	json_object* jname;
	json_object* jver;
	json_object* jauthor;
	json_object* jtcreate;
	json_object* jtmod;
	json_object* jpn;
	json_object* jsubprj;
	json_object* jboms;
	json_object* jprj_itr;
	json_object* jbom_itr;
	json_object* jkey;
	json_object* jval;
	json_object* tmp;
	
	jipn       = json_object_object_get( jprj, "ipn" );
	jpn        = json_object_object_get( jprj, "pn" );
	jname      = json_object_object_get( jprj, "name" ); 
	jver       = json_object_object_get( jprj, "ver" ); 
	jauthor    = json_object_object_get( jprj, "author" ); 
	jtcreate   = json_object_object_get( jprj, "time_created" );
	jtmod      = json_object_object_get( jprj, "time_mod" );
	jsubprj    = json_object_object_get( jprj, "sub_prj" );
	jboms      = json_object_object_get( jprj, "boms" );

	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object for project passed:\n%s\n", json_object_to_json_string_ext(jprj, JSON_C_TO_STRING_PRETTY));
	
	/* Copy data from json objects */

	/* Internal part number */
	prj->ipn = json_object_get_int64( jipn );

	/* Creation time */
	prj->time_created = (time_t) json_object_get_int64( jtcreate );

	/* Modified time */
	prj->time_mod = (time_t) json_object_get_int64( jtmod );

	/* Project Name */
	jstrlen = strlen( json_object_get_string( jname ) );
	prj->name = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == prj->name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project name");
		free_proj_t( prj );
		prj = NULL;
		return -1;
	}
	strncpy( prj->name, json_object_get_string( jname ), jstrlen );

	/* Project Version */
	jstrlen = strlen( json_object_get_string( jver ) );
	prj->ver = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == prj->ver ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project version");
		free_proj_t( prj );
		prj = NULL;
		return -1;
	}
	strncpy( prj->ver, json_object_get_string( jver ), jstrlen );

	/* Project Author */
	jstrlen = strlen( json_object_get_string( jauthor ) );
	prj->author = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == prj->author ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project author");
		free_proj_t( prj );
		prj = NULL;
		return -1;
	}
	strncpy( prj->author, json_object_get_string( jauthor ), jstrlen );

	/* Project Part Number */
	jstrlen = json_object_get_string_len( jpn );
	prj->pn = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == prj->pn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project part number");
		free_proj_t( prj );
		prj = NULL;
		return -1;
	}
	strncpy( prj->pn, json_object_get_string( jpn ), jstrlen );

	/* Get number of boms in project */
	prj->nboms = (unsigned int)json_object_array_length( jboms );
	y_log_message(Y_LOG_LEVEL_DEBUG, "Project contains %d boms", prj->nboms);

	if( prj->nboms > 0 ){
		/* Initialize array of boms */
		prj->boms = calloc( prj->nboms,  sizeof( struct proj_bom_ver_t ));
		if( NULL == prj->boms ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project bom array" );
			free_proj_t( prj );
			prj = NULL;
			return -1;
		}
		for( unsigned int i = 0; i< prj->nboms; i++ ){
			jbom_itr = json_object_array_get_idx( jboms, (int)i );
			if( NULL != jbom_itr ){
				y_log_message( Y_LOG_LEVEL_DEBUG, "BOM Index %d: \n%s", i, json_object_to_json_string(jbom_itr) );

				/* Parse it out to get the required data */
				jkey = json_object_object_get( jbom_itr, "ipn" );
				jval = json_object_object_get( jbom_itr, "ver" );
				unsigned int bom_ipn = json_object_get_int64( jkey );
				prj->boms[i].bom = get_bom_from_ipn( bom_ipn );
				if( NULL == prj->boms[i].bom ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom %d at %d in part array", bom_ipn, i );
					free_proj_t( prj );
					prj = NULL;
					return -1;
				}

				/* subproject version */
				jstrlen = strlen( json_object_get_string( jval ) );
				prj->boms[i].ver = calloc( jstrlen + 1, sizeof( char ) );
				if( NULL == prj->boms[i].ver ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in subproject version" );
					free_proj_t( prj );
					prj = NULL;
					return -1;
				}
				strcpy( prj->boms[i].ver, json_object_get_string( jval ) );
				//json_object_put( jbom_itr );
			}
			else {
				/* Issue with getting BOM */
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not get BOM at array index %d. Json data: \n%s", i, json_object_to_json_string(jboms) );
			}
		}
	}

	if( NULL != jsubprj ){
		/* Get number of subprojects in info key value array */
		prj->nsub = json_object_array_length( jsubprj );
		y_log_message(Y_LOG_LEVEL_DEBUG, "Project contains %d subprojects", prj->nsub);

		/* Get project sub information from array */
		if( prj->nsub > 0 ){
			y_log_message(Y_LOG_LEVEL_DEBUG, "Subprojects exist, start parsing");
			/* Allocate memory for information of part */
			prj->sub = calloc( prj->nsub, sizeof( struct proj_subprj_ver_t ) );
			if( NULL == prj->sub ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for subprojects in project");
				free_proj_t( prj );
				prj = NULL;
				return -1;
			}

			for( unsigned int i = 0; i < prj->nsub; i++ ){
				/* Retrieve subproject object */
				jprj_itr= json_object_array_get_idx( jsubprj, (int)i );
				if( NULL != jprj_itr ){

					y_log_message( Y_LOG_LEVEL_DEBUG, "Subproject Index %d: \n%s", i, json_object_to_json_string(jprj_itr) );
					/* Parse it out to get the required data */
					jkey = json_object_object_get( jprj_itr, "ipn" );
					jval = json_object_object_get( jprj_itr, "ver" );

					if( NULL != jval ){
						/* subproject version */
						jstrlen = strlen( json_object_get_string( jval ) );
						prj->sub[i].ver = calloc( jstrlen + 1, sizeof( char ) );
						if( NULL == prj->sub[i].ver ){
							y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in subproject version" );
							free_proj_t( prj );
							prj = NULL;
							return -1;
						}
						strcpy( prj->sub[i].ver, json_object_get_string( jval ) );
					}
					else {
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not find subproject version" );
						free_proj_t( prj );
						prj = NULL;
						return -1;
					}

					/* Use ipn of subproject to get subproject info */
					if( NULL != jkey ){
						unsigned int ipn = json_object_get_int64(jkey);
						prj->sub[i].prj = get_proj_from_ipn( ipn );
					}
					else {
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not find subproject ipn" );
						free_proj_t( prj );
						prj = NULL;
						return -1;
					}
					y_log_message(Y_LOG_LEVEL_DEBUG, "Cleanup subproject iterator");
					json_object_put(jprj_itr);
				}
				else{
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not get subprojects at index %d. Json data: \n%s", i, json_object_to_json_string(jsubprj) );
				}
			}
		}
	}

	return 0;
}

/* Parse bom json object to dbinfo_t */
static int parse_json_dbinfo( struct dbinfo_t * db, struct json_object* restrict jdb ){

	json_object* jversion;
	json_object* jnprj;
	json_object* jnbom;
	json_object* jnpart;
	json_object* jlock;
	json_object* jinit;
	json_object* jmajor;
	json_object* jminor;
	json_object* jpatch;
	
	jversion  = json_object_object_get( jdb, "version" ); 
	jmajor  = json_object_object_get( jversion, "major" ); 
	jminor  = json_object_object_get( jversion, "minor" ); 
	jpatch  = json_object_object_get( jversion, "patch" ); 
	jnprj     = json_object_object_get( jdb, "nprj" ); 
	jnbom     = json_object_object_get( jdb, "nbom" ); 
	jnpart    = json_object_object_get( jdb, "npart" ); 
	jlock     = json_object_object_get( jdb, "lock" );
	jinit     = json_object_object_get( jdb, "init" );

	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object passed for dbinfo:\n%s\n", json_object_to_json_string_ext(jdb, JSON_C_TO_STRING_PRETTY));
	
	/* Copy data from json objects */

	/* Number of kinds of objects  */
	db->nprj  = json_object_get_int64( jnprj );
	db->nbom  = json_object_get_int64( jnbom );
	db->npart = json_object_get_int64( jnpart );

	/* Flags */
	db->flags  = 0;
	db->flags |= json_object_get_int64( jinit ) << DBINFO_FLAG_INIT_SHFT;
	db->flags |= json_object_get_int64( jlock ) << DBINFO_FLAG_LOCK_SHFT;

	/* Version information */
	db->version.major = json_object_get_int64( jmajor );
	db->version.minor = json_object_get_int64( jminor );
	db->version.patch = json_object_get_int64( jpatch );

	return 0;
}



/* Free the part structure */
void free_part_t( struct part_t* part ){
	
	/* Zero out other data */
	part->ipn = 0;
	part->q = 0;
	part->status = pstat_unknown;
	
	/* Free all the strings if not NULL */
	if( NULL !=  part->type ){
		free( part->type );
		part->type = NULL;
	}
	if( NULL != part->mfg ){
		free( part->mfg );
		part->mfg = NULL;
	}
	if( NULL != part->mpn ){
		free( part->mpn );
		part->mpn = NULL;
	}

	if( NULL != part->info ){
		for( unsigned int i = 0; i < part->info_len; i++ ){
			if( NULL != part->info[i].key ){
				free( part->info[i].key );
				part->info[i].key = NULL;
			}
			if( NULL != part->info[i].val ){
				free( part->info[i].val );
				part->info[i].val = NULL;
			}

		}

		/* Iterated through all the info key value pairs, free the array itself */
		free( part->info );
		part->info = NULL;
		part->info_len = 0;
	}

	if( NULL != part->dist ){
		for( unsigned int i = 0; i < part->dist_len; i++ ){
			if( NULL != part->dist[i].name ){
				free( part->dist[i].name );
				part->dist[i].name = NULL;
			}
			if( NULL != part->dist[i].pn ){
				free( part->dist[i].pn );
				part->dist[i].pn = NULL;
			}
		}

		/* Finished iteration, free array pointer */
		free( part->dist );
		part->dist = NULL;
		part->dist_len = 0;
	}

	if( NULL != part->price ){
		for( unsigned int i = 0; i < part->price_len; i++ ){
			part->price[i].quantity = 0;
			part->price[i].price = 0.0;
		}

		/* Finished iteration, free array pointer */
		free( part->price );
		part->price = NULL;
		part->price_len = 0;
	}

}

/* Free the bom structure */
void free_bom_t( struct bom_t* bom ){

	bom->ipn = 0;

	/* Free all the arrays if not NULL */
	if( NULL !=  bom->line ){
		free( bom->line );
		bom->line = NULL;
	}
	if( NULL != bom->ver ){
		free( bom->ver );
		bom->ver = NULL;
	}
	if( NULL != bom->parts ){
		for( unsigned int i = 0; i < bom->nitems; i++ ){
			if( NULL != bom->parts[i] ){
				free_part_t( bom->parts[i] );
				bom->parts[i] = NULL;
			}
		}

		/* Iterated through all the info key value pairs, free the array itself */
		free( bom->parts );
		bom->parts = NULL;
		bom->nitems = 0;
	}
}

/* Free the project structure */
void free_proj_t( struct proj_t* prj ){

	/* Zero out easy stuff */
	prj->ipn = 0;
	prj->selected = -1;
	prj->time_created = (time_t)0;
	prj->time_mod = (time_t)0;

	/* Free all the arrays if not NULL */
	if( NULL !=  prj->ver ){
		free( prj->ver );
		prj->ver = NULL;
	}
	if( NULL != prj->name ){
		free( prj->name );
		prj->name = NULL;
	}
	if( NULL != prj->pn ){
		free( prj->pn );
		prj->pn = NULL;
	}
	if( NULL != prj->author ){
		free( prj->author );
		prj->author = NULL;
	}
	if( NULL != prj->boms ){
		for( unsigned int i = 0; i < prj->nboms; i++ ){
			if( NULL != prj->boms[i].bom ){
				free_bom_t( prj->boms[i].bom );
				prj->boms[i].bom = NULL;
			}
			if( NULL != prj->boms[i].ver ){
				free( prj->boms[i].ver );
				prj->boms[i].ver = NULL;
			}
		}
		/* Iterated through all the info key value pairs, free the array itself */
		free( prj->boms );
		prj->boms = NULL;
		prj->nboms = 0;
	}

	/* free subprojects, which requires recursion and could get messy */
	if( NULL != prj->sub ){
		for( unsigned int i = 0; i < prj->nsub; i++ ){
			if( NULL != prj->sub[i].prj ){
				free_proj_t( prj->sub[i].prj );
				prj->sub[i].prj = NULL;
			}
			if( NULL != prj->sub[i].ver ){
				free( prj->sub[i].ver );
				prj->sub[i].ver = NULL;
			}
		}
		/* Iterated through all the info key value pairs, free the array itself */
		free( prj->sub );
		prj->sub = NULL;
		prj->nsub = 0;
	}
}


/* Write part to database */
int redis_write_part( struct part_t* part ){
	/* Database part name */
	char * dbpart_name = NULL;

	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Part struct passed when writing to database was NULL" );
		return -1;
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Creating json objects");

	/* Create json object to write */
	json_object *part_root = json_object_new_object();
	json_object *info = json_object_new_object();
	json_object *dist = json_object_new_array_ext(part->dist_len);
	json_object *price = json_object_new_array_ext(part->price_len);


	json_object* dist_itr;
	json_object* price_itr;

	if( NULL == part_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	if( NULL == info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part info json object" );
		json_object_put(part_root);
		return -1;	
	}

	if( NULL == dist ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part distributor info json object" );
		json_object_put(info);
		json_object_put(part_root);
		return -1;	
	}

	if( NULL == price ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part price info json object" );
		json_object_put(info);
		json_object_put(dist);
		json_object_put(part_root);
		return -1;	
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	/* Add all custom fields to info */
	if( part->info_len != 0 ) {
		for( unsigned int i = 0; i < part->info_len; i++ ){
			json_object_object_add( info, part->info[i].key, json_object_new_string(part->info[i].val) );
		}
	}

	/* Distributor information */
	if( part->dist_len != 0 ) {
		for( unsigned int i = 0; i < part->dist_len; i++ ){
			dist_itr = json_object_new_object();
			if( NULL == dist_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part distributor iterator json object" );	
				json_object_put(info);
				json_object_put(dist);
				json_object_put(price);
				json_object_put( part_root );
				return -1;
			}
			json_object_object_add( dist_itr, "name", json_object_new_string(part->dist[i].name) );
			json_object_object_add( dist_itr, "pn", json_object_new_string(part->dist[i].pn) );
			json_object_array_put_idx( dist, i, dist_itr );
		}

	}

	/* Price information */
	if( part->price_len != 0 ) {
		for( unsigned int i = 0; i < part->price_len; i++ ){
			price_itr = json_object_new_object();
			if( NULL == price_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part price iterator json object" );	
				json_object_put(info);
				json_object_put(dist);
				json_object_put(price);
				json_object_put( part_root );
				return -1;
			}			
			json_object_object_add( price_itr, "break", json_object_new_int64(part->price[i].quantity) );
			json_object_object_add( price_itr, "price", json_object_new_double(part->price[i].price) );
			json_object_array_put_idx( price, i, json_object_get( price_itr ) );
		}

	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");

	/* Add all parts of the part struct to the json object */
	json_object_object_add( part_root, "ipn", json_object_new_int64(part->ipn) ); /* This should be generated somehow */
	json_object_object_add( part_root, "q", json_object_new_int64(part->q) );
	json_object_object_add( part_root, "status", json_object_new_int64(part->status) );
	json_object_object_add( part_root, "type", json_object_new_string( part->type ) );
	json_object_object_add( part_root, "mfg", json_object_new_string( part->mfg ) );
	json_object_object_add( part_root, "mpn", json_object_new_string( part->mpn ) );
	json_object_object_add( part_root, "info", info );
	json_object_object_add( part_root, "dist", dist );
	json_object_object_add( part_root, "price", price );

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

	dbpart_name = calloc( strlen(part->mpn) + strlen("part:") + 2, sizeof( char ) );

	int retval = -1;
	if( NULL == dbpart_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbpart_name");
		retval = -1;
	}
	else{
		/* create name */
		sprintf( dbpart_name, "part:%s", part->mpn);

		y_log_message(Y_LOG_LEVEL_DEBUG, "Created name: %s", dbpart_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbpart_name, "$", json_object_to_json_string(part_root) );
		//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}



	/* Cleanup json object */
	free( dbpart_name );
	json_object_put( part_root );

	return retval;

}

int redis_write_bom( struct bom_t* bom ){
	/* Database part name */
	char * dbbom_name = NULL;

	if( NULL == bom ){
		y_log_message( Y_LOG_LEVEL_ERROR, "BOM struct passed when writing to database was NULL" );
		return -1;
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Creating json objects");

	/* Create json object to write */
	json_object *bom_root = json_object_new_object();
	json_object *ipn = json_object_new_object();
	json_object *version = json_object_new_object();
	json_object *line = json_object_new_array_ext(bom->nitems);
	json_object *line_itr = NULL;

	if( NULL == bom_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	if( NULL == ipn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new bom ipn json object" );
		json_object_put(bom_root);
		return -1;	
	}

	if( NULL == version ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate bom version json object" );
		json_object_put( ipn );
		json_object_put(bom_root);
		return -1;	
	}

	if( NULL == line ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new bom items json object" );
		json_object_put(ipn);
		json_object_put(version);
		json_object_put(bom_root);
		return -1;	
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	/* Internal part numbers for bom items */
	if( bom->nitems != 0 ) {
		for( unsigned int i = 0; i < bom->nitems; i++ ){
			line_itr = json_object_new_object();
			if( NULL == line_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new bom line iterator json object" );	
				json_object_put(ipn);
				json_object_put(version);
				json_object_put(line);
				json_object_put(bom_root);
				return -1;
			}			
			json_object_object_add( line_itr, "ipn", json_object_new_int64(bom->parts[i]->ipn) );
			json_object_object_add( line_itr, "q", json_object_new_int64(bom->parts[i]->q) );
			json_object_array_put_idx( line, i, json_object_get( line_itr ) );
		}

	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");

	/* Add all parts of the part struct to the json object */
	json_object_object_add( bom_root, "ipn", json_object_new_int64(bom->ipn) ); /* This should be generated somehow */
	json_object_object_add( bom_root, "ver", version );
	json_object_object_add( bom_root, "items", line );

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

	dbbom_name = calloc( strlen("bom:") + 2 + 16, sizeof( char ) ); /* IPN has to be less than 16 characters right now */

	int retval = -1;
	if( NULL == dbbom_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbbom_name");
		retval = -1;
	}
	else{
		/* create name */
		sprintf( dbbom_name, "bom:%d", bom->ipn);

		y_log_message(Y_LOG_LEVEL_DEBUG, "Created name: %s", dbbom_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbbom_name, "$", json_object_to_json_string(bom_root) );
		//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(bom_root, JSON_C_TO_STRING_PRETTY));
	}

	/* Cleanup json object */
	free( dbbom_name );
	json_object_put( bom_root );

	return retval;

}

int redis_write_proj( struct proj_t* prj ){
	/* Database part name */
	char * dbprj_name = NULL;

	if( NULL == prj ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Project struct passed when writing to database was NULL" );
		return -1;
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Creating json objects");

	/* Create json object to write */
	json_object *prj_root = json_object_new_object();
	json_object *sub = json_object_new_array_ext(prj->nsub);
	json_object *boms = json_object_new_array_ext(prj->nboms);
	json_object *sub_itr = NULL;
	json_object *bom_itr = NULL;

	if( NULL == prj_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	if( NULL == sub ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate project subproject json object" );
		json_object_put(prj_root);
		return -1;	
	}

	if( NULL == boms ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate project boms json object" );
		json_object_put(sub);
		json_object_put(prj_root);
		return -1;	
	}


	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	/* Internal part numbers for boms used */
	if( prj->nboms != 0 ) {
		for( unsigned int i = 0; i < prj->nboms; i++ ){
			bom_itr = json_object_new_object();
			if( NULL == bom_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new project bom iterator json object" );	
				json_object_put(sub);
				json_object_put(prj_root);
				return -1;
			}			
			json_object_object_add( bom_itr, "ipn", json_object_new_int64(prj->boms[i].bom->ipn) );
			json_object_object_add( bom_itr, "ver", json_object_new_string(prj->boms[i].ver) );
			json_object_array_put_idx( boms, i, json_object_get( bom_itr ) );
		}

	}

	/* Internal part numbers for subprojects */
	if( prj->nsub != 0 ) {
		for( unsigned int i = 0; i < prj->nsub; i++ ){
			sub_itr = json_object_new_object();
			if( NULL == sub_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new project bom iterator json object" );	
				json_object_put(sub);
				json_object_put(prj_root);
				return -1;
			}			
			json_object_object_add( sub_itr, "ipn", json_object_new_int64(prj->sub[i].prj->ipn) );
			json_object_object_add( sub_itr, "ver", json_object_new_string(prj->sub[i].ver) );
			json_object_array_put_idx( sub, i, json_object_get( sub_itr ) );
		}

	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");

	/* Add all parts of the part struct to the json object */
	json_object_object_add( prj_root, "ipn", json_object_new_int64(prj->ipn) ); /* This should be generated somehow */
	json_object_object_add( prj_root, "ver", json_object_new_string( prj->ver ) );
	json_object_object_add( prj_root, "name", json_object_new_string( prj->name ) );
	json_object_object_add( prj_root, "author", json_object_new_string( prj->author) );
	json_object_object_add( prj_root, "time_created", json_object_new_int64( prj->time_created ) );
	json_object_object_add( prj_root, "time_mod", json_object_new_int64( prj->time_mod ) );
	json_object_object_add( prj_root, "sub_prj", sub );
	json_object_object_add( prj_root, "boms", boms );

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

	dbprj_name = calloc( strlen("prj:") + 2 + 16, sizeof( char ) ); /* IPN has to be less than 16 characters right now */

	int retval = -1;
	if( NULL == dbprj_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbprj_name");
		retval = -1;
	}
	else{
		/* create name */
		sprintf( dbprj_name, "prj:%d", prj->ipn);

		y_log_message(Y_LOG_LEVEL_DEBUG, "Created name: %s", dbprj_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbprj_name, "$", json_object_to_json_string(prj_root) );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(prj_root, JSON_C_TO_STRING_PRETTY));
	}

	/* Cleanup json object */
	free( dbprj_name );
	json_object_put( prj_root );

	return retval;

}

/* Import file to database */
int redis_import_part_file( char* filepath ){

	/* Check if file is readable */
	FILE* partfp = fopen(filepath, "r");
	if( NULL == partfp ){
		y_log_message(Y_LOG_LEVEL_ERROR, "File %s either does not exist or program does not have read permissions", filepath);
		return -1;
	}
	/* Just using as test for permissions; json-c doesn't like passing the file
	 * descriptor apparently */
	fclose(partfp);

	/* pass fd to importer */
//	struct json_object* root = json_object_from_fd( partfp );
	struct json_object* root = json_object_from_file( filepath );

	if( NULL == root ){
		/* Something wrong happened to the parsing */
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not parse import part file %s. Check syntax", filepath );
//		fclose(partfp);
		return -1;
	}
	
	/* Get number of objects in array */	
	size_t array_len = json_object_array_length( root );
	struct json_object* jo_idx = NULL;
	struct part_t* part = NULL;
	int parse_part_retval = 0;


	for( size_t i = 0; i < array_len; i++ ){
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Index: %d", i);

		/* Allocate space for part */
		part = calloc( 1,  sizeof( struct part_t ) );
		if( NULL == part ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part while parsing input file: %s", filepath);
		}
		else {

			/* Parse object from root at index; ensures that formatting is done
			 * correctly, otherwise it would make sense to just pass the json
			 * directly to the database */
			jo_idx = json_object_array_get_idx( root, (int)i );
			if( NULL == jo_idx ){
				/* Parsing error */
				y_log_message( Y_LOG_LEVEL_ERROR, "Error parsing file %s at array index %d", filepath, i );
			}
			else {
				//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object at index[%d]:\n%s\n", i, json_object_to_json_string_ext(jo_idx, JSON_C_TO_STRING_PRETTY));

				/* Convert json object to part_t */
				parse_part_retval = parse_json_part( part, jo_idx );
				if( !parse_part_retval ){
					/* No parsing errors, write to database */
					if( redis_write_part( part ) ){
						y_log_message( Y_LOG_LEVEL_ERROR, "Could not write part mpn: %s to database", part->mpn );
					}
					else{
						y_log_message( Y_LOG_LEVEL_INFO, "Wrote part mpn: %s to database", part->mpn );
					}
					/* Cleanup part */
					free_part_t( part );
					part = NULL;
					/* Cleanup object  */
					jo_idx = NULL;
					y_log_message(Y_LOG_LEVEL_DEBUG, "Cleaned up objects");
				}
			}
		}
	}

	/* Cleanup root object */
	y_log_message(Y_LOG_LEVEL_DEBUG, "Root object cleanup");
	json_object_put(root);

	return 0;
}

/* Connect to redis database */
int redis_connect( const char* hostname, int port ){
	/* Connect to redis database */
	if( hostname == NULL ){
		hostname = "127.0.0.1";
	}
	if( port == 0 ){
		port = 6379;
	}

	if( init_redis( &rc, hostname, port ) ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not connect to redis database");
		return -1;
	}
	y_log_message(Y_LOG_LEVEL_INFO, "Established connection to redis database");


	return 0;
}

/* Free memory for redis connection */
void redis_disconnect( void ){
	if( NULL != rc ){
		redisFree( rc );
	}
	y_log_message(Y_LOG_LEVEL_INFO, "Disconnected from redis database");

	rc = NULL;
}

/* Create struct from parsed item in database, from part number */
struct part_t* get_part_from_pn( const char* pn ){
	struct part_t * part = NULL;	
	char* dbpart_name = NULL;
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %s", pn);
		return NULL;
	}

	/* Allocate size for database name */
	dbpart_name = calloc( strlen(pn) + strlen("part:") + 2, sizeof( char ) );

	if( NULL == dbpart_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbpart_name");
		return NULL;
	}

	sprintf( dbpart_name, "part:%s", pn);

	/* Allocate space for part */
	part = calloc( 1, sizeof( struct part_t ) );
	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part: %s", pn);
		free( dbpart_name );
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for part number */
	struct json_object* jpart;
	
	/* Sanitize input? */

	/* Get part number, store in json object */
	if( !redis_json_get( rc, dbpart_name, "$", &jpart ) ){
		/* Parse the json */
		parse_json_part( part, jpart );

		/* free json data; will segfault otherwise */
		json_object_put( jpart );
	}
	else {
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not get part for %s", dbpart_name);
		part = NULL;
	}

	/* Free data */
	free( dbpart_name );

	return part;

}

/* Create struct from parsed item in database, from internal part number */
struct part_t* get_part_from_ipn( unsigned int ipn ){
	struct part_t * part = NULL;	
	char ipn_s[32] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %ld", ipn);
		return NULL;
	}

	/* Allocate space for part */
	part = calloc( 1, sizeof( struct part_t ) );
	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part: %ld", ipn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for part number */
	struct json_object* jpart;
	
	/* Sanitize input? */

	/* Search part number by IPN */
	redisReply* reply = NULL;
	sprintf( ipn_s, "[ %ld %ld ]", ipn, ipn);
	redis_json_index_search( rc, &reply, "partid", "ipn", ipn_s );

	/* Returns back a few elements; need to grab the second one to get the
	 * right object */
	if( NULL == reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 3 ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database search did not reply correctly for part ipn %d", ipn );
		free( part ); /* don't need to free other parts of array as they have not been allocated yet */
		/* Free redis reply */
		freeReplyObject(reply);
		return NULL;
	}

	/* Should be correct response, get second element parsed into jbom */
	jpart = json_tokener_parse( reply->element[2]->element[1]->str );	


	/* Free redis reply */
	freeReplyObject(reply);

	/* Parse the json */
	parse_json_part( part, jpart );


	/* Free json */
	json_object_put( jpart );

	return part;

}

/* Create bom struct from parsed item in database, from internal part number */
struct bom_t* get_bom_from_ipn( unsigned int ipn ){
	struct bom_t * bom = NULL;	
	char ipn_s[32] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of bom: %ld", ipn);
		return NULL;
	}

	/* Allocate space for bom */
	bom = calloc( 1, sizeof( struct bom_t ) );
	if( NULL == bom ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom: %ld", ipn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for bom */
	struct json_object* jbom = NULL;
	
	/* Sanitize input? */

	/* Search part number by IPN */
	redisReply* reply;
	sprintf( ipn_s, "[ %ld %ld ]", ipn, ipn);
	redis_json_index_search( rc, &reply, "bomid", "ipn", ipn_s );

	/* Returns back a few elements; need to grab the second one to get the
	 * right object */
	if( NULL == reply || reply->type != REDIS_REPLY_ARRAY ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database search did not reply correctly for bom ipn %d", ipn );
		free( bom ); /* don't need to free other parts of array as they have not been allocated yet */
		/* Free redis reply */
		freeReplyObject(reply);
		return NULL;
	}

	/* Should be correct response, get second element parsed into jbom */
	jbom = json_tokener_parse( reply->element[2]->element[1]->str );	
//	jbom = json_object_array_get_idx( root, 0 );	

	/* Free redis reply */
	freeReplyObject(reply);

	/* Parse the json */
	parse_json_bom( bom, jbom );


	/* Free json */
	json_object_put( jbom );

	return bom;
}

/* Create project struct from parsed item in database, from internal part number */
struct proj_t* get_proj_from_ipn( unsigned int ipn ){
	struct proj_t * prj = NULL;	
	char ipn_s[32] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of project: %ld", ipn);
		return NULL;
	}

	/* Allocate space for bom */
	prj = calloc( 1, sizeof( struct proj_t ) );
	if( NULL == prj ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for project: %ld", ipn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for project */
	struct json_object* jprj = NULL;
	
	/* Sanitize input? */

	/* Search part number by IPN */
	redisReply* reply;
	sprintf( ipn_s, "[ %ld %ld ]", ipn, ipn);
	redis_json_index_search( rc, &reply, "prjid", "ipn", ipn_s );

	/* Returns back a few elements; need to grab the second one to get the
	 * right object */
	if( NULL == reply || reply->type != REDIS_REPLY_ARRAY ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database search for project ipn %d did not reply correctly", ipn );
		free( prj ); /* don't need to free other parts of array as they have not been allocated yet */
		/* Free redis reply */
		freeReplyObject(reply);
		return NULL;
	}
	else if( NULL == reply->element[2] || NULL == reply->element[2]->element[1] || NULL == reply->element[2]->element[1]->str){
		y_log_message(Y_LOG_LEVEL_WARNING, "Database did not return data for project ipn %d. Does it exist, or was there an issue with the database?", ipn);
		free( prj );
		freeReplyObject(reply);
		return NULL;
	}

	enum json_tokener_error parse_err;
	
	/* Should be correct response, get second element parsed into jbom */
	y_log_message( Y_LOG_LEVEL_DEBUG, "Database response: \n%s", reply->element[2]->element[1]->str );
	jprj = json_tokener_parse_verbose( reply->element[2]->element[1]->str, &parse_err );	

	/* Free redis reply */
	freeReplyObject(reply);

	if( parse_err != json_tokener_success ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Issue while parsing database response for project. Error number: %d", parse_err );

		return NULL;
	}

	/* Parse the json */
	parse_json_proj( prj, jprj );

	/* Free json */
	//json_object_put( jprj );
	return prj;
}

/* Read database information */
int redis_read_dbinfo( struct dbinfo_t* db ){
	json_object* jdb;
	if( redis_json_get( rc, "popdb", "$", &jdb ) ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not get database information");
		return -1;
	}

	/* Parse data */
	return parse_json_dbinfo( db, jdb );
}

/* Write database information */
static int write_dbinfo( struct dbinfo_t* db ){
	/* Database part name */
	char * dbinfo_name = "popdb";

	if( NULL == db ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database info struct passed when writing to database was NULL" );
		return -1;
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Creating json objects");


	/* Create json object to write */
	json_object *dbinfo_root = json_object_new_object();
	json_object* version 	 = json_object_new_object();

	int lock = ((db->flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK);
	int init = ((db->flags & DBINFO_FLAG_INIT) == DBINFO_FLAG_INIT);

	if( NULL == dbinfo_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root for dbinfojson object" );
		return -1;
	}

	if( NULL == version ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate dbinfo version json object" );
		json_object_put(dbinfo_root);
		return -1;	
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	json_object_object_add( version, "major", json_object_new_int64( db->version.major ) );
	json_object_object_add( version, "minor", json_object_new_int64( db->version.minor ) );
	json_object_object_add( version, "patch", json_object_new_int64( db->version.patch ) );

	/* Add all parts of the part struct to the json object */
	json_object_object_add( dbinfo_root, "version", version );
	json_object_object_add( dbinfo_root, "nprj", json_object_new_int64(db->nprj) );
	json_object_object_add( dbinfo_root, "nbom", json_object_new_int64(db->nbom) );
	json_object_object_add( dbinfo_root, "npart", json_object_new_int64(db->npart) );
	json_object_object_add( dbinfo_root, "init", json_object_new_int64( init ) );
	json_object_object_add( dbinfo_root, "lock", json_object_new_int64( lock ) );

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

	/* Write object to database */
	int retval = redis_json_set( rc, dbinfo_name, "$", json_object_to_json_string(dbinfo_root) );
	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(dbinfo_root, JSON_C_TO_STRING_PRETTY));

	/* Cleanup json object */
	json_object_put( dbinfo_root );

	return 0;
}

/* Write database information; read modify write  */
int redis_write_dbinfo( struct dbinfo_t* db ){
	struct dbinfo_t db_check= {};
	do{
		redis_read_dbinfo( &db_check );
		if( (db_check.flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK ){
			sleep(2); /* Wait a little until lock is free */
		}

	} while( (db_check.flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK );

	/* now write data */
	return write_dbinfo( db );

}
