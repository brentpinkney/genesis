//
// Pre-history 0: allocate an arena, halt
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE     4096

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stderr,  __VA_ARGS__ )

typedef void cell;

static cell * halt( cell * n )
{
	dprintf( "halting…\n" );
	exit( 1 );
	return n;
}

static cell * sire( cell * ignore )
{
	unsigned long size = PAGE_SIZE * 1;
	void * arena = mmap(
			0,
			size,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_ANONYMOUS | MAP_PRIVATE,
			0, 0 );
	dprintf( "size: %ld, arena: %p, last: %p\n", size, arena, arena + size - 1 );
	return arena;
}

int main( )
{
	halt( sire( 0 ) );
	return 0;
}
