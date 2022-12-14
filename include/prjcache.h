#ifndef PRJCACHE_H
#define PRJCACHE_H

#include <mutex>
#include <vector>
#include <yder.h>
#include <db_handle.h>


class Prjcache {

	private:
		/* The actual cache */
		std::vector<struct proj_t*> cache;

		/* Cache mutex */
		std::recursive_mutex cmtx;

		/* Cleaning mutex */
		std::mutex clean_mtx;

		/* Selected project; Used for UI. Better to keep it here, as it becomes
		 * thread safe then */
		struct proj_t* selected;	
		
		/* Internal functions; not thread safe */
		int _write( struct proj_t * p, unsigned int index );
		struct proj_t* _read( unsigned int index );
		int _clean( void );	
		int _insert( struct proj_t * p, unsigned int index );
		int _insert_ipn( unsigned int ipn, unsigned int index );
		int _append( struct proj_t * p );
		int _append_ipn( unsigned int ipn );
		int _remove( unsigned int index );
		void _DisplayNode( struct proj_t* node );

	public:

		Prjcache( unsigned int size );
		~Prjcache();
		unsigned int items(void);
		int update( struct dbinfo_t** info );
		int write( struct proj_t * p, unsigned int index );
		struct proj_t* read( unsigned int index );
		int insert( struct proj_t * p, unsigned int index );
		int insert_ipn( unsigned int ipn, unsigned int index );
		int append( struct proj_t * p );
		int append_ipn( unsigned int ipn );
		int remove( unsigned int index );

		/* Relating to selected project */
		int select( unsigned int index );
		int select_ptr( struct proj_t * p );
		struct proj_t* get_selected( void );
		void display_projects( bool all_prj );

		/* Acess cache mutex */
		bool getmutex( bool blocking );
		void releasemutex( void );

};

#endif
