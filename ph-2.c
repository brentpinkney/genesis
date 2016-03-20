//
// Pre-history 2: read and print loop, no eval yet
//
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

#define PAGE_SIZE	4096
#define WORD_SIZE	8
#define MASK_TYPE	0x07
#define MASK_SYMBOL	0xff00
#define MASK_INTEGER_HI	0xf000
#define MASK_INTEGER_LO	0x0f00
#define TYPE_NULL	0x01
#define TYPE_TUPLE	0x02
#define TYPE_SYMBOL	0x03
#define TYPE_INTEGER	0x04

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stderr,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )
#define symbol_value( s ) ( ( s->header & MASK_SYMBOL ) >> 8 )

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

static cell * allocate( unsigned long words, cell * env ) // anomaly
{
	cell * this = env->car->next;
	env->car->next = ( (void *) env->car->next ) + ( words * WORD_SIZE );
	return this;
}

static cell * cons( cell * a, cell * b, cell * env )
{
	cell * t = allocate( 3, env );
	t->header = TYPE_TUPLE;
	t->car = a;
	t->cdr = b;
	return t;
}

static cell * symbol( unsigned char c, cell * env )
{
	cell * i = allocate( 1, env );
	i->header = ( c << 8 ) + TYPE_SYMBOL;
	return i;
}

static cell * integer( unsigned char n, cell * env )
{
	cell * i = allocate( 1, env );
	i->header = ( n << 8 ) + TYPE_INTEGER;
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
		return env->car;
	}
	else
	{
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

unsigned char put_char( unsigned char c, cell * env ) // anomaly
{
	return fputc( c, stdout );
}

unsigned char get_char( cell * env ) // anomaly
{
	return fgetc( stdin );
}

static cell * halt( unsigned long n, cell * env )
{
	exit( n );
	return env;
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
	if( arena == MAP_FAILED )
	{
		halt( 1, ignore );
	}

	// no environment yet, so build null by hand…
	cell * null  = arena;
	null->header = TYPE_NULL;
	null->size   = size;
	null->arena  = arena;
	null->next   = arena + ( 4 * WORD_SIZE );

	// now build the environment…
	cell * env  = null->next;
	env->header = TYPE_TUPLE;
	env->car    = null;
	env->cdr    = null;
	null->next  = null->next + ( 3 * WORD_SIZE );

	return env;
}

static cell * print_integer( cell * exp, cell * env )
{
	unsigned long i;
	put_char( '0', env ); put_char( 'x', env );
	i = ( exp->header & MASK_INTEGER_HI ) >> 12;
	i += ( i > 0x09 ) ? 0x57 : 0x30; 
	put_char( i, env );

	i = ( exp->header & MASK_INTEGER_LO ) >> 8;
	i += ( i > 0x09 ) ? 0x57 : 0x30; 
	put_char( i, env );

	return env;
}

static cell * print( cell * exp, cell * env );
cell * print_list( cell * lst, cell * env )
{
	put_char( '(', env );
	while( 1 )
	{
		print( lst->car, env );

		if( lst->cdr == env->car )
		{
			put_char( ')', env );
			break;
		}
		lst = lst->cdr;
		if( is_tuple( lst, env ) == env->car )
		{
			put_char( ' ', env ); put_char( '.', env ); put_char( ' ', env );
			print( lst->cdr, env );
			put_char( ']', env );
			break;
		}
		put_char( ' ', env );
	}
	return lst;
}

static cell * print( cell * exp, cell * env )
{
	switch( cell_type( exp ) )
	{
		case TYPE_NULL:
			put_char( '(', env ); put_char( ')', env );
			break;
		case TYPE_TUPLE:
			print_list( exp, env );
			break;
		case TYPE_SYMBOL:
			put_char( symbol_value( exp ), env );
			break;
		case TYPE_INTEGER:
			print_integer( exp, env );
			break;
	}
	return env;
}

static cell * reverse( cell * lst, cell * env )
{
	cell * rev = env->car;
	while( 1 )
	{
		if( lst == env->car )
		{
			return rev;
		}
		else
		{
			rev = cons( lst->car, rev, env );
			lst = lst->cdr;
		}
	}
}

static cell * read_integer( cell * env )
{
	unsigned char c = get_char( env );
	if( ( c >= 0x30 ) && ( c <= 0x39 ) ) // 0 - 9
	{
		c -= 0x30;
	}
	else
	{	
		if( ( c >= 0x61 ) && ( c <= 0x66 ) ) // a - f
		{
			c -= 0x57;
		}
		else
		{
			halt( 3, env );
		}
	}
	c = c * 0x10;
	unsigned char d = get_char( env );
	if( ( d >= 0x30 ) && ( d <= 0x39 ) ) // 0 - 9
	{
		d -= 0x30;
	}
	else
	{	
		if( ( d >= 0x61 ) && ( d <= 0x66 ) ) // a - f
		{
			d -= 0x57;
		}
	}
	c += d;
	return integer( c, env );
}

static cell * read( cell * env );
static cell * read_list( cell * lst, cell * env )
{
	cell * c = read( env );

	if( ( cell_type( c ) == TYPE_SYMBOL )
	 && ( symbol_value( c ) == '.' )
         && ( cell_type( lst ) == TYPE_TUPLE ) )
	{
		cell * d = read( env );
		if( ( cell_type( d ) != TYPE_SYMBOL )
		 || ( symbol_value( d ) != ')' ) )
		{
			cell * e = read( env );
			if( ( cell_type( e ) == TYPE_SYMBOL )
			 && ( symbol_value( e ) == ')' ) )
			{
				if( cell_type( lst->cdr ) == TYPE_NULL )
				{
					return cons( lst->car, cons( c, cons( d, env->car, env ), env ), env );
				}
				else
				{
					return halt( 4, env );
				}
			}
			else
			{
				return halt( 4, env );
			}
		}
		else
		{
			return halt( 4, env );
		}
	}
	else
	{
		if( ( cell_type( c ) == TYPE_SYMBOL )
		 && ( symbol_value( c ) == ')' ) )
		{
			return reverse( lst, env );
		}
		else
		{
			return read_list( cons( c, lst, env ), env );
		}
	}
}

static cell * read( cell * env )
{
	unsigned char c, d;

	c = get_char( env );
	if( ( c == ' ' ) || ( c == '\t' ) || ( c == '\n' ) )
	{
		return read( env );
	}
	if( c == '0' ) // 0x.. ?
	{
		d = get_char( env );
		if( d == 'x' )
		{
			return read_integer( env );
		}
		else
		{
			return halt( 3, env );
		}
	}
	if( ( c >= 0x30 ) && ( c <= 0x39 ) ) // 0 - 9 sans 0x prefix
	{
		return halt( 3, env );
	}
	if( c == '(' )
	{
		return read_list( env->car, env );
	}
	if( c == ')' )
	{
		return symbol( c, env );
	}
	if( ( c >= 0x21 ) && ( c <= 0x7e ) ) // printable
	{
		return symbol( c, env );;
	}
	
	return halt( 2, env );
}

static cell * repl( cell * env )
{
	put_char( '>', env ); put_char( ' ', env );

	env = print( read( env ), env );

	put_char( '\n', env );

	return repl( env );
}

int main( )
{
	repl( sire( 0 ) );
	return 0;
}

