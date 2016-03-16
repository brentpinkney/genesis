//
// Pre-history 0: allocate a arena, skeletons for the REPL, halt in read
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

void * arena, * environment;

static void grant( )
{
	arena = mmap(
		0,
		4096 * 1,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
	printf( "arena: %p\n", arena );
}

static void initialize( )
{
	grant( );
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

static void print( )
{
	return;
}

static void * eval( void * exp, void * env )
{
	return;
}

static void interpret( )
{
	while( 1 ) print( eval( read( ), environment ) );
	return;
}

int main( )
{
	initialize( );
	interpret( );
	return 0;
}
