//
// Pre-history 1: define cell, allocate ( null | tuple | symbol | integer ) and related functions
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE     4096
#define WORD_SIZE     8
#define MASK_TYPE     0x07
#define TYPE_NULL     0x01
#define TYPE_TUPLE    0x02
#define TYPE_SYMBOL   0x03
#define TYPE_INTEGER  0x04

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stderr,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )

struct _cell;
typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union {
		struct { unsigned long size; void * arena, * next; };
		struct { cell * car, * cdr; }; };
};

void * arena;

static cell * car( cell * c, cell * env ) { return c->car; }

static cell * cdr( cell * c, cell * env ) { return c->cdr; }

static cell * set_car( cell * c, cell * x, cell * env ) { c->car = x; return x; }

static cell * set_cdr( cell * c, cell * x, cell * env ) { c->cdr = x; return x; }

static cell * is_null( cell * c, cell * env )  { return ( cell_type( c ) == TYPE_NULL ) ? env : env->car; }

static cell * is_tuple( cell * c, cell * env ) { return ( cell_type( c ) == TYPE_TUPLE ) ? env : env->car; }

static cell * is_symbol( cell * c, cell * env )  { return ( cell_type( c ) == TYPE_SYMBOL ) ? env : env->car; }

static cell * is_integer( cell * c, cell * env ) { return ( cell_type( c ) == TYPE_INTEGER ) ? env : env->car; }

static cell * allocate( unsigned long words, cell * env ) // anomaly - words is not cell *
{
	cell * this = env->car->next;
	env->car->next = ( (void *) env->car->next ) + ( words * WORD_SIZE );
	dprintf( "words: %ld, cell: %p, next: %p\n", words, this, env->car->next );
	return this;
}

static cell * cons( cell * a, cell * b, cell * env )
{
	cell * t = allocate( 3, env );
	t->header = TYPE_TUPLE;
	t->car = a;
	t->cdr = b;
	dprintf( "%016lx (%016lx . %016lx)\n", t->header, (unsigned long) car( t, env ), (unsigned long) cdr( t, env ) );
	return t;
}

static cell * symbol( unsigned char c, cell * env )
{
	cell * i = allocate( 1, env );
	i->header = ( c << 8 ) + TYPE_SYMBOL;
	dprintf( "c: '%c', %016lx\n", (unsigned char) ( i->header >> 8 ), i->header );
	return i;
}

static cell * integer( unsigned char n, cell * env )
{
	cell * i = allocate( 1, env );
	i->header = ( n << 8 ) + TYPE_INTEGER;
	dprintf( "n: 0x%02lx, %016lx\n", i->header >> 8, i->header );
	return i;
}

static cell * equals( cell * a, cell * b, cell * env )
{
	if( ( is_tuple( a, env ) != env->car ) && ( is_tuple( b, env ) != env->car ) )
	{
		return ( a == b ) ? env : env->car;
	}
	else
	{
		return ( a->header == b->header ) ? env : env->car;
	}
}

static cell * assq( cell * key, cell * alist, cell * env )
{
	if( is_null( alist, env ) != env->car )
	{
		dprintf( "assq: not found\n" );
		return env->car;
	}
	else
	{
		dprintf( "assq: caar(alist) = %016lx\n", car( car( alist, env ), env )->header );
		equals( key, car( car( alist, env ), env ), env );
		if( equals( key, car( car( alist, env ), env ), env ) != env->car )
		{
			return cdr( car( alist, env ), env );
		}
		else
		{
			return assq( key, cdr( alist, env ), env );
		}
	}
}

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

	// no environment yet, so build null by hand…
	cell * null  = arena;
	null->header = TYPE_NULL;
	null->size   = size;
	null->arena  = arena;
	null->next   = arena + ( 4 * WORD_SIZE );
	dprintf( "null: %016lx, size: 0x%lx, arena: %016lx, next: %016lx\n",
		null->header, null->size, (unsigned long) null->arena, (unsigned long) null->next );

	// now build the environment…
	cell * env  = null->next;
	env->header = TYPE_TUPLE;
	env->car    = null;
	env->cdr    = null;
	null->next  = null->next + ( 3 * WORD_SIZE );
	dprintf( "null: %016lx, size: 0x%lx, arena: %016lx, next: %016lx\n",
		null->header, null->size, (unsigned long) null->arena, (unsigned long) null->next );

	// tests…
	printf( "environment : %p\n", (void *) env );
	printf( "null        : %p\n", (void *) car( env, env ) );
	printf( "definitions : %p\n", (void *) cdr( env, env ) );

	cell * nn = is_null( env, env );
	printf( "is_null(env)  : %p\n", (void *) nn );
	cell * in = is_null( null, env );
	printf( "is_null(null) : %p\n", (void *) in );

	cell * ip = is_tuple( env, env );
	printf( "is_tuple(env)  : %p\n", (void *) ip );
	cell * np = is_tuple( null, env );
	printf( "is_tuple(null) : %p\n", (void *) np );

	cell * tuple = cons( symbol( 'd', env ), integer( 4, env ), env );
	set_car( tuple, integer( 5, env ), env );
	set_cdr( tuple, symbol( 'd', env ), env );
	printf( "is_integer() : %p\n", (void *) is_integer( car( tuple, env ), env ) );
	printf( "is_symbol()  : %p\n", (void *) is_symbol( cdr( tuple, env ), env ) );

	cell * eq = equals( integer( 5, env ), integer( 5, env ), env );
	printf( "equals(5, 5) : %p\n", (void *) eq );
	cell * ne = equals( integer( 5, env ), integer( 4, env ), env );
	printf( "equals(5, 4) : %p\n", (void *) ne );
	cell * pe = equals( env, env, env );
	printf( "equals(e, e)      : %p\n", (void *) pe );
	cell * pn = equals( env, cdr( env, env ), env );
	printf( "equals(e, cdr(e)) : %p\n", (void *) pn );

	env->cdr = cons( cons( symbol( 'a', env ), integer( 0x0a, env ), env ), env->cdr, env );
	env->cdr = cons( cons( symbol( 'b', env ), integer( 0x0a, env ), env ), env->cdr, env );
	env->cdr = cons( cons( symbol( 'c', env ), integer( 0x0a, env ), env ), env->cdr, env );
	printf( "definitions : %p\n", (void *) cdr( env, env ) );
	cell * fs = assq( symbol( 'b', env ), env->cdr, env );
	printf( "assq(b, env) : %016lx\n", (void *) fs->header );
	cell * nf = assq( symbol( 'd', env ), env->cdr, env );
	printf( "assq(d, env) : %016lx\n", (void *) nf->header );

	return env;
}

int main( )
{
	halt( sire( 0 ) );
	return 0;
}

