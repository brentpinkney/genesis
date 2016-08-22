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

typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union
	{
		struct { void * arena, * extent, * next; };
	};
};

// procedures…
static void quit( unsigned long n ) { exit( n ); }

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
	if( null->next > null->extent ) quit( 2 );
	printf( "words: %ld, cell: %p, next: %p\n", words, this, null->next );
	return this;
}

static cell * heap( unsigned long pages )
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
	null->extent = arena + bytes;
	null->next   = arena + ( 4 * WORD_SIZE );

	printf( "null: %016lx, arena: %016lx, extent: %016lx, next: %016lx\n",
		null->header, (unsigned long) null->arena, (unsigned long) null->extent, (unsigned long) null->next );

	return null;
}

int main( )
{
	cell * null = heap( NUM_PAGES );
	cell * pair = allocate( null, 1 );
	return 0;
}
