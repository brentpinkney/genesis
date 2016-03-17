//
// Pre-history 2: build environment, find any prmitive via environment, assq
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

int verbose = 0;
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

static cell * assq( cell * key, cell * alist )
{
	dprintf( "assq: key = %p, alist = %p\n", key, alist );
	if( is_null( alist ) != null )
	{
		dprintf( "assq: not found\n" );
		return null;
	}
	else
	{
		dprintf( "assq: caar(alist) = %p\n", car( car( alist ) ) );
		if( equals( key, car( car( alist ) ) ) != null )
		{
			return cdr( car( alist ) );
		}
		else
		{
			return assq( key, cdr( alist ) );
		}
	}
}

static cell * read( )
{
	halt( 1 );
	return 0;
}

static cell * eval( cell * exp, cell * env )
{
	return;
}

static cell * print( cell * exp )
{
	return;
}

static cell * interpret( )
{
	while( 1 ) print( eval( read( ), environment ) );
	return;
}

static void initialize( )
{
	grant( );

	null = nil( );

	environment = 
		cons( cons( integer( 0x13 ), function( initialize ) ),
		cons( cons( integer( 0x12 ), function( interpret ) ),
		cons( cons( integer( 0x11 ), function( print ) ),
		cons( cons( integer( 0x10 ), function( eval ) ),
		cons( cons( integer( 0x0f ), function( read ) ),
		cons( cons( integer( 0x0e ), function( assq ) ),
		cons( cons( integer( 0x0d ), function( cons ) ),
		cons( cons( integer( 0x0c ), function( function ) ),
		cons( cons( integer( 0x0b ), function( integer ) ),
		cons( cons( integer( 0x0a ), function( symbol ) ),
		cons( cons( integer( 0x09 ), function( set_cdr ) ),
		cons( cons( integer( 0x08 ), function( set_car ) ),
		cons( cons( integer( 0x07 ), function( cdr ) ),
		cons( cons( integer( 0x06 ), function( car ) ),
		cons( cons( integer( 0x05 ), function( equals ) ),
		cons( cons( integer( 0x04 ), function( is_pair ) ),
		cons( cons( integer( 0x03 ), function( is_null ) ),
		cons( cons( integer( 0x02 ), function( allocate ) ),
		cons( cons( integer( 0x01 ), function( halt ) ),
		cons( cons( integer( 0x00 ), function( grant ) ),
		null))))))))))))))))))));

	printf( "arena       : %p\n", (void *) arena );
	printf( "null        : %p\n", (void *) null );
	printf( "environment : %p\n", (void *) environment );

	// find 0x0e → assq( k, alist ), then find 0x0a → symbol( c )
	cell * p = car( cdr( cdr( cdr( cdr( cdr( environment ) ) ) ) ) );
	cell * k = car( p );
	cell * h = cdr( p );
	printf( "k: 0x%016lx, fn: 0x%016lx\n", (unsigned long) k->header, (unsigned long) h->header );

	typedef cell * (* fn_assq) ( cell * key, cell * alist );
	fn_assq a = (fn_assq) (((size_t) h->header) >> 8);
	printf( "a: %p\n", (void *) (size_t) *a );
	cell * f = a( integer( 0x0a ), environment );
	printf( "a( 0x0a, e0) : %p\n", (void *) f );
}

int main( )
{
	initialize( );
	interpret( );
	return 0;
}
