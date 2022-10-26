#ifndef PROJ_FUNCT_H
#define PROJ_FUNCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <db_handle.h>
#include <yder.h>

/* Get number of all items in project BOM and subprojects */
unsigned int get_num_all_proj_items( struct proj_t * p );

#ifdef __cplusplus
}
#endif

#endif /* PROJ_FUNCT_H */
