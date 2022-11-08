#include <stdlib.h>
#include <string.h>
#include <part_funct.h>
#include <proj_funct.h>

/* Get number of all items in project BOM and subprojects */
unsigned int get_num_all_uniq_proj_items( struct proj_t * p ){
	unsigned int nitems = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
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
		/* Check if parts are unique */

		/* Recursively go through subprojects */
		int sub_nitems = get_num_all_uniq_proj_items( p->sub[i].prj ); 
		nitems += sub_nitems;
	}
	y_log_message( Y_LOG_LEVEL_DEBUG, "Total unique items in project: %u", nitems );
	return nitems;
}

/* Retrieve total number of single part type used */
unsigned int get_num_all_proj_type_items( struct proj_t * p, char * ptype ){
	unsigned int nitems = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			/* Check if type matches */
			if( !strcmp( ptype, p->boms[i].bom->line[j].type ) ){
				/* Add number used to count */
				nitems += p->boms[i].bom->line[j].q;
			}
		}
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

/* Retrieve total number of parts used */
unsigned int get_num_all_proj_items( struct proj_t * p ){
	unsigned int nitems = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			/* Add number used to count */
			nitems += p->boms[i].bom->line[j].q;
		}
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

/* Return the first part that is the limiting factor for production */
struct part_t ** get_proj_prod_lim_factor( struct proj_t* p ){
	unsigned int tmp_total_part = 0;	
	unsigned int uparts = 0;
	unsigned int pidx = 0; /* Index for return array */
	struct part_t** parts = NULL;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return NULL;
	}

	uparts = get_num_all_uniq_proj_items( p );
	/* Size array to be as large as the number of total parts */
	parts = calloc( uparts, sizeof( struct part_t *));

	if( NULL == parts ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; could not allocate memory for part return array", __func__ );
		return NULL;
	}


	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			/* Determine if total quantity for part is enough */
			tmp_total_part = get_part_total_inventory( p->boms[i].bom->parts[j] );
			if( p->boms[i].bom->line[j].q > tmp_total_part  ){
				/* Not enough parts, increment index of bad parts and add to
				 * return array  */
				parts[pidx] = p->boms[i].bom->parts[j];
				pidx++;

			}
		}
	}

	/* Next loop through all other subprojects in project */
	for( unsigned int i = 0; i < p->nsub; i++ ){
		if( NULL == p->sub || NULL == p->sub[i].prj ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return NULL;		
		}
		/* Recursively go through subprojects */
		struct part_t** tmp = get_proj_prod_lim_factor( p->sub[i].prj );
		/* Append array with array from sub */
		pidx += ( (sizeof( tmp )) / (sizeof( struct part_t *)) );
		if( pidx > (sizeof( parts )/sizeof( struct part_t * )) ){
			y_log_message( Y_LOG_LEVEL_ERROR, "%s: Part index(%d) is larger than array size ", __func__, pidx );
			return parts;
		}
		y_log_message( Y_LOG_LEVEL_DEBUG, "Subproject size: %d", ( (sizeof( tmp )) / (sizeof( struct part_t *)) ));
		y_log_message( Y_LOG_LEVEL_DEBUG, "New index: %d", pidx );
		for( unsigned int i = 0; i < ( (sizeof( tmp )) / (sizeof( struct part_t *)) ); i++ ){
			parts[pidx] = tmp[i];	
		}
		/* Free memory from recursed call */
		free( tmp );
	}
	
	/* Reallocate to trim allocated size */
	parts = realloc( parts, sizeof(struct part_t *) * (pidx) );

	return parts;
}

/* Determine number of units that can be manufacturered with current inventory */
unsigned int get_proj_prod_nunit( struct proj_t* p ){
	unsigned int n = 0;

}

/* Get optimal project cost with number of units passed */
double get_optimal_project_cost( struct proj_t * p, unsigned int units ){
	double cost = 0.0;

	/* scaled quantity for number of units */
	unsigned int scaled_q = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			scaled_q = p->boms[i].bom->line[j].q * units;
			/* Fix the quantity for optimal amount */
			scaled_q = get_optimal_part_amount( p->boms[i].bom->parts[j], scaled_q );
			y_log_message( Y_LOG_LEVEL_DEBUG, "Optimal Scaled q: %u", scaled_q );
			cost += get_optimal_part_cost( p->boms[i].bom->parts[j], scaled_q );
		}
	}

	/* Next loop through all other subprojects in project */
	for( unsigned int i = 0; i < p->nsub; i++ ){
		if( NULL == p->sub || NULL == p->sub[i].prj ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		/* Recursively go through subprojects */
		cost += get_optimal_project_cost( p->sub[i].prj, units );
	}
	return cost;
}

/* Get exact project cost with number of units passed */
double get_exact_project_cost( struct proj_t * p, unsigned int units ){
	double cost = 0.0;

	/* scaled quantity for number of units */
	unsigned int scaled_q = 0;

	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			scaled_q = p->boms[i].bom->line[j].q * units;
			y_log_message( Y_LOG_LEVEL_DEBUG, "Exact scaled q: %u", scaled_q );
			cost += get_exact_part_cost( p->boms[i].bom->parts[j], scaled_q );
		}
	}

	/* Next loop through all other subprojects in project */
	for( unsigned int i = 0; i < p->nsub; i++ ){
		if( NULL == p->sub || NULL == p->sub[i].prj ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		/* Recursively go through subprojects */
		cost += get_exact_project_cost( p->sub[i].prj, units );
	}
	return cost;
}

/* Retrieve total number of supplied part status */
unsigned int get_num_proj_partstatus( struct proj_t * p, enum part_status_t status ){
	unsigned int nitems = 0;


	/* Check if project is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* First get items from all boms in topmost level */
	for( unsigned int i = 0; i < p->nboms; i++ ){
		if( NULL == p->boms || NULL == p->boms[i].bom ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		
		/* Loop through the BOM for each part */
		for( unsigned int j = 0; j < p->boms[i].bom->nitems; j++ ){
			/* Check if status matches */
			if( p->boms[i].bom->parts[j]->status == status ){
				nitems++;
			}
		}
	}

	/* Next loop through all other subprojects in project */
	for( unsigned int i = 0; i < p->nsub; i++ ){
		if( NULL == p->sub || NULL == p->sub[i].prj ){
			y_log_message( Y_LOG_LEVEL_ERROR, "Could not access project bom for project %d. NULL pointer found", p->ipn );
			return 0;		
		}
		/* Recursively go through subprojects */
		nitems += get_num_proj_partstatus( p->sub[i].prj, status );
	}

	return nitems;
}
