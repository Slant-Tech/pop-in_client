#ifndef PART_FUNCT_H
#define PART_FUNCT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <db_handle.h>
#include <yder.h>

/* Determine total inventory amount for part */
unsigned int get_part_total_inventory( struct part_t * p );

/* Determine optimal order amount for cost reduction. Pass the amount to check
 * against (q) */
unsigned int get_optimal_part_amount( struct part_t * p, unsigned int q );

/* Determine optimal order cost for cost reduction. Pass the amount to check
 * against (q) */
double get_optimal_part_cost( struct part_t * p, unsigned int q );

/* Determine exact order cost for comparing cost reduction. Pass the amount to check
 * against (q) */
double get_exact_part_cost( struct part_t * p, unsigned int q );

#ifdef __cplusplus
}
#endif

#endif /* PART_FUNCT_H */
