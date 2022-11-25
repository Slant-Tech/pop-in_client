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
#else
const wchar_t* default_plugin_path = L"/usr/share/popin/plugins/";
const wchar_t* python_syspath = L"/usr/lib/python3.10";
#endif

static void append_python_path( PyConfig* config, char* path ){
	/* Convert path to wide char string */
	wchar_t wpath[1024] = {0}; /* This may get ugly... */
	swprintf(wpath, 1024, L"%hs", path);
	config->module_search_paths_set = 1;

	PyWideStringList_Append( &(config->module_search_paths), wpath );
}

static void setup_python_path( PyConfig* config ){
	PyWideStringList_Append( &(config->module_search_paths), python_syspath );
	PyWideStringList_Append( &(config->module_search_paths), default_plugin_path );
}

/* Python function wrapper. Calls python function 'funct' from 'file', returns
 * retval. Return value is status of function call; if it succeeded or not.
 * Variable arguments should be PyObjects */
static int call_python_funct( char** retval, char* path, char* file, char* funct, int nargs, ... ){
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
	
	PyStatus status;
	PyConfig config;
	PyConfig_InitPythonConfig( &config );
	config.isolated = 1;

	/* Set the paths for the plugins to work properly */
	setup_python_path( &config );
	append_python_path( &config, path );

	status = Py_InitializeFromConfig( &config );
	if( PyStatus_Exception( status ) ){
		PyConfig_Clear( &config );
		if( PyStatus_IsExit(status) ){
			return status.exitcode;
		}
		Py_ExitStatusException(status);
	}

	/* Start python interface */
//	Py_Initialize();


	/* Get python file */
	pyfile = PyUnicode_DecodeFSDefault( file );
	if( NULL == pyfile ){
		y_log_message( Y_LOG_LEVEL_ERROR, "Could not open plugin: %s", file );
		return -1;
	}

	/* Get module */
	pymodule = PyImport_Import( pyfile );
	Py_DECREF( pyfile );

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
			pyretval = PyObject_CallObject( pyfunct, pyargs );
			Py_DECREF( pyargs );
			
			/* Check return value */
			if( NULL != pyretval ){
				PyObject* string = PyUnicode_AsEncodedString( pyretval, "utf-8", "strict");
				y_log_message( Y_LOG_LEVEL_DEBUG, "Plugin function returned: %s", PyBytes_AsString(string) );
				*retval = PyBytes_AsString(string);
				Py_DECREF( pyretval );
				Py_DECREF( string );
			}
			else {
				/* Failed to return value */
				Py_DECREF( pyfunct );
				Py_DECREF( pymodule );
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
		Py_DECREF( pymodule );
	}
	else {
		/* Issues with getting module */
		PyErr_Print();
		y_log_message( Y_LOG_LEVEL_ERROR, "Plugin file %s was not able to be loaded", file );
		return -1;
	}
	if( Py_FinalizeEx() < 0 ){
		return 120;
	}
	return 0;
}

char* get_string( void ){
	/* Use test file */
	const char* test_file = "api_test";
	const char* test_funct = "test_string";

	char* retval = NULL;

	call_python_funct( &retval, "plugins", test_file, test_funct, 2, PyUnicode_FromString("apple"), PyUnicode_FromString("taco") );

	return retval;
}
