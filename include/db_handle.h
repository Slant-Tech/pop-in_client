#ifndef DB_HANDLE_H
#define DB_HANDLE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* Struct for overall database information */
struct dbinfo_t {
	unsigned int version;	/* Database version for keeping everything in sync */
	uint32_t flags;			/* Option flags */
};

/* dbinfo_t flags */
#define DBINFO_FLAG_INITIALIZED	0x0001 	/* Database has been initialized */
#define DBINFO_FLAG_LOCK		0x0002	/* Lock on database; should not edit when locked */


/* Key Value pair for part info*/
struct part_info_t {
	char* key;	
	char* val;
};

/* Structure for parts */
struct part_t {
	unsigned int ipn;			/* Internal part number */
	unsigned int q;				/* Quantity/Stock */
	unsigned int info_len;		/* Number of key value pairs in info */
	char* type;					/* Part type */
	char* mfg;					/* Manufacturer */
	char* mpn;					/* Manufacturer Part Number */
	struct part_info_t* info; 	/* Key Value info for dynamic part definitions */
};

/* Create struct from parsed item in database, from part number */
struct part_t* get_part_from_pn( const char* pn );

/* Create struct from parsed item in database, from internal part number */
struct part_t* get_part_from_ipn( unsigned int pn );

/* Free the part structure */
void free_part_t( struct part_t* part );

/* Write part to database */
int redis_write_part( struct part_t* part );

/* Import file to database */
int redis_import_part_file( char* filepath );

/* Connect to redis database */
int redis_connect( const char* hostname, int port );

/* Free memory for redis connection */
void redis_disconnect( void );

#ifdef __cplusplus
}
#endif


#endif
