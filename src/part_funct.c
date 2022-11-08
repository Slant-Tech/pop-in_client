#include <part_funct.h>

/* Determine total inventory amount for part */
unsigned int get_part_total_inventory( struct part_t * p ){
	unsigned int ntotal = 0;	

	/* Check if part is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* Loop through inventory locations */
	for( unsigned int i = 0; i < p->inv_len; i++ ){
		ntotal += p->inv[i].q;
	}
	return ntotal;
}

/* Determine optimal order amount for cost reduction. Pass the amount to check
 * against (q) */
unsigned int get_optimal_part_amount( struct part_t * p, unsigned int q ){
	double price_tmp_exact = 0.0;
	double price_tmp_break = 0.0;

	/* Check if part is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* Check if number passed is equal to zero; can't do anything there */
	if( 0 == q ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Quantity given for checking optimal price was 0 for part: %s", p->mpn );
		return 0;
	}

	/* Check if price information exists for this part */
	if( !p->price_len ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Did not find any price information for part: %s", p->mpn );
		return 0;
	}

	/* Check if first price break is higher than passed amount */
	if( q <= p->price[0].quantity ){
		return q;
	}

	/* Check if last price break is lower than passed amount */
	if( q >= p->price[ p->price_len - 1 ].quantity ){
		return q;
	}

	/* Loop through the price breaks; skip first, as it was checked */
	for( unsigned int i = 1; i < p->price_len; i++ ){
		/* Price break is exactly the same as the quantity, just return that */
		if( q == p->price[i].quantity ){
			return q;
		} 
		/* Check if price break is higher than quantity, use that number to
		 * determine if cost is higher or lower than previous price break */
		else if( q < p->price[i].quantity ){
			/* Calculate cost */
			price_tmp_exact = p->price[i-1].price * q;
			price_tmp_break = p->price[i].price * p->price[i].quantity;

			y_log_message(Y_LOG_LEVEL_DEBUG, "Exact price: %lf | Price Break: %lf", price_tmp_exact, price_tmp_break);
			/* Check if exact amount or price break is cheaper */
			if( price_tmp_exact > price_tmp_break ){
				return p->price[i].quantity;
			}
			else{
				/* Price is the same, or more expensive */
				return q;
			}
		}
	}

	y_log_message( Y_LOG_LEVEL_ERROR, "End of %s should not have been able to be reached!!", __func__ );
	return 0;
}

/* Determine optimal order cost for cost reduction. Pass the amount to check
 * against (q) */
double get_optimal_part_cost( struct part_t * p, unsigned int q ){
	double retval = 0.0;
	double price_tmp_exact = 0.0;
	double price_tmp_break = 0.0;

	/* Check if part is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* Check if number passed is equal to zero; can't do anything there */
	if( 0 == q ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Quantity given for checking optimal price was 0 for part: %s", p->mpn );
		return 0;
	}

	/* Check if price information exists for this part */
	if( !p->price_len ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Did not find any price information for part: %s", p->mpn );
		return 0.0;
	}

	/* Check if first price break is higher than passed amount */
	if( q <= p->price[0].quantity ){
		y_log_message( Y_LOG_LEVEL_DEBUG, "Couldn't get better price for part %s, than lowest break", p->mpn );
		return (p->price[0].price * q);
	}

	/* Check if last price break is lower than passed amount */
	if( q >= p->price[ p->price_len - 1 ].quantity ){
		y_log_message( Y_LOG_LEVEL_DEBUG, "Couldn't get better price for part %s, than highest break", p->mpn );
		return (p->price[ p->price_len - 1 ].price * q);
	}

	/* Loop through the price breaks; skip first, as it was checked */
	for( unsigned int i = 1; i < p->price_len; i++ ){
		/* Price break is exactly the same as the quantity, just return that */
		if( q == p->price[i].quantity ){
			y_log_message( Y_LOG_LEVEL_DEBUG, "Price break is same as quantity");
			return (p->price[i].price * (double)q);
		} 
		/* Check if price break is higher than quantity, use that number to
		 * determine if cost is higher or lower than previous price break */
		else if( q < p->price[i].quantity ){
			/* Calculate cost */
			price_tmp_exact = p->price[i-1].price * (double)q;
			price_tmp_break = p->price[i].price * (double)p->price[i].quantity;

			/* Check if exact amount or price break is cheaper */
			if( price_tmp_exact > price_tmp_break ){
				y_log_message( Y_LOG_LEVEL_DEBUG, "Price break optimization was used" );
				retval = price_tmp_break;
			}
			else{
				/* Price is the same, or more expensive */
				y_log_message( Y_LOG_LEVEL_DEBUG, "Exact price was used" );
				retval = price_tmp_exact;
			}
		}
	}

//	y_log_message( Y_LOG_LEVEL_DEBUG, "Optimal part cost: %lf", retval );
	return retval;
}

/* Determine exact order cost for comparing cost reduction. Pass the amount to check
 * against (q) */
double get_exact_part_cost( struct part_t * p, unsigned int q ){
	double retval = 0.0;
	double price_tmp_exact = 0.0;
	double price_tmp_break = 0.0;

	/* Check if part is valid */
	if( NULL == p ){
		y_log_message( Y_LOG_LEVEL_ERROR, "In: %s; NULL pointer passed", __func__ );
		return 0;
	}

	/* Check if number passed is equal to zero; can't do anything there */
	if( 0 == q ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Quantity given for checking optimal price was 0 for part: %s", p->mpn );
		return 0;
	}

	/* Check if price information exists for this part */
	if( !p->price_len ){
		y_log_message( Y_LOG_LEVEL_WARNING, "Did not find any price information for part: %s", p->mpn );
		return 0.0;
	}

	/* Check if first price break is higher than passed amount */
	if( q <= p->price[0].quantity ){
		return (p->price[0].price * q);
	}

	/* Check if last price break is lower than passed amount */
	if( q >= p->price[ p->price_len - 1 ].quantity ){
		return (p->price[ p->price_len - 1 ].price * q);
	}

	/* Loop through the price breaks; skip first, as it was checked */
	for( unsigned int i = 1; i < p->price_len; i++ ){
		/* Price break is exactly the same as the quantity, just return that */
		if( q == p->price[i].quantity ){
			return (p->price[i].price * (double)q);
		} 
		/* Check if price break is higher than quantity, use that number to
		 * determine if cost is higher or lower than previous price break */
		else if( q < p->price[i].quantity ){
			/* Calculate cost */
			retval = price_tmp_exact = p->price[i-1].price * (double)q;
		}
	}

	return retval;
}
