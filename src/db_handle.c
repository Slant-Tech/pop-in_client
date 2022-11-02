#include <redis-wrapper/redis-wrapper.h>
#include <redis-wrapper/redis-json.h>
#include <json-c/json.h>
#include <string.h>
#include <yder.h>
#include <db_handle.h>
#include <unistd.h>
#include <stdio.h>
	

/* Redis Context for handling in database */
static redisContext *rc;

static int dbinfo_mtx = 0;

static struct dbinfo_t dbinfo = {0};

/* Lock method to enforce atomic access to dbinfo */
static int lock( volatile int *exclusion ){
	int retval = 0;
	retval = __sync_lock_test_and_set( exclusion, 1);
	return retval;
}
static void unlock( volatile int *exclusion ){
	__sync_lock_release( exclusion );
}

/* Remove escape characters for display */
static char* remove_escape_char( char* s, size_t len ){
	char* out = calloc( len + 1, sizeof(char) );
	if( NULL == out ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for escape character fix string" );
		return NULL;
	}
	char* itr = out;
	for( size_t i = 0; i < len; i++){
		if( (s[i] == '\\') && ((i+1) < len) ){
			switch( s[i] ){
				case '{':
				case '}':
				case '[':
				case ']':
				case '+':
				case '-':
				case '/':
				case '\\':
					/* FALLTHRU */
					/* Correct output to ignore copying this part of data */
					break;

				default:
					/* Copy data over */	
					*itr = s[i];
					itr++;
					break;
			}
		}
		else {
			*itr = s[i];
			itr++;
		}
	}
	int newlen = strnlen( out, len );	
	if( strnlen( out, len ) < len ){
		/* Unallocate some memory, don't need it */
		out = realloc( out, newlen + 1 );
	}

	y_log_message( Y_LOG_LEVEL_DEBUG, "String with removed escape characters: %s", out);
	return out;

}

/* Useful for sanitizing search string for redis */
static char* sanitize_redis_string(  char* s ){
	int required_escapes = 0;
	char* out = NULL;
	for( unsigned int i = 0; i < strnlen( s, 1024 ); i++){
		/* Check if is in list of characters required for escaping */
		switch( s[i] ){
			case '{':
			case '}':
			case '[':
			case ']':
			case '+':
			case '-':
			case '/':
				/* FALLTHRU */
				/* Needs to be escaped */
				required_escapes++;
				break;

			default:
				/* Do nothing */
				break;

		}
	}

	if( required_escapes > 0 ) {
		out = calloc( strnlen( s, 1024 ) + 1 + required_escapes, sizeof( char ) );
		if( NULL == out ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for sanitized string");
			return NULL;
		}

		/* Copy over data that is needed, with escape characters */
		unsigned int new_idx = 0;
		for( unsigned int i = 0; i < strnlen( s, 1024 ); i++){
			/* Check if is in list of characters required for escaping */
			switch( s[i] ){
				case '{':
				case '}':
				case '[':
				case ']':
				case '+':
				case '-':
				case '/':
					/* FALLTHRU */
					/* Needs to be escaped; add escaped character */
					out[new_idx] = '\\';
					new_idx++;
					break;
	
				default:
					/* Do nothing */
					break;
	
			}
			out[new_idx] = s[i];
			new_idx++;
		}
	}
	else{
		out = s;
	}

	y_log_message( Y_LOG_LEVEL_DEBUG, "Sanitized string: %s", out ); 
	return out;
}

/* Parse part json object to part_t */
static int parse_json_part( struct part_t * part, struct json_object* restrict jpart ){

	/* Temporary string length variable */
	size_t jstrlen = 0;

	/* Need to create a bunch of json structs to parse everything out */
	json_object *jipn, *jq, *jtype, *jmfg, *jmpn, *jinfo;
	json_object *jprice, *jdist, *jstatus, *jinv;
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
	jinv    = json_object_object_get( jpart, "inv" );

	/* Copy data from json objects */

	/* Internal part number */
	part->ipn = json_object_get_int64( jipn );

	/* Internal Stock */
	part->q = json_object_get_int64( jq );

	/* Part Type */
	jstrlen = json_object_get_string_len( jtype );
	part->type = calloc( jstrlen + 1,sizeof( char ) );
	if( NULL == part->type ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part type");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->type, json_object_get_string( jtype ), jstrlen );

	/* Manufacturer */
	jstrlen = json_object_get_string_len( jmfg );
	part->mfg = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == part->mfg ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part manufacturer");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->mfg, json_object_get_string( jmfg ), jstrlen );

	/* Manufacturer part number */
	jstrlen = json_object_get_string_len( jmpn );
	part->mpn = remove_escape_char( json_object_get_string( jmpn ), jstrlen );
	if( NULL == part->mpn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for manufacturer part number");
		free_part_t( part );
		part = NULL;
		return -1;
	}

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

		jstrlen = json_object_get_string_len( val );
		part->info[count].val = calloc( jstrlen + 1, sizeof( char ) );
		strncpy( part->info[count].val, json_object_get_string( val ), jstrlen );

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
			jstrlen = json_object_get_string_len( jkey );
			part->dist[i].name = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == part->dist[i].name ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in distributor name" );
				free_part_t( part );
				part = NULL;
				return -1;
			}
			strncpy( part->dist[i].name, json_object_get_string( jkey ), jstrlen );

			/* Distributor part number */
			jstrlen = json_object_get_string_len( jval );
			part->dist[i].pn = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == part->dist[i].pn ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in price break" );
				free_part_t( part );
				part = NULL;
				return -1;
			}
			strncpy( part->dist[i].pn, json_object_get_string( jval ), jstrlen);

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
	/* Get inventory information from array */
	part->inv_len = (unsigned int)json_object_array_length( jinv );
	if( part->inv_len > 0 ){
		part->inv = calloc( part->inv_len, sizeof( struct part_inv_t ) );	
		if( NULL == part->inv ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for location structure for part: %s", part->mpn );
			free_part_t( part );
			part = NULL;
			return -1;
		}
		for( unsigned int i = 0; i < part->inv_len; i++){
			/* Retrieve price object */
			jitr = json_object_array_get_idx( jinv, (int)i );

			/* Parse it out to get the required data */
			jkey = json_object_object_get( jitr, "loc" );
			jval = json_object_object_get( jitr, "q" );

			/* location number */
			part->inv[i].loc = json_object_get_int64( jkey );

			/* location quantity  */
			part->inv[i].q = json_object_get_int64( jval );
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
	json_object* jline_ipn;
	json_object* jline_q;
	json_object* jline_type;
	
	jipn       = json_object_object_get( jbom, "ipn" );
	jname      = json_object_object_get( jbom, "name" ); 
	jversion   = json_object_object_get( jbom, "ver" ); 
	jlines     = json_object_object_get( jbom, "items" );

	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object passed for bom:\n%s\n", json_object_to_json_string_ext(jbom, JSON_C_TO_STRING_PRETTY));
	
	/* Copy data from json objects */

	/* Internal part number */
	bom->ipn = json_object_get_int64( jipn );

	/* BOM Version */
	jstrlen = json_object_get_string_len( jversion );
	bom->ver = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == bom->ver ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom version");
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}
	strncpy( bom->ver, json_object_get_string( jversion ), jstrlen );

	/* BOM Name */
	jstrlen = strlen( json_object_get_string( jname ) );
	bom->name = calloc( jstrlen + 1, sizeof( char ) );
	if( NULL == bom->name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom name");
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}
	strncpy( bom->name, json_object_get_string( jname ), jstrlen );

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
			jline_ipn = json_object_object_get( jline_itr, "ipn" );
			jline_q = json_object_object_get( jline_itr, "q" );
			jline_type = json_object_object_get( jline_itr, "type" );

			/* part ipn */
			bom->line[i].ipn = json_object_get_int64( jline_ipn );

			/* quantity of part in bom */
			bom->line[i].q = json_object_get_double( jline_q );

			/* part type */
			jstrlen = json_object_get_string_len( jline_type );
			bom->line[i].type = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == bom->line[i].type ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom version");
				free_bom_t( bom );
				bom = NULL;
				return -1;
			}
			strncpy( bom->line[i].type, json_object_get_string( jline_type ), jstrlen );
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
		bom->parts[i] = get_part_from_ipn( bom->line[i].type, bom->line[i].ipn );
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

				/* bom version */
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
				unsigned int bom_ipn = json_object_get_int64( jkey );
				prj->boms[i].bom = get_bom_from_ipn( bom_ipn, prj->boms[i].ver );
				if( NULL == prj->boms[i].bom ){
					y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom %d at %d in part array", bom_ipn, i );
					free_proj_t( prj );
					prj = NULL;
					return -1;
				}
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
				//	y_log_message(Y_LOG_LEVEL_DEBUG, "Cleanup subproject iterator");
//					json_object_put(jprj_itr);
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

	size_t jstrlen = 0;

	json_object* jversion = NULL;
	json_object* jnprj = NULL;
	json_object* jnbom = NULL;
	json_object* jnptype = NULL;
	json_object* jninv = NULL;
	json_object* jlock = NULL;
	json_object* jinit = NULL;
	json_object* jmajor = NULL;
	json_object* jminor = NULL;
	json_object* jpatch = NULL;
	json_object* jptypes = NULL;
	json_object* jinvs = NULL;
	json_object* jitr = NULL;
	json_object* jkey = NULL;
	json_object* jval = NULL;
	
	if( NULL == db ){
		y_log_message(Y_LOG_LEVEL_ERROR, "dbinfo passed was NULL");
		return -1;
	}

	if( NULL == jdb ){
		y_log_message(Y_LOG_LEVEL_ERROR, "json passed was NULL");
		return -1;
	}


	jversion  	= json_object_object_get( jdb, "version" ); 
	jmajor		= json_object_object_get( jversion, "major" ); 
	jminor 		= json_object_object_get( jversion, "minor" ); 
	jpatch 		= json_object_object_get( jversion, "patch" ); 
	jnprj     	= json_object_object_get( jdb, "nprj" ); 
	jnbom     	= json_object_object_get( jdb, "nbom" ); 
	jlock     	= json_object_object_get( jdb, "lock" );
	jinit     	= json_object_object_get( jdb, "init" );
	jptypes		= json_object_object_get( jdb, "ptypes" );
	jinvs		= json_object_object_get( jdb, "invs" );


	//y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object passed for dbinfo:\n%s\n", json_object_to_json_string_ext(jdb, JSON_C_TO_STRING_PRETTY));
	
	/* Check if incoming data was even parsed properly */
	if( NULL == jversion  || NULL == jmajor || NULL == jminor
		|| NULL == jpatch || NULL == jnprj || NULL == jnbom
		|| NULL == jlock  || NULL == jinit || NULL == jptypes
		|| NULL == jinvs ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Issue parsing dbinfo");
		return -1;
	}

	/* Copy data from json objects */

	/* Number of kinds of objects  */
	db->nprj  = json_object_get_int64( jnprj );
	db->nbom  = json_object_get_int64( jnbom );
	db->nptype = (unsigned int)json_object_array_length( jptypes );
	db->ninv = (unsigned int)json_object_array_length( jinvs );

	/* Flags */
	db->flags  = 0;
	db->flags |= json_object_get_int64( jinit ) << DBINFO_FLAG_INIT_SHFT;
	db->flags |= json_object_get_int64( jlock ) << DBINFO_FLAG_LOCK_SHFT;

	/* Version information */
	db->version.major = json_object_get_int64( jmajor );
	db->version.minor = json_object_get_int64( jminor );
	db->version.patch = json_object_get_int64( jpatch );

	/* Part Type table */
	if( db->nptype != 0 ){
		db->ptypes = calloc( db->nptype, sizeof( struct dbinfo_ptype_t ) );
		if( NULL == db->ptypes ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part type array" );
			free_dbinfo_t( db );
			db = NULL;
			return -1;
		}
		for( unsigned int i = 0; i < db->nptype; i++ ){
			/* Loop through array to get info */
			jitr = json_object_array_get_idx( jptypes, (int) i );
			jkey = json_object_object_get( jitr, "type" );
			jval = json_object_object_get( jitr, "npart" );

			/* Part type */
			jstrlen = json_object_get_string_len( jkey );
			db->ptypes[i].name = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == db->ptypes[i].name  ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in dbinfo part type name" );
				free_dbinfo_t( db );
				db = NULL;
				return -1;
			}
			strncpy( db->ptypes[i].name, json_object_get_string( jkey ), jstrlen );

			/* Quantity of parts */
			db->ptypes[i].npart = json_object_get_int64( jval );

		}
	}

	/* Inventory String Lookup */
	if( db->ninv != 0 ){
		db->invs = calloc( db->ninv, sizeof( struct inv_lookup_t ) );
		if( NULL == db->invs ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for inventory lookup array" );
			free_dbinfo_t( db );
			db = NULL;
			return -1;
		}
		for( unsigned int i = 0; i < db->ninv; i++ ){
			/* Loop through array to get info */
			jitr = json_object_array_get_idx( jinvs, (int) i );
			jkey = json_object_object_get( jitr, "name" );
			jval = json_object_object_get( jitr, "loc" );

			/* Part type */
			jstrlen = json_object_get_string_len( jkey );
			db->invs[i].name = calloc( jstrlen + 1, sizeof( char ) );
			if( NULL == db->invs[i].name  ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory in dbinfo inventory lookup name" );
				free_dbinfo_t( db );
				db = NULL;
				return -1;
			}
			strncpy( db->invs[i].name, json_object_get_string( jkey ), jstrlen );

			/* Index number */
			db->invs[i].loc = json_object_get_int64( jval );
		}
	}


	return 0;
}



/* Free the part structure */
void free_part_t( struct part_t* part ){
	if( NULL != part ){
//		y_log_message( Y_LOG_LEVEL_DEBUG, "Freeing part:%d", part->ipn );
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

		if( NULL != part->inv ){
			for( unsigned int i = 0; i < part->inv_len; i++ ){
				part->inv[i].q = 0;
				part->inv[i].loc = 0;
			}

			/* Finished iteration, free array pointer */
			free( part->inv );
			part->inv = NULL;
			part->inv_len = 0;
		}

		/* Free allocated memory for the structure */
		free( part );
		part = NULL;
	}
}

/* Free the part structure, don't touch the part passed as it should be an
 * address, not an allocated pointer */
void free_part_addr_t( struct part_t* part ){
	if( NULL != part ){
//		y_log_message( Y_LOG_LEVEL_DEBUG, "Freeing part:%d", part->ipn );
		/* Zero out other data */
		part->ipn = 0;
		part->q = 0;
		part->status = pstat_unknown;
		
		/* Free all the strings if not NULL */
		if( NULL !=  part->type ){
			memset( part->type, 0, sizeof( part->type ) );
		}
		if( NULL != part->mfg ){
			memset( part->mfg, 0, sizeof( part->mfg ) );
		}
		if( NULL != part->mpn ){
			memset( part->mpn, 0, sizeof( part->mpn ) );
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

		if( NULL != part->inv ){
			for( unsigned int i = 0; i < part->inv_len; i++ ){
				part->inv[i].q = 0;
				part->inv[i].loc = 0;
			}

			/* Finished iteration, free array pointer */
			free( part->inv );
			part->inv = NULL;
			part->inv_len = 0;
		}
	}
}


/* Free the bom structure */
void free_bom_t( struct bom_t* bom ){
	if( NULL != bom ){
//		y_log_message( Y_LOG_LEVEL_DEBUG, "Freeing bom:%d", bom->ipn );
		bom->ipn = 0;

		/* Free all the arrays if not NULL */
		if( NULL !=  bom->line ){
			for( unsigned int i = 0; i < bom->nitems; i++){
				if( NULL != bom->line[i].type ){
					free( bom->line[i].type );
					bom->line[i].type = NULL;
				}
			}
			free( bom->line );
			bom->line = NULL;
		}
		if( NULL != bom->name ){
			free( bom->name );
			bom->name = NULL;
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

		/* Free allocated memory for the structure */
		free( bom );
		bom = NULL;

	}
}

/* Free the project structure */
void free_proj_t( struct proj_t* prj ){
	/* Check if already NULL */
	if( NULL != prj ){
//		y_log_message( Y_LOG_LEVEL_DEBUG, "Freeing project:%d", prj->ipn );
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
		/* Free allocated memory for the structure */
		free( prj );
		prj = NULL;
	}
}

/* Free the database structure */
void free_dbinfo_t( struct dbinfo_t* db ){
	if( NULL != db ){
		/* Zero out easy data */
		db->flags = 0;
		db->nprj = 0;
		db->nbom = 0;
		db->version.major = 0;
		db->version.minor = 0;
		db->version.patch = 0;
	}
	if( NULL != db->ptypes ){
		for( unsigned int i = 0; i < db->nptype; i++ ){
			if( NULL != db->ptypes[i].name ){
				free( db->ptypes[i].name);
				db->ptypes[i].name = NULL;
				db->ptypes[i].npart = 0;
			}
		}
		free( db->ptypes );
	}
	db->nptype = 0;

	if( NULL != db->invs ){
		for( unsigned int i = 0; i < db->ninv; i++ ){
			if( NULL != db->invs[i].name ){
				free( db->invs[i].name);
				db->invs[i].name = NULL;
				db->invs[i].loc = 0;
			}
		}
		free( db->invs );
	}
	db->ninv = 0;
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
	json_object *inv = json_object_new_array_ext(part->inv_len);


	json_object* dist_itr;
	json_object* inv_itr;
	json_object* price_itr;


	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");
	/* Add all parts of the part struct to the json object */
	json_object_object_add( part_root, "ipn", json_object_new_int64(part->ipn) ); /* This should be generated somehow */
	json_object_object_add( part_root, "q", json_object_new_int64(part->q) );
	json_object_object_add( part_root, "status", json_object_new_int64(part->status) );
	json_object_object_add( part_root, "type", json_object_new_string( part->type ) );
	json_object_object_add( part_root, "mfg", json_object_new_string( part->mfg ) );

	if( NULL == part_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	/* Sanitize string for mpn search */
	char* mpn_sanitized = sanitize_redis_string( part->mpn );
	if( NULL == mpn_sanitized ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not sanitize string for %s", part->mpn );	
		json_object_put( part_root );
		return -1;
	}

	json_object_object_add( part_root, "mpn", json_object_new_string( part->mpn ) );

	if( NULL == info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part info json object" );
		json_object_put(part_root);
//		free( mpn_sanitized );
		return -1;	
	}

	if( NULL == dist ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part distributor info json object" );
		json_object_put(info);
		json_object_put(part_root);
//		free( mpn_sanitized );
		return -1;	
	}

	if( NULL == price ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part price info json object" );
		json_object_put(info);
		json_object_put(dist);
		json_object_put(part_root);
//		free( mpn_sanitized );
		return -1;	
	}

	if( NULL == inv ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part inventory info json object" );
		json_object_put(info);
		json_object_put(dist);
		json_object_put(price);
		json_object_put(part_root);
//		free( mpn_sanitized );
		return -1;	
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object before array:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	/* Add all custom fields to info */
	if( part->info_len != 0 ) {
		for( unsigned int i = 0; i < part->info_len; i++ ){
			json_object_object_add( info, part->info[i].key, json_object_new_string(part->info[i].val) );
		}
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object info:\n%s\n", json_object_to_json_string_ext(info, JSON_C_TO_STRING_PRETTY));
		json_object_object_add( part_root, "info", info );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object before after info:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}

	/* Distributor information */
	if( part->dist_len != 0 ) {
		for( unsigned int i = 0; i < part->dist_len; i++ ){
			dist_itr = json_object_new_object();
			if( NULL == dist_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part distributor iterator json object" );	
		//		json_object_put(info);
				json_object_put(dist);
				json_object_put(price);
				json_object_put( part_root );
//		free( mpn_sanitized );
				return -1;
			}
			json_object_object_add( dist_itr, "name", json_object_new_string(part->dist[i].name) );
			json_object_object_add( dist_itr, "pn", json_object_new_string(part->dist[i].pn) );
			json_object_array_put_idx( dist, i, dist_itr );
		}
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object dist:\n%s\n", json_object_to_json_string_ext(dist, JSON_C_TO_STRING_PRETTY));
		json_object_object_add( part_root, "dist", dist );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object after dist:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}

	/* Price information */
	if( part->price_len != 0 ) {
		for( unsigned int i = 0; i < part->price_len; i++ ){
			price_itr = json_object_new_object();
			if( NULL == price_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part price iterator json object" );	
//				json_object_put(info);
//				json_object_put(dist);
				json_object_put(price);
				json_object_put( part_root );
				return -1;
			}			
			json_object_object_add( price_itr, "break", json_object_new_int64(part->price[i].quantity) );
			json_object_object_add( price_itr, "price", json_object_new_double(part->price[i].price) );
			json_object_array_put_idx( price, i, price_itr );
		}
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object price:\n%s\n", json_object_to_json_string_ext(price, JSON_C_TO_STRING_PRETTY));
		json_object_object_add( part_root, "price", price );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object after price:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}

	/* Inventory information */
	if( part->inv_len != 0 ) {
		for( unsigned int i = 0; i < part->inv_len; i++ ){
			inv_itr = json_object_new_object();
			if( NULL == inv_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part inventory iterator json object" );	
//				json_object_put(info);
//				json_object_put(dist);
//				json_object_put(price);
				json_object_put(inv);
				json_object_put( part_root );
//		free( mpn_sanitized );
				return -1;
			}			
			json_object_object_add( inv_itr, "loc", json_object_new_int64(part->inv[i].loc) );
			json_object_object_add( inv_itr, "q", json_object_new_int64(part->inv[i].q) );
			json_object_array_put_idx( inv, i, inv_itr );
		}
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object inv:\n%s\n", json_object_to_json_string_ext(inv, JSON_C_TO_STRING_PRETTY));
		json_object_object_add( part_root, "inv", inv );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object after inv:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

//	dbpart_name = calloc( strlen(part->mpn) + strlen("part:") + strlen(part->type) + 3, sizeof( char ) );

	/* create name */
	asprintf( &dbpart_name, "part:%s:%u", part->type, part->ipn);
	int retval = -1;
	if( NULL == dbpart_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbpart_name");
		retval = -1;
	}
	else{
		y_log_message(Y_LOG_LEVEL_DEBUG, "Created name: %s", dbpart_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbpart_name, "$", json_object_to_json_string(part_root) );
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}



	/* Cleanup json object */
	free( dbpart_name );
	json_object_put( part_root );
//		free( mpn_sanitized );

	return retval;

}

/* Copy part structure to new structure */
struct part_t* copy_part_t( struct part_t* src ){
	struct part_t* dest;
	
	/* Check if source is valid */
	if( NULL == src ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Source for part copy is invalid" );
		return NULL;
	}

	y_log_message( Y_LOG_LEVEL_DEBUG, "IPN Of Part to copy: %d", src->ipn );

	/* Allocate memory for struct */
	dest = calloc( 1, sizeof( struct part_t ) );
	if( NULL == dest ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part");
		return NULL;
	}

	/* Copy data over */
	dest->ipn = src->ipn;
	dest->q = src->q;
	dest->type = calloc( strlen(src->type) + 1, sizeof( char ) );
	if( NULL == dest->type ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part type" );
		free_part_t( dest );
		return NULL;
	}
	strcpy( dest->type, src->type );

	dest->mfg = calloc( strlen(src->mfg) + 1, sizeof( char ) );
	if( NULL == dest->mfg ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part manufacturer" );
		free_part_t( dest );
		return NULL;
	}
	strcpy( dest->mfg, src->mfg );

	dest->mpn = calloc( strlen(src->mpn) + 1, sizeof( char ) );
	if( NULL == dest->mpn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part manufacturer part number" );
		free_part_t( dest );
		return NULL;
	}
	strcpy( dest->mpn, src->mpn );

	dest->status = src->status;

	dest->info_len = src->info_len;

	dest->info = calloc( src->info_len, sizeof( struct part_info_t ) );
	if( NULL == dest->info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part info fields" );
		free_part_t( dest );
		return NULL;
	}
	for( unsigned int i = 0; i < dest->info_len; i++ ){
		dest->info[i].key = calloc( (strlen( src->info[i].key ) + 1), sizeof( char ) );
		if( NULL == dest->info[i].key){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part info key %d", i );
			free_part_t( dest );
			return NULL;
		}
		strcpy( dest->info[i].key, src->info[i].key );

		dest->info[i].val = calloc( (strlen( src->info[i].val ) + 1), sizeof( char ) );
		if( NULL == dest->info[i].val){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part info value %d", i );
			free_part_t( dest );
			return NULL;
		}
		strcpy( dest->info[i].val, src->info[i].val );
	}


	dest->dist_len = src->dist_len;

	dest->dist = calloc( src->dist_len, sizeof( struct part_dist_t ) );
	if( NULL == dest->dist ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part distributor fields" );
		free_part_t( dest );
		return NULL;
	}
	for( unsigned int i = 0; i < dest->dist_len; i++ ){
		dest->dist[i].name = calloc( (strlen( src->dist[i].name ) + 1), sizeof( char ) );
		if( NULL == dest->info[i].key){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part distributor name %d", i );
			free_part_t( dest );
			return NULL;
		}
		strcpy( dest->dist[i].name, src->dist[i].name );

		dest->dist[i].pn = calloc( (strlen( src->dist[i].pn ) + 1), sizeof( char ) );
		if( NULL == dest->dist[i].pn){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part distributor part number %d", i );
			free_part_t( dest );
			return NULL;
		}
		strcpy( dest->dist[i].pn, src->dist[i].pn );
	}

	dest->price_len = src->price_len;

	dest->price = calloc( src->price_len, sizeof( struct part_price_t ) );
	if( NULL == dest->price ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part price breaks" );
		free_part_t( dest );
		return NULL;
	}
	for( unsigned int i = 0; i < dest->price_len; i++ ){
		dest->price[i].quantity = src->price[i].quantity;	
		dest->price[i].price = src->price[i].price;
	}

	dest->inv_len = src->inv_len;

	dest->inv = calloc( src->inv_len, sizeof( struct part_inv_t ) );
	if( NULL == dest->price ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination part inventory" );
		free_part_t( dest );
		return NULL;
	}
	for( unsigned int i = 0; i < dest->inv_len; i++ ){
		dest->inv[i].q = src->inv[i].q;	
		dest->inv[i].loc = src->inv[i].loc;
	}


	return dest;
}

/* Copy bom structure to new structure */
struct bom_t* copy_bom_t( struct bom_t* src ){
	struct bom_t* dest;
	
	/* Check if source is valid */
	if( NULL == src ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Source for bom copy is invalid" );
		return NULL;
	}

	y_log_message( Y_LOG_LEVEL_DEBUG, "IPN Of BOM to copy: %d", src->ipn );

	/* Allocate memory for struct */
	dest = calloc( 1, sizeof( struct bom_t ) );
	if( NULL == dest ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination bom");
		return NULL;
	}

	/* Copy data over */
	dest->ipn = src->ipn;
	dest->name = calloc( strlen(src->name) + 1, sizeof( char ) );
	if( NULL == dest->name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination bom name" );
		free_bom_t( dest );
		return NULL;
	}
	strcpy( dest->name, src->name );

	dest->ver = calloc( strlen(src->ver) + 1, sizeof( char ) );
	if( NULL == dest->ver ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination bom version" );
		free_bom_t( dest );
		return NULL;
	}
	strcpy( dest->ver, src->ver );


	dest->nitems = src->nitems;
	
	/* Create line items */
	dest->line = calloc( src->nitems, sizeof( struct bom_line_t ) );
	if( NULL == dest->line ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination bom line items" );
		free_bom_t( dest );
		return NULL;	
	}

	/* Wait to copy data until parts array is also created, saves another for
	 * loop */

	dest->parts = calloc( src->nitems, sizeof( struct part_t* ) );
	if( NULL == dest->parts ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination bom parts" );
		free_bom_t( dest );
		return NULL;
	}
	for( unsigned int i = 0; i < dest->nitems; i++ ){
		dest->parts[i] = copy_part_t( src->parts[i] );

		if( NULL == dest->parts[i] ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Failed to copy part to destination bom" );
			free_bom_t( dest );
			return NULL;			
		}

		/* Now copy line item data */
		dest->line[i].ipn = src->line[i].ipn;
		dest->line[i].q = src->line[i].q;

	}


	return dest;
}

/* Copy dbinfo structure to new structure */
struct dbinfo_t* copy_dbinfo_t( struct dbinfo_t* src ){
	struct dbinfo_t* dest;
	mutex_spin_lock_dbinfo();

	/* Check if valid source */
	if( NULL == src ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Source for dbinfo copy is invalid");
		return NULL;
	}

	/* Allocate memory for struct */
	dest = calloc( 1, sizeof( struct dbinfo_t ) );
	if( NULL == dest ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for destination dbinfo");
		return NULL;
	}

	/* Copy data over */
	dest->flags = src->flags;
	dest->nprj = src->nprj;
	dest->nbom = src->nbom;
	dest->nptype = src->nptype;
	dest->version.major = src->version.major;
	dest->version.minor = src->version.minor;
	dest->version.patch = src->version.patch;

	/* part type copy */
	dest->ptypes = calloc( dest->nptype, sizeof( struct dbinfo_ptype_t ) );
	if( NULL == dest->ptypes ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbinfo ptype array");
		free_dbinfo_t( dest );
		dest = NULL;
		return NULL;
	}
	for( unsigned int i = 0; i < dest->nptype; i++ ){
		dest->ptypes[i].npart = src->ptypes[i].npart;	
		dest->ptypes[i].name = calloc( strlen( src->ptypes[i].name ) + 1, sizeof( char ) );
		if( NULL == dest->ptypes[i].name ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbinfo ptype name");
			free_dbinfo_t( dest );
			dest = NULL;
			return NULL;
		}
		strcpy(dest->ptypes[i].name, src->ptypes[i].name );
	}


	/* Inventory lookup copy */
	dest->invs = calloc( dest->ninv, sizeof( struct inv_lookup_t ) );
	if( NULL == dest->invs ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbinfo location array");
		free_dbinfo_t( dest );
		dest = NULL;
		return NULL;
	}
	for( unsigned int i = 0; i < dest->ninv; i++ ){
		dest->invs[i].loc = src->invs[i].loc;	
		dest->invs[i].name = calloc( strlen( src->invs[i].name ) + 1, sizeof( char ) );
		if( NULL == dest->invs[i].name ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbinfo inventory location name");
			free_dbinfo_t( dest );
			dest = NULL;
			return NULL;
		}
		strcpy(dest->invs[i].name, src->invs[i].name );
	}

	mutex_unlock_dbinfo();
	return dest;
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
	json_object *line = json_object_new_array_ext(bom->nitems);
	json_object *line_itr = NULL;


	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");
	/* Add all parts of the part struct to the json object */
	json_object_object_add( bom_root, "ipn", json_object_new_int64(bom->ipn) ); /* This should be generated somehow */
	json_object_object_add( bom_root, "ver", json_object_new_string(bom->ver) );
	json_object_object_add( bom_root, "items", line );
	json_object_object_add( bom_root, "name", json_object_new_string(bom->name) );

	if( NULL == bom_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	if( NULL == line ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new bom items json object" );
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
				json_object_put(line);
				json_object_put(bom_root);
				return -1;
			}			
			json_object_object_add( line_itr, "type", json_object_new_string(bom->line[i].type) );
			json_object_object_add( line_itr, "ipn", json_object_new_int64(bom->line[i].ipn) );
			json_object_object_add( line_itr, "q", json_object_new_int64(bom->line[i].q) );
			json_object_array_put_idx( line, i, line_itr );
		}

	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");




//	dbbom_name = calloc( strlen("bom:") + 2 + 16, sizeof( char ) ); /* IPN has to be less than 16 characters right now */
	asprintf( &dbbom_name, "bom:%d:%s", bom->ipn, bom->ver);

	int retval = -1;
	if( NULL == dbbom_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbbom_name");
		retval = -1;
	}
	else{
		/* create name */
//		sprintf( dbbom_name, "bom:%d", bom->ipn);

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
//		json_object_put(sub);
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
			json_object_array_put_idx( boms, i, bom_itr);
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
			json_object_array_put_idx( sub, i, sub_itr );
		}
		
	}



	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

//	dbprj_name = calloc( strlen("prj:") + 2 + 16, sizeof( char ) ); /* IPN has to be less than 16 characters right now */
	/* create name */
	asprintf( &dbprj_name, "prj:%d", prj->ipn);
	int retval = -1;
	if( NULL == dbprj_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbprj_name");
		retval = -1;
	}
	else{
		y_log_message(Y_LOG_LEVEL_DEBUG, "Created name: %s", dbprj_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbprj_name, "$", json_object_to_json_string(prj_root) );
//		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(prj_root, JSON_C_TO_STRING_PRETTY));
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
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %s", pn);
		return NULL;
	}

	if( NULL == pn ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Was not passed valid string for part number search");
		return NULL;
	}

	/* Allocate space for part */
	part = calloc( 1, sizeof( struct part_t ) );
	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part: %s", pn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for part number */
	struct json_object* jpart;
	redisReply* reply = NULL;
	/* Sanitize input */
	char* pn_sanitized = sanitize_redis_string( pn );
	y_log_message( Y_LOG_LEVEL_DEBUG, "Sanitized part number: %s", pn_sanitized ); 
	redis_json_index_search( rc, &reply, "partid", "mpn", pn_sanitized );
	if( pn_sanitized != pn ){
		/* New string was created, free the memory */
		free( pn_sanitized );
	}

	/* Returns back a few elements; need to grab the second one to get the
	 * right object */
	if( NULL == reply || reply->type != REDIS_REPLY_ARRAY || reply->elements != 3 ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database search did not reply correctly for part pn %s", pn );
		free( part ); /* don't need to free other parts of array as they have not been allocated yet */
		part = NULL;
	}
	else {

		/* Should be correct response, get second element parsed into jbom */
		jpart = json_tokener_parse( reply->element[2]->element[1]->str );	
	 	if( NULL != jpart ){
			/* Parse the json */
			parse_json_part( part, jpart );

			/* Free data */
			json_object_put( jpart );
		}
		else {
			free( part );
			part = NULL;
		}
	}

	/* Free redis reply */
	freeReplyObject(reply);

	return part;

}

/* Create struct from parsed item in database, from internal part number */
struct part_t* get_part_from_ipn( const char* type, unsigned int ipn ){
	struct part_t * part = NULL;	
	char* dbpart_name = NULL;
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
	y_log_message(Y_LOG_LEVEL_DEBUG, "Part type: %s; part IPN: %ld", type, ipn);

	asprintf( &dbpart_name, "part:%s:%d", type, ipn);

	/* Get part number, store in json object */
	if( !redis_json_get( rc, dbpart_name, "$", &jpart ) ){
		/* Parse the json */
		parse_json_part( part, jpart );

		/* free json data; will segfault otherwise */
		json_object_put( jpart );
	}
	else {
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not get part for %ld", ipn);
		part = NULL;
	}

	/* Free json */
	free( dbpart_name );

	return part;

}

/* Create bom struct from parsed item in database, from internal part number */
struct bom_t* get_bom_from_ipn( unsigned int ipn, char* version ){
	struct bom_t * bom = NULL;	
	char* dbbom_name = NULL;
//	char ipn_s[32] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of bom: %ld", ipn);
		return NULL;
	}

	/* Allocate size for database name
	 * should be in format of bom:ipn:version */
//	dbbom_name = calloc( strlen(ipn) + strlen("bom:") + strnlen(version, 32) + 2, sizeof( char ) );
	asprintf( &dbbom_name, "bom:%d:%s", ipn, version);

	if( NULL == dbbom_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbbom_name");
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

	/* Get bom, store in json object */
	if( !redis_json_get( rc, dbbom_name, "$", &jbom ) ){
		/* Parse the json */
		parse_json_bom( bom, jbom );

		/* free json data; will segfault otherwise */
//		json_object_put( jbom );
	}
	else {
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not get bom for %s", dbbom_name);
		free_bom_t( bom );
		bom = NULL;
	}
	

#if 0
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

	/* Free redis reply */
	freeReplyObject(reply);

	/* Parse the json */
	parse_json_bom( bom, jbom );

#endif

	/* Free json */
	json_object_put( jbom );

	/* Free database name */
	free( dbbom_name );

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
	snprintf( ipn_s, sizeof( ipn_s ), "[ %ld %ld ]", ipn, ipn);
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
	/* Check if correct number of elements in array */
	else if( reply->elements < 3 ) {
		y_log_message( Y_LOG_LEVEL_WARNING, "Expected more elements in database reply for project:%d. Received %d", ipn, reply->elements );
		free( prj );
		freeReplyObject(reply);
		return NULL;
		
	}

	else if( NULL == reply->element[2] ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Unexpected null element in database reply for project:%d", ipn);
		free( prj );
		freeReplyObject(reply);
		return NULL;
	}
	else if( reply->element[2]->elements < 2 ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Expected more subelements in database reply for project:%d. Received %d", ipn, reply->element[2]->elements );
		free( prj );
		freeReplyObject(reply);
		return NULL;
	}
	else if( NULL == reply->element[2]->element[1] ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Unexpected null subelement in database reply for project:%d", ipn);
		free( prj );
		freeReplyObject(reply);
		return NULL;
	}
	else if ( NULL == reply->element[2]->element[1]->str ) {
		y_log_message(Y_LOG_LEVEL_WARNING, "Unexpected missing data for project%d. Does the project exist?", ipn);
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
	json_object_put( jprj );
	return prj;
}

/* Read database information */
struct dbinfo_t* redis_read_dbinfo( void ){
	json_object* jdb;

	struct dbinfo_t* tmp = NULL;

	tmp = calloc( 1, sizeof( struct dbinfo_t ) );
	if( NULL == tmp ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for database info" );
		return NULL;
	}

	if( redis_json_get( rc, "popdb", "$", &jdb ) ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not get database information");
		free_dbinfo_t( tmp );
		/* If failed, make sure to unlock */
		tmp = NULL;
	}

	/* Parse data */
	if( parse_json_dbinfo( tmp, jdb ) ){
		/* Error occurred, free memory */
		free_dbinfo_t( tmp );
		tmp = NULL;
	}

	json_object_put( jdb );
	return tmp;
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
	json_object* ptypes	 	 = json_object_new_array_ext(db->nptype);
	json_object* invs		 = json_object_new_array_ext(db->ninv);
	json_object* inv_itr;
	json_object* ptype_itr;

	int lock = ((db->flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK);
	int init = ((db->flags & DBINFO_FLAG_INIT) == DBINFO_FLAG_INIT);


	json_object_object_add( version, "major", json_object_new_int64( db->version.major ) );
	json_object_object_add( version, "minor", json_object_new_int64( db->version.minor ) );
	json_object_object_add( version, "patch", json_object_new_int64( db->version.patch ) );

	/* Add all parts of the part struct to the json object */
	json_object_object_add( dbinfo_root, "version", version );
	json_object_object_add( dbinfo_root, "nprj", json_object_new_int64(db->nprj) );
	json_object_object_add( dbinfo_root, "nbom", json_object_new_int64(db->nbom) );
	json_object_object_add( dbinfo_root, "init", json_object_new_int64( init ) );
	json_object_object_add( dbinfo_root, "lock", json_object_new_int64( lock ) );
	json_object_object_add( dbinfo_root, "ptypes", ptypes );
	json_object_object_add( dbinfo_root, "invs", invs );


	if( NULL == dbinfo_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root for dbinfojson object" );
		return -1;
	}

	if( NULL == version ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate dbinfo version json object" );
		json_object_put(dbinfo_root);
		return -1;	
	}

	if( NULL == ptypes ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate dbinfo part types json object" );
//		json_object_put(version);
		json_object_put(dbinfo_root);
		return -1;	
	}

	if( NULL == invs ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate dbinfo inventory lookup json object" );
//		json_object_put(ptypes);
//		json_object_put(version);
		json_object_put(dbinfo_root);
		return -1;	
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Adding json objects");

	/* Part type quantities */
	if( db->nptype != 0 ){
		for( unsigned int i = 0; i < db->nptype; i++ ){
			ptype_itr = json_object_new_object();	
			if( NULL == inv_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate part type iterator json object " );
//				json_object_put(invs);
//				json_object_put(ptypes);
//				json_object_put(version);
				json_object_put(dbinfo_root);
				free_dbinfo_t( db );
				return -1;
			}
			json_object_object_add( ptype_itr, "type", json_object_new_string( db->ptypes[i].name ) );
			json_object_object_add( ptype_itr, "npart", json_object_new_int64( db->ptypes[i].npart ) );
			json_object_array_put_idx( ptypes, i, ptype_itr );
		} 
	}

	/* Store inventory lookup table */
	if( db->ninv != 0 ){
		for( unsigned int i = 0; i < db->ninv; i++ ){
			inv_itr = json_object_new_object();	
			if( NULL == inv_itr ){
				y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate inventory lookup iterator json object " );
//				json_object_put(invs);
//				json_object_put(ptypes);
//				json_object_put(version);
				json_object_put(dbinfo_root);
				free_dbinfo_t( db );
				return -1;
			}
			json_object_object_add( inv_itr, "name", json_object_new_string( db->invs[i].name ) );
			json_object_object_add( inv_itr, "loc", json_object_new_int64( db->invs[i].loc ) );
			json_object_array_put_idx( invs, i, inv_itr );
		} 
	}



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
#if 0
	struct dbinfo_t* db_check;
	do{
		db_check = redis_read_dbinfo();
		if( (db_check->flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK ){
			sleep(2); /* Wait a little until lock is free */
		}

	} while( (db_check->flags & DBINFO_FLAG_LOCK) == DBINFO_FLAG_LOCK );

	/* Cleanup temporary info */
	free_dbinfo_t( db_check );
	free( db_check );
#endif

	/* now write data */
	return write_dbinfo( db );

}

/* Lock access to dbinfo */
int mutex_lock_dbinfo( void ){
	return lock( &dbinfo_mtx );
}
void mutex_spin_lock_dbinfo( void ){
	while( lock( &dbinfo_mtx ) );
}

/* Unlock access to dbinfo */
void mutex_unlock_dbinfo( void ){
	unlock( &dbinfo_mtx );
}

/* Get inventory index number from string */
int dbinv_str_to_loc( struct dbinfo_t** info, const char* s, size_t len ){
	int index = -1;
	mutex_spin_lock_dbinfo();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Inventory location passed: %s", s );
	for( unsigned int i = 0; i < (*info)->ninv; i++){
		y_log_message(Y_LOG_LEVEL_DEBUG, "dbinfo inv[%u]:%s", i, (*info)->invs[i].name );
		if( !strncmp( (*info)->invs[i].name, s, len ) ){
			y_log_message( Y_LOG_LEVEL_DEBUG, "Found matching inventory location" );
			index = i;
			break;
		}
			
	}
	mutex_unlock_dbinfo();
	return index;
}

/* Get the index of the requested part type in the database */
unsigned int dbinfo_get_ptype_index( struct dbinfo_t** info, const char* type ){
	unsigned int index = (unsigned int)-1;
	/* Find existing part type in database info */
	mutex_spin_lock_dbinfo();
	y_log_message(Y_LOG_LEVEL_DEBUG, "Part type to search: %s", type );
	for( unsigned int i = 0; i < (*info)->nptype; i++){
		y_log_message(Y_LOG_LEVEL_DEBUG, "dbinfo ptype[%u]:%s", i, (*info)->ptypes[i].name );
		if( !strncmp( (*info)->ptypes[i].name, type, sizeof( type ) ) ){
			y_log_message( Y_LOG_LEVEL_DEBUG, "Found matching part type" );
			index = i;
		}
			
	}
	mutex_unlock_dbinfo();
	return index;
}
