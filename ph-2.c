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
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )
#define operator_type( c ) ( c->header & MASK_OPERATOR )
#define symbol_value( s )  ( ( s->header & MASK_SYMBOL )  >> 8 )
#define integer_value( s ) ( ( s->header & MASK_INTEGER ) >> 8 )

#define is_true  != null
#define is_false == null

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

static cell * is_atom( cell * null, cell * c ) { return ( is_tuple( null, c ) is_true ) ? null : null->size; }

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
		return null;
	}
	else
	{
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
	i += ( i > 0x09 ) ? ( 'a' - 10 ) : '0';
	put_char( i );

	i = ( exp->header & MASK_INTEGER_LO ) >> 8;
	i += ( i > 0x09 ) ? ( 'a' - 10 ) : '0';
	put_char( i );

	return exp;
}

static cell * print( cell * null, cell * exp );
cell * print_list( cell * null, cell * lst )
{
	cell * c = lst;
	cell * queue = null;
	while( 1 )
	{
		queue = cons( null, c, queue );
		if( is_tuple( null, c ) is_true ) c = c->cdr; else break;
	}

	if( is_null( null, queue->car ) is_true )
	{
		put_char( '(' );
		c = lst;
		while( 1 )
		{
			print( null, c->car );

			if( is_null( null, c->cdr ) is_true )
			{
				put_char( ')' );
				break;
			}
			else
			{
				put_char( ' ' );
				c = c->cdr;
			}
		}
	}
	else
	{
		queue = reverse( null, queue );
		c = queue;
		int depth = 0;
		while( 1 )
		{
			if( is_null( null, c ) is_true )
			{
				break;
			}
			else
			{
				if( is_tuple( null, c->car ) is_true )
				{
					put_char( '(' ); put_char( '.' ); put_char( ' ' );
					depth++;
					print( null, c->car->car );
					put_char( ' ' );
				}
				else
				{
					print( null, c->car );
				}
				c = c->cdr;
			}
		}
		while( depth > 0 )
		{
			put_char( ')');
			depth--;
		}
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
	if( ( c >= '0' ) && ( c <= '9' ) )
	{
		c -= '0';
	}
	else
	{	
		if( ( c >= 'a' ) && ( c <= 'f' ) )
		{
			c -= ( 'a' - 10 );
		}
		else
		{
			halt( 3 );
		}
	}
	c = c << 4;
	unsigned char d = get_char( );
	if( ( d >= '0' ) && ( d <= '9' ) )
	{
		d -= '0';
	}
	else
	{	
		if( ( d >= 'a' ) && ( d <= 'f' ) )
		{
			d -= ( 'a' - 10 );
		}
	}
	c += d;
	return integer( null, (unsigned char) c );
}

static cell * read( cell * null );
static cell * read_list( cell * null, cell * lst )
{
	cell * c = read( null );
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

static cell * read( cell * null )
{
	unsigned char c, d;

	c = get_char( );
	if( c == ';' )
	{
		while( c != '\n' ) c = get_char( );
		return read( null );
	}
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
	if( ( c >= '0' ) && ( c <= '9' ) ) // 0 - 9 sans 0x prefix
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
	if( ( c >= '!' ) && ( c <= '~' ) ) // printable
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

