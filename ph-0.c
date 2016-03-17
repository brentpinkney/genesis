//
// Pre-history 0: allocate a arena, skeletons for the REPL, halt in read
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE     4096

int verbose = 1;
#define dprintf( ... ) if( verbose ) printf( __VA_ARGS__ )

void * arena, * environment;

static void grant( )
{
	arena = mmap(
		0,
		PAGE_SIZE * 1,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0 );
	dprintf( "arena: %p\n", arena );
}

static void halt( int n )
{
	exit( n );
}

static void * read( )
{
	halt( 1 );
	return 0;
}

static void * eval( void * exp, void * env )
{
	return NULL;
}

static void print( void * exp )
{
	return;
}

static void interpret( )
{
	while( 1 ) print( eval( read( ), environment ) );
	return;
}

static void initialize( )
{
	grant( );
}

int main( )
{
	initialize( );
	interpret( );
	return 0;
}
