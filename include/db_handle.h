#ifndef DB_HANDLE_H
#define DB_HANDLE_H


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>

struct dbver_t {
	unsigned int major;
	unsigned int minor;
	unsigned int patch;
};

/* Struct for overall database information */
struct dbinfo_t {
	uint32_t flags;			/* Option flags */
	unsigned int nprj;		/* Number of projects in database */
	unsigned int nbom;		/* Number of boms in database */
	unsigned int npart;		/* Number of parts in database */
	struct dbver_t version;	/* Database version for keeping everything in sync */
};

/* dbinfo_t flags */
#define DBINFO_FLAG_INIT	0x0001 	/* Database has been initialized */
#define DBINFO_FLAG_LOCK	0x0002	/* Lock on database; should not edit when locked */

/* dbinfo_t flag shift */
#define DBINFO_FLAG_INIT_SHFT	0
#define DBINFO_FLAG_LOCK_SHFT			1

/* Key Value pair for part info*/
struct part_info_t {
	char* key;	
	char* val;
};

/* Key Value pair for distributor info */
struct part_dist_t {
	char* name;
	char* pn;
};

/* Key Value pair for pricing */
struct part_price_t {
	int quantity;	/* Price break quantity */
	double price;	/* Price in native currency. Dollars, need to figure out setting for this later */
};

/* Part status information */
enum part_status_t {
	pstat_unknown,
	pstat_prod,
	pstat_low_stock,
	pstat_unavailable,
	pstat_nrnd,
	pstat_obsolete
};

/* Structure for parts */
struct part_t {
	unsigned int ipn;				/* Internal part number */
	unsigned int q;					/* Quantity/Stock */
	enum part_status_t status;		/* Part production status */
	unsigned int info_len;			/* Number of key value pairs in info */
	unsigned int price_len; 		/* Number of price breaks in price */
	unsigned int dist_len;			/* Number of distributors */
	char* type;						/* Part type */
	char* mfg;						/* Manufacturer */
	char* mpn;						/* Manufacturer Part Number */
	struct part_info_t* info; 		/* Key Value info for dynamic part definitions */
	struct part_dist_t* dist;	/* Key Value distributor info */
	struct part_price_t* price;		/* Key Value price break info */
};

/* Structure for part number with qua*/
struct bom_line_t{
	unsigned int ipn;
	unsigned int q;
};

/* Structure for bill of materials */
struct bom_t {
	unsigned int ipn;				/* BOM Internal Part Number */
	unsigned int nitems;			/* Number of bom line items */
	char* name;						/* Name/Title of BOM */
	char* ver;						/* Version */
	struct bom_line_t* line;		/* Bom line; contains part number and quantity for part */
	struct part_t** parts;			/* Parts array (cached values for parts) */
};

/* For getting specific versions of BOM */
struct proj_bom_ver_t{
	char* ver;
	struct bom_t* bom;
};

/* For getting specific versions of subprojects */
struct proj_subprj_ver_t{
	char* ver;
	struct proj_t* prj;
};

/* Project flags */
#define PROJ_FLAG_DIRTY				0x0001 /* Edited locally, should be pushed to database */
#define PROJ_FLAG_STALE				0x0002 /* Data is old, should be refreshed */

/* Structure for project */
struct proj_t {
	int flags;						/* Project handling flags. Not stored in database */
	int selected;					/* If selected in UI or not. Can't use bool because of c/c++ api differences */	
	unsigned int ipn;				/* Internal part number */
	int nsub;						/* Number of subprojects */
	int nboms;						/* Number of boms in project */
	time_t time_created;			/* Date stamp of when project was created */
	time_t time_mod;				/* Date stamp of when project was last modified */
	char* ver;						/* Project version */
	char* name;						/* Project name */
	char* pn;						/* Part number */
	char* author;					/* Author of project, maybe department? Could be useful for a number of things. */
	struct proj_subprj_ver_t* sub;	/* Array of subprojects */
	struct proj_bom_ver_t* boms;	/* BOMs for project with specific version */
};

/* Create struct from parsed item in database, from part number */
struct part_t* get_part_from_pn( const char* pn );

/* Create part struct from parsed item in database, from internal part number */
struct part_t* get_part_from_ipn( unsigned int ipn );

/* Create bom struct from parsed item in database, from internal part number */
struct bom_t* get_bom_from_ipn( unsigned int ipn );

/* Create project struct from parsed item in database, from internal part number */
struct proj_t* get_proj_from_ipn( unsigned int ipn );

/* Free the part structure */
void free_part_t( struct part_t* part );

/* Free the bom structure */
void free_bom_t( struct bom_t* bom );

/* Free the project structure */
void free_proj_t( struct proj_t* prj );

/* Write part to database */
int redis_write_part( struct part_t* part );

/* Copy part structure to new structure */
struct part_t* copy_part_t( struct part_t* src );

/* Copy bom structure to new structure */
struct bom_t* copy_bom_t( struct bom_t* src );

/* Write bom to database */
int redis_write_bom( struct bom_t* bom );

/* Write project to database */
int redis_write_proj( struct proj_t* prj );

/* Read database information */
int redis_read_dbinfo( struct dbinfo_t* db );

/* Write database information */
int redis_write_dbinfo( struct dbinfo_t* db );

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
