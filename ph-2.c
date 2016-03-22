//
// Pre-history 2: read and print loop, no eval yet
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
#define dprintf( ... ) if( verbose ) fprintf( stderr,  __VA_ARGS__ )
#define cell_type( c )    ( c->header & MASK_TYPE )
#define symbol_value( s ) ( ( s->header & MASK_SYMBOL )  >> 8 )

struct _cell;
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
	exit( n );
}

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
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

	null->size = size;

	return null;
}

static cell * symbol( cell * null, unsigned char c )
{
	cell * i = allocate( null, 1 );
	i->header = ( c << 8 ) + CELL_SYMBOL;
	return i;
}

static cell * integer( cell * null, unsigned char n )
{
	cell * i = allocate( null, 1 );
	i->header = ( n << 8 ) + CELL_INTEGER;
	return i;
}

unsigned char put_char( unsigned char c )
{
	return fputc( c, stdout );
}

unsigned char get_char( )
{
	return fgetc( stdin );
}

// procedures…
static cell * car( cell * null, cell * c ) { return c->car; }

static cell * cdr( cell * null, cell * c ) { return c->cdr; }

static cell * set_car( cell * null, cell * c, cell * x ) { c->car = x; return x; }

static cell * set_cdr( cell * null, cell * c, cell * x ) { c->cdr = x; return x; }

static cell * is_null( cell * null, cell * c )  { return ( cell_type( c ) == CELL_NULL )  ? null->size : null; }

static cell * is_tuple( cell * null, cell * c ) { return ( cell_type( c ) == CELL_TUPLE ) ? null->size : null; }

static cell * is_atom( cell * null, cell * c ) { return ( is_tuple( null, c ) != null ) ? null : null->size; }

static cell * is_symbol( cell * null, cell * c )  { return ( cell_type( c ) == CELL_SYMBOL ) ? null->size : null; }

static cell * is_integer( cell * null, cell * c ) { return ( cell_type( c ) == CELL_INTEGER ) ? null->size : null; }

static cell * cons( cell * null, cell * a, cell * b )
{
	cell * t = allocate( null, 3 );
	t->header = CELL_TUPLE;
	t->car = a;
	t->cdr = b;
	return t;
}

static cell * equals( cell * null, cell * a, cell * b )
{
	if( ( is_atom( null, a ) != null ) && ( is_atom( null, b ) != null ) )
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
	if( is_null( null, alist ) != null )
	{
		return null;
	}
	else
	{
		if( equals( null, key, car( null, car( null, alist ) ) ) != null )
		{
			return car( null, alist );
		}
		else
		{
			return assq( null, key, cdr( null, alist ) );
		}
	}
}

static cell * reverse( cell * null, cell * lst )
{
	cell * rev = null;
	while( 1 )
	{
		if( lst == null )
		{
			return rev;
		}
		else
		{
			rev = cons( null, lst->car, rev );
			lst = lst->cdr;
		}
	}
}

static cell * print_integer( cell * null, cell * exp )
{
	unsigned long i;
	put_char( '0' ); put_char( 'x' );
	i = ( exp->header & MASK_INTEGER_HI ) >> 12;
	i += ( i > 0x09 ) ? 0x57 : 0x30; 
	put_char( i );

	i = ( exp->header & MASK_INTEGER_LO ) >> 8;
	i += ( i > 0x09 ) ? 0x57 : 0x30; 
	put_char( i );

	return exp;
}

static cell * print( cell * null, cell * exp );
cell * print_list( cell * null, cell * lst )
{
	put_char( '(' );
	while( 1 )
	{
		print( null, lst->car );

		if( lst->cdr == null )
		{
			put_char( ')' );
			break;
		}
		lst = lst->cdr;
		if( is_tuple( null, lst ) == null )
		{
			put_char( ' ' ); put_char( '.' ); put_char( ' ' );
			print( null, lst->cdr );
			put_char( ')' );
			break;
		}
		put_char( ' ' );
	}
	return lst;
}

static cell * print( cell * null, cell * exp )
{
	switch( cell_type( exp ) )
	{
		case CELL_NULL:
			put_char( '(' ); put_char( ')' );
			break;
		case CELL_TUPLE:
			print_list( null, exp );
			break;
		case CELL_SYMBOL:
			put_char( symbol_value( exp ) );
			break;
		case CELL_INTEGER:
			print_integer( null, exp );
			break;
	}
	return exp;
}

static cell * read_integer( cell * null )
{
	unsigned char c = get_char( );
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
			halt( 3 );
		}
	}
	c = c * 0x10;
	unsigned char d = get_char( );
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
	return integer( null, c );
}

static cell * read( cell * null );
static cell * read_list( cell * null, cell * lst )
{
	cell * c = read( null );

	if( ( cell_type( c ) == CELL_SYMBOL )
	 && ( symbol_value( c ) == '.' )
         && ( cell_type( lst ) == CELL_TUPLE ) )
	{
		cell * d = read( null );
		if( ( cell_type( d ) != CELL_SYMBOL )
		 || ( symbol_value( d ) != ')' ) )
		{
			cell * e = read( null );
			if( ( cell_type( e ) == CELL_SYMBOL )
			 && ( symbol_value( e ) == ')' ) )
			{
				if( cell_type( lst->cdr ) == CELL_NULL )
				{
					return cons( null, lst->car, cons( null, c, cons( null, d, null ) ) );
				}
			}
		}
		halt( 4 );
	}
	else
	{
		if( ( cell_type( c ) == CELL_SYMBOL )
		 && ( symbol_value( c ) == ')' ) )
		{
			return reverse( null, lst );
		}
		else
		{
			return read_list( null, cons( null, c, lst ) );
		}
	}
}

static cell * read( cell * null )
{
	unsigned char c, d;

	c = get_char( );
	if( ( c == ' ' ) || ( c == '\t' ) || ( c == '\n' ) )
	{
		return read( null );
	}
	if( c == '0' ) // 0x.. ?
	{
		d = get_char( );
		if( d == 'x' )
		{
			return read_integer( null );
		}
		else
		{
			halt( 3 );
		}
	}
	if( ( c >= 0x30 ) && ( c <= 0x39 ) ) // 0 - 9 sans 0x prefix
	{
		halt( 3 );
	}
	if( c == '(' )
	{
		return read_list( null, null );
	}
	if( c == ')' )
	{
		return symbol( null, c );
	}
	if( ( c >= 0x21 ) && ( c <= 0x7e ) ) // printable
	{
		return symbol( null, c );
	}
	
	halt( 2 );
}

static cell * repl( cell * null, cell * env )
{
	put_char( '>' ); put_char( ' ' );
	print( null, read( null ) );
	put_char( '\n' );

	return repl( null, env );
}

int main( )
{
	cell * null = sire( );
	cell * env  = null;

	repl( null, env );
	return 0;
}

