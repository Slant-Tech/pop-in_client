#ifndef PLUGIN_API_H
#define PLUGIN_API_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>

char* get_string( void );
void get_price_breaks( void );

#ifdef __cplusplus
}
#endif


#endif
