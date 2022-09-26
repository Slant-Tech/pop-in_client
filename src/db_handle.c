#include <redis-wrapper/redis-wrapper.h>
#include <redis-wrapper/redis-json.h>
#include <json-c/json.h>
#include <string.h>
#include <yder.h>
#include <db_handle.h>

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
	part->type = malloc( jstrlen * sizeof( char ) );
	if( NULL == part->type ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part type");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->type, json_object_get_string( jtype ), jstrlen );

	/* Manufacturer */
	jstrlen = strlen( json_object_get_string( jmfg ) );
	part->mfg = malloc( jstrlen * sizeof( char ) );
	if( NULL == part->mfg ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part manufacturer");
		free_part_t( part );
		part = NULL;
		return -1;
	}
	strncpy( part->mfg, json_object_get_string( jmfg ), jstrlen );

	/* Manufacturer part number */
	jstrlen = strlen( json_object_get_string( jmpn ) );
	part->mpn = malloc( jstrlen * sizeof( char ) );
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
	part->info = malloc( sizeof( struct part_info_t ) * part->info_len );
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
		part->info[count].key = malloc( (strlen( key ) + 1 ) * sizeof( char ) );
		strcpy( part->info[count].key, key );

		jstrlen = strlen( json_object_get_string( val ) ) + 1;
		part->info[count].val = malloc( jstrlen * sizeof( char ) );
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

/* Parse part json object to part_t */
static int parse_json_bom( struct bom_t * bom, struct json_object* restrict jpart ){

	/* Temporary string length variable */
	size_t jstrlen = 0;

	/* Need to create a bunch of json structs to parse everything out */
	json_object* jipn;
	json_object* jversion;
	json_object* jpart_ipns;
	json_object* jipn_itr;
	
	jipn       = json_object_object_get( jpart, "ipn" );
	jversion   = json_object_object_get( jpart, "ver" ); 
	jpart_ipns = json_object_object_get( jpart, "items" );

	/* Copy data from json objects */

	/* Internal part number */
	bom->ipn = json_object_get_int64( jipn );

	/* Part Type */
	jstrlen = strlen( json_object_get_string( jversion ) );
	bom->ver = malloc( jstrlen * sizeof( char ) );
	if( NULL == bom->ver ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom version");
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}
	strncpy( bom->ver, json_object_get_string( jversion ), jstrlen );

	/* Get number of elements in info key value array */
	bom->nitems = json_object_object_length( jpart_ipns );

	/* Allocate memory for information of part */
	bom->part_ipns = malloc( sizeof(unsigned int) * bom->nitems );
	if( NULL == bom->part_ipns ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part number array in bom");
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}

	for( unsigned int i = 0; i < bom->nitems; i++ ){
		jipn_itr = json_object_array_get_idx( jpart_ipns, (int)i );
		bom->part_ipns[i] = json_object_get_int64( jipn_itr );
	}

	/* Initialize array of parts to be cached later */
	bom->parts = malloc( sizeof( struct part_t ) * bom->nitems );
	if( NULL == bom->parts ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom part array" );
		free_bom_t( bom );
		bom = NULL;
		return -1;
	}

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
	if( NULL !=  bom->part_ipns ){
		free( bom->part_ipns );
		bom->part_ipns = NULL;
	}
	if( NULL != bom->ver ){
		free( bom->ver );
		bom->ver = NULL;
	}
	if( NULL != bom->parts ){
		for( unsigned int i = 0; i < bom->nitems; i++ ){
			free_part_t( &bom->parts[i] );
		}

		/* Iterated through all the info key value pairs, free the array itself */
		free( bom->parts );
		bom->parts = NULL;
		bom->nitems = 0;
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

	if( NULL == dist_itr ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part distributor iterator json object" );	
		json_object_put(info);
		json_object_put(dist);
		json_object_put(price);
		json_object_put( part_root );
		return -1;
	}
	
	if( NULL == price_itr ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part price iterator json object" );	
		json_object_put(info);
		json_object_put(dist);
		json_object_put(price);
		json_object_put(dist_itr);
		json_object_put( part_root );
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
			json_object_object_add( dist_itr, "name", json_object_new_string(part->dist[i].name) );
			json_object_object_add( dist_itr, "pn", json_object_new_string(part->dist[i].pn) );
			json_object_array_put_idx( dist, i, dist_itr );
		}

	}

	/* Price information */
	if( part->price_len != 0 ) {
		for( unsigned int i = 0; i < part->price_len; i++ ){
			dist_itr = json_object_new_object();
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

	dbpart_name = malloc( strlen(part->mpn) + strlen("part:") + 2 );

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
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
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
		y_log_message( Y_LOG_LEVEL_ERROR, "Part struct passed when writing to database was NULL" );
		return -1;
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Creating json objects");

	/* Create json object to write */
	json_object *bom_root = json_object_new_object();
	json_object *ipn = json_object_new_object();
	json_object *version = json_object_new_object();
	json_object *part_ipns = json_object_new_array_ext(bom->nitems);

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

	if( NULL == part_ipns ){
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
			json_object_array_add( part_ipns, json_object_new_int64(bom->part_ipns[i]) );
		}
	}

	y_log_message(Y_LOG_LEVEL_DEBUG, "Parsing");

	/* Add all parts of the part struct to the json object */
	json_object_object_add( bom_root, "ipn", json_object_new_int64(bom->ipn) ); /* This should be generated somehow */
	json_object_object_add( bom_root, "ver", version );
	json_object_object_add( bom_root, "items", part_ipns );

	y_log_message(Y_LOG_LEVEL_DEBUG, "Added items to object");

	dbbom_name = malloc( strlen("bom:") + 2 + 16 ); /* IPN has to be less than 16 characters right now */

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
		y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object to send:\n%s\n", json_object_to_json_string_ext(bom_root, JSON_C_TO_STRING_PRETTY));
	}



	/* Cleanup json object */
	free( dbbom_name );
	json_object_put( bom_root );

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
				y_log_message(Y_LOG_LEVEL_DEBUG, "JSON Object at index[%d]:\n%s\n", i, json_object_to_json_string_ext(jo_idx, JSON_C_TO_STRING_PRETTY));

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

	y_init_logs("Pop:In", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Pop:In Inventory Management");

	if( init_redis( &rc, hostname, port ) ){
		y_log_message(Y_LOG_LEVEL_ERROR, "Could not connect to redis database");
		return -1;
	}
	y_log_message(Y_LOG_LEVEL_INFO, "Established connection to redis database");


	return 0;
}

/* Free memory for redis connection */
void redis_disconnect( void ){
	redisFree( rc );
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
	dbpart_name = malloc( strlen(pn) + strlen("part:") + 2 );

	if( NULL == dbpart_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbpart_name");
		return NULL;
	}

	sprintf( dbpart_name, "part:%s", pn);

	/* Allocate space for part */
	part = malloc( sizeof( struct part_t ) );
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
	char ipn_s[16] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %ld", ipn);
		return NULL;
	}

	/* Allocate space for part */
	part = malloc( sizeof( struct part_t ) );
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
	sprintf( ipn_s, "%ld", ipn);
	redis_json_index_search( rc, reply, "part:", "ipn", ipn_s );

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
	char ipn_s[16] = {0};
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of bom: %ld", ipn);
		return NULL;
	}

	/* Allocate space for part */
	bom = malloc( sizeof( struct bom_t ) );
	if( NULL == bom ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for bom: %ld", ipn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for bom */
	struct json_object* jbom;
	
	/* Sanitize input? */

	/* Search part number by IPN */
	redisReply* reply = NULL;
	sprintf( ipn_s, "%ld", ipn);
	redis_json_index_search( rc, reply, "bom:", "ipn", ipn_s );

	/* Free redis reply */
	freeReplyObject(reply);

	/* Parse the json */
	parse_json_bom( bom, jbom );


	/* Free json */
	json_object_put( jbom );

	return bom;
}

