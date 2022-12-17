/*
 * 	SPDX-FileCopyrightText: 2022 Slant Tech, Dylan Wadler <dwadler@slant.tech>
 *  
 * 	SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <db_handle.h>

char* get_string( void );
void get_price_breaks( struct part_t * part );

#ifdef __cplusplus
}
#endif


#endif
