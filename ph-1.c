//
// Pre-history 1: define cell, allocate ( null | tuple | symbol | integer ) and related functions
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE		4096
#define WORD_SIZE		8
#define CELL_NULL		0x01
#define CELL_TUPLE		0x02
#define CELL_SYMBOL		0x03
#define CELL_INTEGER		0x04
#define MASK_TYPE		0x07
#define MASK_SYMBOL		0xff00
#define MASK_INTEGER_HI		0xf000
#define MASK_INTEGER_LO		0x0f00

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )
#define symbol_value( s )  ( ( s->header & MASK_SYMBOL )  >> 8 )
#define integer_value( s ) ( ( s->header & MASK_INTEGER ) >> 8 )
#define is_true  != null
#define is_false == null

typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union
	{
		struct { cell * size; void * arena, * next; };
		struct { cell * car, * cdr; };
	};
};

// functions…
static void halt( unsigned long n )
{
	dprintf( "halting…\n" );
	exit( n );
}

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
	dprintf( "words: %ld, cell: %p, next: %p\n", words, this, null->next );
	return this;
}

static cell * sire( )
{
	unsigned long bytes = PAGE_SIZE * 1;
	void * arena = mmap(
			0,
			bytes,
			PROT_READ | PROT_WRITE | PROT_EXEC,
			MAP_ANONYMOUS | MAP_PRIVATE,
			0, 0 );
	if( arena == MAP_FAILED )
	{
		halt( 1 );
	}

	// build null by hand…
	cell * null  = arena;
	null->header = CELL_NULL;
	null->arena  = arena;
	null->next   = arena + ( 4 * WORD_SIZE );

	// make the size integer by hand (useful as 'not null')…
	cell * size  = allocate( null, 1 );
	size->header = ( bytes << 8 ) + CELL_INTEGER;
	dprintf( "size: 0x%02lx, %016lx\n", size->header >> 8, size->header );

	null->size = size;

	dprintf( "null: %016lx, size: %016lx, arena: %016lx, next: %016lx\n",
		null->header, null->size->header, (unsigned long) null->arena, (unsigned long) null->next );

	return null;
}

static cell * symbol( cell * null, unsigned char c )
{
	cell * i = allocate( null, 1 );
	i->header = ( c << 8 ) + CELL_SYMBOL;
	dprintf( "c: '%c', %016lx\n", (unsigned char) ( i->header >> 8 ), i->header );
	return i;
}

static cell * integer( cell * null, unsigned char n )
{
	cell * i = allocate( null, 1 );
	i->header = ( n << 8 ) + CELL_INTEGER;
	dprintf( "n: 0x%02lx, %016lx\n", i->header >> 8, i->header );
	return i;
}

// procedures…
static cell * car( cell * null, cell * c ) { return c->car; }

static cell * cdr( cell * null, cell * c ) { return c->cdr; }

static cell * set_car( cell * null, cell * c, cell * x ) { c->car = x; return x; }

static cell * set_cdr( cell * null, cell * c, cell * x ) { c->cdr = x; return x; }

static cell * is_null( cell * null, cell * c )  { return ( cell_type( c ) == CELL_NULL )  ? null->size : null; }

static cell * is_tuple( cell * null, cell * c ) { return ( cell_type( c ) == CELL_TUPLE ) ? null->size : null; }

static cell * is_atom( cell * null, cell * c ) { return ( is_tuple( null, c ) is_true ) ? null : null->size; }

static cell * is_symbol( cell * null, cell * c )  { return ( cell_type( c ) == CELL_SYMBOL ) ? null->size : null; }

static cell * is_integer( cell * null, cell * c ) { return ( cell_type( c ) == CELL_INTEGER ) ? null->size : null; }

static cell * cons( cell * null, cell * a, cell * b )
{
	cell * t = allocate( null, 3 );
	t->header = CELL_TUPLE;
	t->car = a;
	t->cdr = b;
	dprintf( "%016lx (%016lx . %016lx)\n", t->header, (unsigned long) car( null, t ), (unsigned long) cdr( null, t ) );
	return t;
}

static cell * equals( cell * null, cell * a, cell * b )
{
	if( ( is_atom( null, a ) is_true ) && ( is_atom( null, b ) is_true ) )
	{
		return ( a->header == b->header ) ? null->size : null;
	}
	else
	{
		return ( a == b ) ? null->size : null;
	}
}

static cell * assq( cell * null, cell * key, cell * alist )
{
	if( is_null( null, alist ) is_true )
	{
		dprintf( "assq: not found\n" );
		return null;
	}
	else
	{
		dprintf( "assq: caar(alist) = %016lx\n", car( null, car( null, alist ) )->header );
		if( equals( null, key, car( null, car( null, alist ) ) ) is_true )
		{
			return car( null, alist );
		}
		else
		{
			return assq( null, key, cdr( null, alist ) );
		}
	}
}

int main( )
{
	cell * null = sire( );
	cell * env  = null;

	// tests…
	dprintf( "null        : %p\n", (void *) null );
	dprintf( "null->size  : %p\n", (void *) null->size );
	dprintf( "environment : %p\n", (void *) env  );

	cell * tuple = cons( null, symbol( null, 'd' ), integer( null, 4 ) );
	set_car( null, tuple, integer( null, 5 ) );
	set_cdr( null, tuple, symbol( null, 'd' ) );

	cell * in = is_null( null, null );
	dprintf( "is_null(null)   : %p\n", (void *) in );
	cell * nn = is_null( null, tuple );
	dprintf( "is_null(tuple)  : %p\n", (void *) nn );
	cell * it = is_tuple( null, tuple );
	dprintf( "is_tuple(tuple) : %p\n", (void *) it );
	cell * nt = is_tuple( null, null );
	dprintf( "is_tuple(null)  : %p\n", (void *) nt );

	dprintf( "is_integers(5)  : %p\n", (void *) is_integer( null, car( null, tuple ) ) );
	dprintf( "is_symbol(d)    : %p\n", (void *) is_symbol( null, cdr( null, tuple ) ) );

	cell * eq = equals( null, integer( null, 5 ), integer( null, 5 ) );
	dprintf( "equals(5, 5) : %p\n", (void *) eq );
	cell * ne = equals( null, integer( null, 5 ), integer( null, 4 ) );
	dprintf( "equals(5, 4) : %p\n", (void *) ne );
	cell * te = equals( null, tuple, tuple );
	dprintf( "equals(tuple, tuple) : %p\n", (void *) te );
	cell * tn = equals( null, env, tuple );
	dprintf( "equals(e, tuple)     : %p\n", (void *) tn );

	env = cons( null, cons( null, symbol( null, 'a' ), integer( null, 0x0a ) ), env );
	env = cons( null, cons( null, symbol( null, 'b' ), integer( null, 0x0b ) ), env );
	env = cons( null, cons( null, symbol( null, 'c' ), integer( null, 0x0c ) ), env );
	dprintf( "environment          : %p\n", env );
	cell * fs = assq( null, symbol( null, 'b' ), env );
	dprintf( "assq(b, env), found  : %016lx\n", fs->header );
	cell * nf = assq( null, symbol( null, 'd' ), env );
	dprintf( "assq(d, env), absent : %016lx\n", nf->header );

	return 0;
}

