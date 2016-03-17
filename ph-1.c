//
// Pre-history 1: allocate ( null | integer | symbol | function | pairs ),  is_null, is_pair, equals, invoke 
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE     4096
#define WORD_SIZE     8
#define TYPE_POINTER  0x00
#define TYPE_NULL     0x01
#define TYPE_INTEGER  0x02
#define TYPE_SYMBOL   0x03
#define TYPE_FUNCTION 0x04
#define TYPE_PAIR     0x05

int verbose = 1;
#define dprintf( ... ) if( verbose ) printf( __VA_ARGS__ )

struct _cell;
typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union { struct { cell * car, * cdr; }; };
};

void * arena;
cell * next, * null, * environment;

static void grant( )
{
	arena = mmap(
		0,
		PAGE_SIZE * 1,
		PROT_READ | PROT_WRITE | PROT_EXEC,
		MAP_ANONYMOUS | MAP_PRIVATE, 0, 0 );

	next = (cell *) arena;
	dprintf( "arena: %p, next: %p\n", arena, next );
}

static void halt( int n )
{
	exit( n );
}

static cell * allocate( int words )
{
	cell * this = next;
	next = ( ( void * ) next ) + ( WORD_SIZE * words );
	dprintf( "word: %d, cell: %p, next: %p\n", words, this, next );
	return this;
}

static cell * is_null( cell * i )
{
	return i == null ? environment : null;
}

static cell * is_pair( cell * i )
{
	return ( i->header & 0x07 ) == TYPE_PAIR ? i : null;
}

static cell * equals( cell * i, cell * j )
{
	if( ( is_pair( i ) != null ) && ( is_pair( j ) != null ) )
	{
		return ( i == j ) ? i : null;
	}
	else
	{
		return ( i->header == j->header ) ? i : null;
	}
}

static cell * car( cell * i )
{
	return i->car;
}

static cell * cdr( cell * i )
{
	return i->cdr;
}

static cell * set_car( cell * i, cell * j )
{
	i->car = j;
	return j;
}

static cell * set_cdr( cell * i, cell * j )
{
	i->cdr = j;
	return j;
}

static cell * nil( )
{
	cell * i = allocate( 1 );
	i->header =  TYPE_NULL;
	dprintf( "null: %016lx\n", i->header );
	return i;
}

static cell * symbol( unsigned char c )
{
	cell * i = allocate( 1 );
	i->header = ( c << 8 ) + TYPE_SYMBOL;
	dprintf( "c: '%c', %016lx\n", (unsigned char) ( i->header >> 8 ), i->header );
	return i;
}

static cell * integer( unsigned char n )
{
	cell * i = allocate( 1 );
	i->header = ( n << 8 ) + TYPE_INTEGER;
	dprintf( "n: 0x%02lx, %016lx\n", i->header >> 8, i->header );
	return i;
}

static cell * function( void * f )
{
	cell * i = allocate( 1 );
	i->header = ( (size_t) f << 8 ) + TYPE_FUNCTION;
	dprintf( "f: %016lx\n", i->header );
	return i;
}

static cell * cons( cell * j, cell * k )
{
	cell * i = allocate( 3 );
	i->header = TYPE_PAIR;
	set_car( i, j );
	set_cdr( i, k );
	dprintf( "%016lx (%016lx . %016lx)\n", i->header, (unsigned long) car( i ), (unsigned long) cdr( i ) );
	return i;
}

static cell * read( )
{
	halt( 1 );
	return 0;
}

static cell * eval( cell * exp, cell * env )
{
	return NULL;
}

static cell * print( cell * exp )
{
	return NULL;
}

static cell * interpret( )
{
	while( 1 ) print( eval( read( ), environment ) );
	return NULL;
}

static void initialize( )
{
	grant( );

	null = nil( );
	cell * sym  = symbol( 'a' );
	environment = cons( sym, null );

	printf( "arena       : %p\n", (void *) arena );
	printf( "null        : %p\n", (void *) null );
	printf( "environment : %p\n", (void *) environment );

	cell * in = is_null( null );
	printf( "is_null(null)        : %p\n", (void *) in );
	cell * nn = is_null( environment );
	printf( "is_null(environment) : %p\n", (void *) nn );

	cell * np = is_pair( null );
	printf( "is_pair(null)        : %p\n", (void *) np );
	cell * ip = is_pair( environment );
	printf( "is_pair(environment) : %p\n", (void *) ip );

	cell * eq = equals( integer( 5 ), integer( 5 ) );
	printf( "equals(5, 5) : %p\n", (void *) eq );
	cell * ne = equals( integer( 5 ), integer( 4 ) );
	printf( "equals(5, 4) : %p\n", (void *) ne );
	cell * pe = equals( environment, environment );
	printf( "equals(e, e)      : %p\n", (void *) pe );
	cell * pn = equals( environment, cdr( environment ) );
	printf( "equals(e, cdr(e)) : %p\n", (void *) pn );

	cell * f = function( integer );

	typedef cell * (* fn_integer) (unsigned char);
	fn_integer g = (fn_integer) (((size_t) f->header) >> 8);
	dprintf( "g: %p\n", (void *) (size_t) *g );
	g( 21 );
}

int main( )
{
	initialize( );
	interpret( );
	return 0;
}
