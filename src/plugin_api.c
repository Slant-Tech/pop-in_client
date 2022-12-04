#include <plugin_api.h>
#include <yder.h>
#include <stdio.h>
#include <errno.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <libgen.h>
#define PY_SSIZE_T_CLEAN
#include <Python.h>

/* Default plugin path */
#ifdef _WIN32
const wchar_t* default_plugin_path = L"C:\\PopIn\\plugins";
const wchar_t* python_syspath = L"C:\\Program Files\\Python 3.10";
//const wchar_t* user_syspath = L"~/.local/lib/python3.10";
//const wchar_t* python_syspath_sitepack = L"/usr/lib/python3.10/site-packages";
//const wchar_t* python_syspath_dynload = L"/usr/lib/python3.10/lib-dynload";
#else

#if __APPLE__
const wchar_t* default_plugin_path = L"/usr/local/share/popin/plugins/";
const wchar_t* user_syspath = L"/usr/local/Cellar/python@3.10/3.10.6_2/Frameworks/Python.framework/Versions/3.10/lib/python3.10";
const wchar_t* python_syspath = L"/usr/local/Cellar/python@3.10/3.10.6_2/Frameworks/Python.framework/Versions/3.10/lib/python310.zip"; 
const wchar_t* python_syspath_dynload = L"/usr/local/Cellar/python@3.10/3.10.6_2/Frameworks/Python.framework/Versions/3.10/lib/python3.10/lib-dynload"; 
const wchar_t* python_syspath_sitepack = L"/usr/local/lib/python3.10/site-packages";
const wchar_t* python_localpath_sitepack = L"/usr/local/lib/python3.10/site-packages";
const wchar_t* python_localpath_dynload = L"/usr/local/lib/python3.10/lib-dynload";
#else

const wchar_t* default_plugin_path = L"/usr/share/popin/plugins/";
const wchar_t* user_syspath = L"~/.local/lib/python3.10";
const wchar_t* python_syspath = L"/usr/lib/python3.10";
const wchar_t* python_syspath_sitepack = L"/usr/lib/python3.10/site-packages";
const wchar_t* python_syspath_dynload = L"/usr/lib/python3.10/lib-dynload";
const wchar_t* python_localpath_sitepack = L"/usr/local/lib/python3.10/site-packages";
const wchar_t* python_localpath_dynload = L"/usr/local/lib/python3.10/lib-dynload";
#endif

#endif

static void append_python_path(  char* path ){
	/* Convert path to wide char string */
//	wchar_t wpath[1024] = {0}; /* This may get ugly... */
//	swprintf(wpath, 1024, L"%hs", path);
//	config->module_search_paths_set = 1;
//	Py_SetPythonHome(wpath);

	PyObject* syspath = PySys_GetObject("path");
	Py_ssize_t n = PyList_Size( syspath );

	/* Append the path */
//	PyWideStringList_Append( syspath, wpath );
	PyObject* append = PyUnicode_FromString( path );
	if( PyList_Append( syspath, append ) ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not append python path: %s", path );
	}
	PySys_SetObject(L"path", syspath );

	/* Cleanup */
//	Py_XDECREF(syspath);
	Py_XDECREF( append );
}

static void setup_python_path( PyConfig* config ){
//	PyWideStringList_Append( &(config->module_search_paths), default_plugin_path );
//	PyWideStringList_Append( &(config->module_search_paths), python_syspath );
	
	
#ifndef _WIN32
//	PyWideStringList_Append( &(config->module_search_paths), python_syspath_sitepack );
//	PyWideStringList_Append( &(config->module_search_paths), python_syspath_dynload );
//	PyWideStringList_Append( &(config->module_search_paths), default_plugin_path );
	PyWideStringList_Append( &(config->module_search_paths), user_syspath );
#endif
}

/* Create python interface */
static int open_python_api( char* path ){
#if 0
	PyStatus status;
	PyConfig config;
	PyConfig_InitPythonConfig( &config );
	config.isolated = 1;


	
	/* Start python interface */
	status = Py_InitializeFromConfig( &config );
	if( PyStatus_Exception( status ) ){
		PyConfig_Clear( &config );
		if( PyStatus_IsExit(status) ){
			return status.exitcode;
		}
		Py_ExitStatusException(status);
	}
#endif
	Py_Initialize();
	/* Set the paths for the plugins to work properly */
//	setup_python_path( &config );
#if 0
	PyObject* syspath = PySys_GetObject("path");
	Py_ssize_t n = PyList_Size( syspath );
	printf("%zu items\n", n);
	printf("Path before: \n");
	PyObject* p, *tmp;
	for( Py_ssize_t i = 0; i < n; i++ ){
		tmp = PyList_GetItem(syspath, i);
		p = PyUnicode_AsEncodedString( tmp, "utf-8", "strict");
		printf("%s\n", PyBytes_AsString(p) );
	}
#endif


	/* Append path */
	append_python_path( path );
#if 0
	syspath = PySys_GetObject("path");
	n = PyList_Size( syspath );
	printf("%zu items\n", n);
	printf("Path after: \n");
	for( Py_ssize_t i = 0; i < n; i++ ){
		tmp = PyList_GetItem(syspath, i);
		p = PyUnicode_AsEncodedString( tmp, "utf-8", "strict");
		printf("%s\n", PyBytes_AsString(p) );
	}
	
#endif

}

/* Close python interface */
static int close_python_api( void ){
	if( Py_FinalizeEx() < 0 ){
		return 120;
	}
}


/* Python function wrapper. Calls python function 'funct' from 'file', returns
 * retval. Return value is status of function call; if it succeeded or not.
 * Variable arguments should be PyObjects */
static int call_python_funct( PyObject** retval, char* file, char* funct, int nargs, ... ){
	va_list args;
	PyObject *pyfile, *pymodule, *pyfunct;
	PyObject *pyargs, *pyretval;

	if( NULL == file ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Plugin file is not valid");
		return -1;
	}
	if( NULL == funct ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Plugin function not provided" );
		return -1;
	}

	/* Get python file */
	pyfile = PyUnicode_DecodeFSDefault( file );
	if( NULL == pyfile ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not open plugin: %s", file );
		return -1;
	}

	/* Get module */
	pymodule = PyImport_Import( pyfile );
	Py_XDECREF( pyfile );

	/* Call the function if module is available */
	if( NULL != pymodule ){
		pyfunct = PyObject_GetAttrString( pymodule, funct );

		if( pyfunct && PyCallable_Check( pyfunct ) ){
			pyargs = PyTuple_New( nargs );
			va_start( args, nargs );
			for( int i = 0; i< nargs; i++ ){
				PyTuple_SetItem( pyargs, i, va_arg( args, PyObject* ) );	
			}
			va_end( args );

			/* Call the function, get the return value */
			*retval = PyObject_CallObject( pyfunct, pyargs );
			Py_XDECREF( pyargs );
			
			/* Check if return value is incorrect */
			if( NULL == *retval ) {
				/* Failed to return value */
				Py_XDECREF( pyfunct );
				Py_XDECREF( pymodule );
				PyErr_Print();
				y_log_message( Y_LOG_LEVEL_ERROR, "Plugin function %s in file %s failed to return a value", file, funct );
				return -1;
			}

		}
		else {
			/* Function not found */
			if( PyErr_Occurred() ){
				PyErr_Print();
			}
			y_log_message( Y_LOG_LEVEL_ERROR, "Plugin function %s was not found in %s", funct, file );
		}
		Py_XDECREF( pyfunct );
		Py_XDECREF( pymodule );
	}
	else {
		/* Issues with getting module */
		PyErr_Print();
		y_log_message( Y_LOG_LEVEL_ERROR, "Plugin file %s was not able to be loaded", file );
		return -1;
	}

	return 0;
}

char* get_string( void ){
	/* Use test file */
	const char* test_file = "api_test";
	const char* test_funct = "test_string";

	PyObject* func_return = NULL;
	char* retval = NULL;

	open_python_api("./plugins");

	call_python_funct( &func_return, test_file, test_funct, 2, PyUnicode_FromString("apple"), PyUnicode_FromString("taco") );
	
	if( NULL != func_return ){
		PyObject* string = PyUnicode_AsEncodedString( func_return, "utf-8", "strict");
		retval = PyBytes_AsString(string);
		Py_XDECREF( func_return );
		Py_XDECREF( string );
	}
	else {
		y_log_message( Y_LOG_LEVEL_ERROR, "Issue with return value for plugin" );
		retval = NULL;
	}

	close_python_api();

	return retval;
}

/* Testing digikey api to get price breaks for part */
void get_price_breaks( struct part_t * part ){
	/* Use test file */
	const char* test_file = "plugin_digikey";
	const char* test_funct = "get_price_breaks";

	PyObject* func_return = NULL;

	open_python_api("./plugins/mfg_digikey");

	call_python_funct( &func_return, test_file, test_funct, 1, PyUnicode_FromString("P5555-ND") );
//	call_python_funct( &func_return, test_file, test_funct, 1, PyUnicode_FromString( (*part)->dist[0].pn ) );
//	Py_IncRef( func_return );

	if( NULL != func_return && PyList_Check(func_return) ){
		/* Parse return object tuple */
		Py_ssize_t n = PyList_Size( func_return );
		printf("Size of list: %x\n", n);
		for( Py_ssize_t i = 0; i < n; i++ ){
			PyObject* b = PyList_GetItem(func_return, i);
			long b_amt = PyLong_AsLong( PyObject_GetAttrString( b,"break_quantity"));
			/* Issue reading data  */
			if( PyErr_Occurred() ){
				PyErr_Print();
			}
			double b_price = PyFloat_AsDouble( PyObject_GetAttrString( b,"unit_price" ) );
			/* Issue reading data  */
			if( PyErr_Occurred() ){
				PyErr_Print();
			}

			printf("[%x]\t%ld:\t$%f\n", i, b_amt, b_price );

			Py_XDECREF( b );
		}
		Py_XDECREF( func_return );
	}
	else {
		y_log_message( Y_LOG_LEVEL_ERROR, "Issue with return value for plugin" );
	}

	close_python_api();

}
