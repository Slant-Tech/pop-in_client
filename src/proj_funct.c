#include <proj_funct.h>

/* Get number of all items in project BOM and subprojects */
unsigned int get_num_all_proj_items( struct proj_t * p ){
	unsigned int nitems = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not get number of items in project. NULL pointer passed" );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		nitems += p->boms[i].bom->nitems;
	}

	/* Next loop through all other subprojects in project */
	for( unsigned int i = 0; i < p->nsub; i++ ){
		if( NULL == p->sub || NULL == p->sub[i].prj ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		/* Recursively go through subprojects */
		nitems += get_num_all_proj_items( p->sub[i].prj );
	}

	return nitems;

}
