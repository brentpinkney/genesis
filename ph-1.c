//
// Pre-history 1: define cell, allocate ( null | tuple | symbol | integer ) and related functions
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE		4096
#define NUM_PAGES		1024
#define WORD_SIZE		8
#define CELL_NULL		0x01
#define CELL_TUPLE		0x02
#define CELL_SYMBOL		0x03
#define CELL_INTEGER		0x04
#define MASK_TYPE		0x0f	// 0000 1111
#define MASK_INTEGER_HI		0xf00000
#define MASK_INTEGER_LO		0x0f0000

#define is_true  != null
#define is_false == null

typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union
	{
		struct { void * zero, * arena, * extent, * next; };
		struct { cell * car, * cdr; };
	};
};

// procedures…
static void quit( unsigned long n ) { exit( n ); }

static unsigned long cell_type( cell * c )     { return c->header & MASK_TYPE; }
static unsigned long symbol_value( cell * s )  { return ( s->header >> 16 ) & 0xff; }
static unsigned long integer_value( cell * s ) { return ( s->header >> 16 ) & 0xff; }

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
	null->next   = arena + ( 5 * WORD_SIZE );
	null->zero   = (void *) CELL_INTEGER;		// a cell that is not null

	printf( "null: %016lx, zero: %016lx, arena: %016lx, extent: %016lx, next: %016lx\n",
		null->header, (unsigned long) null->zero, (unsigned long) null->arena, (unsigned long) null->extent, (unsigned long) null->next );

	return null;
}

static cell * symbol( cell * null, unsigned long c )
{
	cell * i = allocate( null, 1 );
	i->header = ( c << 16 ) + CELL_SYMBOL;
	printf( "c: '%c', %016lx\n", (unsigned char) ( i->header >> 8 ), i->header );
	return i;
}

static cell * integer( cell * null, unsigned long n )
{
	cell * i = allocate( null, 1 );
	i->header = ( n << 16 ) + CELL_INTEGER;
	printf( "n: 0x%02lx, %016lx\n", i->header >> 8, i->header );
	return i;
}

// functions…
static cell * car( cell * null, cell * c ) { return c->car; }

static cell * cdr( cell * null, cell * c ) { return c->cdr; }

static cell * is_null( cell * null, cell * c )  { return ( cell_type( c ) == CELL_NULL )  ? (cell*) &null->zero : null; }

static cell * is_tuple( cell * null, cell * c ) { return ( cell_type( c ) == CELL_TUPLE ) ? (cell*) &null->zero : null; }

static cell * is_symbol( cell * null, cell * c )  { return ( cell_type( c ) == CELL_SYMBOL ) ? (cell*) &null->zero : null; }

static cell * is_integer( cell * null, cell * c ) { return ( cell_type( c ) == CELL_INTEGER ) ? (cell*) &null->zero : null; }

static cell * cons( cell * null, cell * a, cell * b )
{
	cell * t = allocate( null, 3 );
	t->header = CELL_TUPLE;
	t->car = a;
	t->cdr = b;
	printf( "%016lx (%016lx . %016lx)\n", t->header, (unsigned long) car( null, t ), (unsigned long) cdr( null, t ) );
	return t;
}

static cell * equals( cell * null, cell * a, cell * b )
{
	return ( ( is_tuple( null, a ) is_true ) || ( is_tuple( null, b ) is_true ) )
		? ( a == b ) ? (cell *) &null->zero : null
		: ( a->header == b->header ) ? (cell *) &null->zero : null;
}

static cell * assq( cell * null, cell * key, cell * alist )
{
	return ( alist == null )
		? null
		: ( equals( null, key, alist->car->car ) is_true )
			? alist->car
			: assq( null, key, alist->cdr );
}

int main( )
{
	cell * null = heap( NUM_PAGES );
	cell * env  = null;

	// tests…
	printf( "null        : %p\n", (void *) null );
	printf( "null->zero  : %p\n", (void *) &null->zero );
	printf( "environment : %p\n", (void *) env  );

	cell * tuple = cons( null, integer( null, 5 ), symbol( null, 'd' ) );

	cell * in = is_null( null, null );
	printf( "is_null(null)   : %p\n", (void *) in );
	cell * nn = is_null( null, tuple );
	printf( "is_null(tuple)  : %p\n", (void *) nn );
	cell * it = is_tuple( null, tuple );
	printf( "is_tuple(tuple) : %p\n", (void *) it );
	cell * nt = is_tuple( null, null );
	printf( "is_tuple(null)  : %p\n", (void *) nt );

	printf( "is_integers(5)  : %p\n", (void *) is_integer( null, car( null, tuple ) ) );
	printf( "is_symbol(d)    : %p\n", (void *) is_symbol( null, cdr( null, tuple ) ) );

	printf( "integer_value(5)  : 0x%02lx\n", integer_value( car( null, tuple ) ) );
	printf( "symbol_value('d') : 0x%02lx\n", symbol_value( cdr( null, tuple ) ) );

	cell * eq = equals( null, integer( null, 5 ), integer( null, 5 ) );
	printf( "equals(5, 5) : %p\n", (void *) eq );
	cell * ne = equals( null, integer( null, 5 ), integer( null, 4 ) );
	printf( "equals(5, 4) : %p\n", (void *) ne );
	cell * te = equals( null, tuple, tuple );
	printf( "equals(tuple, tuple) : %p\n", (void *) te );
	cell * tn = equals( null, env, tuple );
	printf( "equals(e, tuple)     : %p\n", (void *) tn );

	env = cons( null, cons( null, symbol( null, 'a' ), integer( null, 0x0a ) ), env );
	env = cons( null, cons( null, symbol( null, 'b' ), integer( null, 0x0b ) ), env );
	env = cons( null, cons( null, symbol( null, 'c' ), integer( null, 0x0c ) ), env );
	printf( "environment          : %p\n", env );
	cell * fs = assq( null, symbol( null, 'b' ), env );
	printf( "assq(b, env), found  : %016lx\n", fs->header );
	cell * nf = assq( null, symbol( null, 'd' ), env );
	printf( "assq(d, env), absent : %016lx\n", nf->header );

	return 0;
}
