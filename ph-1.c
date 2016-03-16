//
// Pre-history 1: allocate 1 or 3 word cells, make integers, symbols, pairs, functions. make nil, invoke halt
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

struct _cell;
typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union { struct { cell * car, * cdr; }; };
};

void * arena;
cell * next, * environment;

static void grant( )
{
	arena = mmap(
		0,
		4096 * 1,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);

	next = (cell *) arena;
	printf( "arena: %p, next: %p\n", arena, next );
}

static void halt( int n )
{
	printf( "halting…\n" );
	exit( n );
}

static cell * allocate( int words )
{
	cell * this = next;
	next = next + (8 * words);
	printf( "cell: %p, next: %p\n", this, next );
	return this;
}

static cell * make_integer( unsigned char n )
{
	cell * i = allocate( 1 );
	i->header = (n << 8) + 0x01;
	printf( "n: 0x%02x, %016lx\n", (unsigned int) (i->header >> 8), (unsigned long) i->header );
	return i;
}

static cell * make_symbol( unsigned char c )
{
	cell * i = allocate( 1 );
	i->header = (c << 8) + 0x02;
	printf( "c: '%c', %016lx\n", (unsigned char) (i->header >> 8), (unsigned long) i->header );
	return i;
}

static cell * make_pair( cell * j, cell * k )
{
	cell * i = allocate( 3 );
	i->header = 0x03;
	i->car = (cell *) j;
	i->cdr = (cell *) j;
	printf( "%016lx (%016lx . %016lx)\n", (unsigned long) i->header, (unsigned long) i->car, (unsigned long) i->cdr );
	return i;
}

static cell * make_function( void * f )
{
	cell * i = allocate( 1 );
	i->header = ((size_t) f << 8) + 0x04;
	printf( "f: %p\n", (void *) i->header );
	return i;
}
static void initialize( )
{
	printf( "sizeof( long ): %d\n", (int) sizeof( unsigned long ) );
	grant( );
	make_pair( (cell *) arena, (cell *) arena );
	make_symbol( 'a' );

	typedef cell * (* make_integer_ptr) (unsigned char);
	make_integer_ptr f = make_integer;
	printf( "f: %p\n", (void *) (size_t) *f );
	cell * i = f( 21 );

	cell * g = make_function( &make_integer );
	printf( "g: %p\n", (void *) g->header );

	make_integer_ptr h = (make_integer_ptr) (((size_t) g->header) >> 8);
	printf( "h: %p\n", (void *) (size_t) *h );
	cell * j = f( 22 );
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
