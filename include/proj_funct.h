#ifndef PROJ_FUNCT_H
#define PROJ_FUNCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <db_handle.h>
#include <yder.h>

/* Get number of unique items in project BOM and subprojects */
unsigned int get_num_all_uniq_proj_items( struct proj_t * p );

/* Retrieve total number of single part type used */
unsigned int get_num_all_proj_type_items( struct proj_t * p, char * ptype );

/* Get number of total parts used in project */
unsigned int get_num_all_proj_items( struct proj_t * p );

/* Return the first part that is the limiting factor for production */
struct part_t ** get_proj_prod_lim_factor( struct proj_t* p );

/* Determine number of units that can be manufacturered with current inventory */
unsigned int get_proj_prod_nunit( struct proj_t* p );

/* Get optimal project cost with number of units passed */
double get_optimal_project_cost( struct proj_t * p, unsigned int units );

/* Get exact project cost with number of units passed */
double get_exact_project_cost( struct proj_t * p, unsigned int units );

/* Retrieve total number of supplied part status */
unsigned int get_num_proj_partstatus( struct proj_t * p, enum part_status_t status );

#ifdef __cplusplus
}
#endif

#endif /* PROJ_FUNCT_H */
