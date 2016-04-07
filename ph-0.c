//
// Pre-history 0: allocate an arena, quit
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE		4096
#define NUM_PAGES		1024
#define WORD_SIZE		8
#define CELL_NULL		0x01


#define CELL_INTEGER		0x04

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c )     ( c->header & MASK_TYPE )

typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union
	{
		struct { cell * size; void * arena, * next; };
	};
};

// procedures…
static void quit( unsigned long n ) { exit( n ); }

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
	dprintf( "words: %ld, cell: %p, next: %p\n", words, this, null->next );
	return this;
}

static cell * sire( unsigned long pages )
{
	unsigned long bytes = PAGE_SIZE * pages;
	void * arena = mmap(
			0,
			bytes,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_ANONYMOUS | MAP_PRIVATE,
			0, 0 );
	if( arena == MAP_FAILED ) quit( 1 );

	cell * null  = arena;				// build null by hand
	null->header = CELL_NULL;
	null->arena  = arena;
	null->next   = arena + ( 4 * WORD_SIZE );

	cell * size  = allocate( null, 1 );		// make the size integer (useful as 'not null')
	size->header = ( bytes << 8 ) + CELL_INTEGER;
	null->size   = size;
	dprintf( "size: 0x%02lx, %016lx\n", size->header >> 8, size->header );

	dprintf( "null: %016lx, size: %016lx, arena: %016lx, next: %016lx\n",
		null->header, null->size->header, (unsigned long) null->arena, (unsigned long) null->next );

	return null;
}

int main( )
{
	sire( NUM_PAGES );
	quit( 0 );
	return 0;
}
