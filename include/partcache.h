#ifndef PARTCACHE_H
#define PARTCACHE_H

#include <mutex>
#include <vector>
#include <string>
#include <yder.h>
#include <db_handle.h>


class Partcache {

	private:
		/* The actual cache */
		std::vector<struct part_t*> cache;

		/* Cache mutex */
		std::mutex cmtx;

		/* Cleaning mutex */
		std::mutex clean_mtx;

		/* Selected project; Used for UI. Better to keep it here, as it becomes
		 * thread safe then */
		struct part_t* selected;	
		
		/* Internal functions; not thread safe */
		int _write( struct part_t * p, unsigned int index );
		struct part_t* _read( unsigned int index );
		int _clean( void );	
		int _insert( struct part_t * p, unsigned int index );
		int _insert_ipn( unsigned int ipn, unsigned int index );
		int _append( struct part_t * p );
		int _append_ipn( unsigned int ipn );
		int _remove( unsigned int index );
		void _DisplayNode( struct part_t* node );

	public:
		std::string type;

		Partcache( unsigned int size, std::string init_type );
		~Partcache();
		unsigned int items(void);
		int update( struct dbinfo_t** info );
		int write( struct part_t * p, unsigned int index );
		struct part_t* read( unsigned int index );
		int insert( struct part_t * p, unsigned int index );
		int insert_ipn( unsigned int ipn, unsigned int index );
		int append( struct part_t * p );
		int append_ipn( unsigned int ipn );
		int remove( unsigned int index );

		/* Relating to selected project */
		int select( unsigned int index );
		int select_ptr( struct part_t * p );
		struct part_t* get_selected( void );
		void display_parts( bool* clicked );

		bool getmutex( bool blocking );
		void releasemutex( void );

};

#endif
