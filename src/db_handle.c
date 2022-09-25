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
	
	jipn  = json_object_object_get( jpart, "ipn" );
	jq    = json_object_object_get( jpart, "q" );
	jtype = json_object_object_get( jpart, "type" );
	jmfg  = json_object_object_get( jpart, "mfg" );
	jmpn  = json_object_object_get( jpart, "mpn" );
	jinfo = json_object_object_get( jpart, "info" );
	


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

	/* Get number of elements in info key value array */
	part->info_len = json_object_object_length( jinfo );

	/* Allocate memory for information of part */
	part->info = malloc( sizeof( struct part_info_t ) * part->info_len );
	if( NULL == part->info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for info of part");
		free( part );
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



}

/* Free the part structure */
void free_part_t( struct part_t* part ){
	
	/* Zero out other data */
	part->ipn = 0;
	part->q = 0;
	
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

}

/* Write part to database */
int redis_write_part( struct part_t* part ){
	/* Database part name */
	char * dbpart_name = NULL;

	printf("Creating json objects\n");

	/* Create json object to write */
	json_object *part_root = json_object_new_object();
	json_object *info = json_object_new_object();

	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Part struct passed when writing to database was NULL" );
		return -1;
	}

	if( NULL == part_root ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new root json object" );
		return -1;
	}

	if( NULL == info ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate new part info json object" );
		json_object_put(part_root);
		return -1;	
	}

	printf("Adding json objects \n");

	/* Add all custom fields to info */
	if( part->info_len != 0 ) {
		for( unsigned int i = 0; i < part->info_len; i++ ){
			json_object_object_add( info, part->info[i].key, json_object_new_string(part->info[i].val) );
		}
	}
	printf("Parsing \n");

	/* Add all parts of the part struct to the json object */
	json_object_object_add( part_root, "ipn", json_object_new_int64(part->ipn) ); /* This should be generated somehow */
	json_object_object_add( part_root, "q", json_object_new_int64(part->q) );
	json_object_object_add( part_root, "type", json_object_new_string( part->type ) );
	json_object_object_add( part_root, "mfg", json_object_new_string( part->mfg ) );
	json_object_object_add( part_root, "mpn", json_object_new_string( part->mpn ) );
	json_object_object_add( part_root, "info", info );

	printf("Added items to object");

	dbpart_name = malloc( strlen(part->mpn) + strlen("part:") + 2 );

	int retval = -1;
	if( NULL == dbpart_name ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for dbpart_name");
		retval = -1;
	}
	else{
		/* create name */
		sprintf( dbpart_name, "part:%s", part->mpn);

		printf("Created name: %s\n", dbpart_name);

		/* Write object to database */
		retval = redis_json_set( rc, dbpart_name, "$", json_object_to_json_string(part_root) );
		printf("JSON Object to send:\n%s\n", json_object_to_json_string_ext(part_root, JSON_C_TO_STRING_PRETTY));
	}



	/* Cleanup json object */
	free( dbpart_name );
	json_object_put( part_root );

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
		printf("\nJSON Index: %d\n", i);

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
				printf("JSON Object at index[%d]:\n%s\n", i, json_object_to_json_string_ext(jo_idx, JSON_C_TO_STRING_PRETTY));

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
					printf("Cleaned up objects\n");
				}
			}
		}
	}

	/* Cleanup root object */
	printf("Root object cleanup\n");
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
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %s", pn);
		return NULL;
	}

	/* Allocate space for part */
	part = malloc( sizeof( struct part_t ) );
	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part: %s", pn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for part number */
	struct json_object* jpart;
	
	/* Sanitize input? */

	/* Get part number, store in json object */
	redis_json_get( rc, pn, "$", &jpart );

	/* Parse the json */
	parse_json_part( part, jpart );

	/* Free json */
	json_object_put( jpart );

	return part;

}

/* Create struct from parsed item in database, from internal part number */
struct part_t* get_part_from_ipn( unsigned int pn ){
	struct part_t * part = NULL;	
	if( NULL == rc ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Database is not connected. Could not get info of part: %s", pn);
		return NULL;
	}

	/* Allocate space for part */
	part = malloc( sizeof( struct part_t ) );
	if( NULL == part ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not allocate memory for part: %s", pn);
		return NULL;
	}

	/* Memory has been allocated, begin actually doing stuff */

	/* Query database for part number */
	struct json_object* jpart;
	
	/* Sanitize input? */

	/* Get part number from internal part number */
//	redis_json_get( rc, pn, "$", &jpart );

	/* Parse the json */
	parse_json_part( part, jpart );

	/* Free json */
	json_object_put( jpart );

	return part;

}

