//
// Pre-history 3: add eval.
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
#define CELL_OPERATOR		0x05
#define CELL_FUNCTION		0x15	// machine language, no REPL access
#define CELL_PROCEDURE		0x25	// machine language, REPL access
#define CELL_LAMBDA		0x35	// operative s-expressions
#define CELL_FEXPR		0x45	// applicative s-expressions
#define MASK_TYPE		0x07
#define MASK_SYMBOL		0xff00
#define MASK_INTEGER		0xff00
#define MASK_INTEGER_HI		0xf000
#define MASK_INTEGER_LO		0x0f00
#define MASK_OPERATOR		0x00f5

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )
#define operator_type( c ) ( c->header & MASK_OPERATOR )
#define symbol_value( s )  ( ( s->header & MASK_SYMBOL )  >> 8 )
#define integer_value( s ) ( ( s->header & MASK_INTEGER ) >> 8 )

struct _cell;
typedef struct _cell cell;
struct _cell
{
	unsigned long header;
	union
	{
		struct { cell * size; void * arena, * next; };
		struct { cell * car, * cdr; };
		struct { cell * operation; };
		struct { void * bytes; };
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

static cell * function( cell * null, cell * exp ) { halt( 9 ); return null; }
static cell * procedure( cell * null, cell * exp ) { halt( 9 ); return null; }

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

static cell * lambda( cell * null, cell * exp )
{
	cell * i = allocate( null, 2 );
	i->header = CELL_LAMBDA;
	i->operation = exp;
	return i;
}

static cell * fexpr( cell * null, cell * exp )
{
	cell * i = allocate( null, 2 );
	i->header = CELL_FEXPR;
	i->operation = exp;
	return i;
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
			print( null, lst );
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
		case CELL_OPERATOR:
			switch( operator_type( exp ) )
			{
				case CELL_FUNCTION:
				case CELL_PROCEDURE:
					halt( 9 );
					break;
				case CELL_LAMBDA:
				case CELL_FEXPR:
					print( null, exp->operation );
					break;
			}
			break;
	}
	return exp;
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

static cell * eval( cell * null, cell * exp, cell * env );
static cell * apply_terms( cell * null, cell * formals, cell * args, cell * terms, cell * env )
{
	dprintf( "apply_terms:\n" );

	cell * tmp  = env;				// any set! will be lost on return
	while( is_tuple( null, formals ) != null )
	{
		tmp = cons( null, cons( null, formals->car, args->car ), tmp );
		formals = formals->cdr;
		args = args->cdr;
	}

	cell * ans, * res;
	while( is_tuple( null, terms ) != null )
	{
		ans = eval( null, terms->car, tmp );
		res = ans->car;
		tmp = ans->cdr;
		terms = terms->cdr;
	}

	return cons( null, res, env );
}

static cell * eval_list( cell * null, cell * lst, cell * env );
static cell * apply( cell * null, cell * exp, cell * args, cell * env )
{
	dprintf( "apply:\n" );
	dprintf( "apply: exp: " ); print( null, exp ); put_char( '\n' );
	dprintf( "apply: env: " ); print( null, env ); put_char( '\n' );
	cell * ans = eval( null, exp, env );
	cell * op  = ans->car;
	dprintf( "apply:  op: " ); print( null, op ); put_char( '\n' );
	dprintf( "apply:  op %016lx\n", op->header );
	env = ans->cdr;

	switch( operator_type( op ) )
	{
		case CELL_FUNCTION:
			halt( 99 );
			break;
		case CELL_PROCEDURE:
		{
			cell * (* proc)(cell *, cell *) = (void *) op->bytes;
			halt( 99 );
			break;
		}
		case CELL_LAMBDA:
		{
			dprintf( "apply: ^\n" );
			ans  = eval_list( null, args, env );
			args = ans->car; env = ans->cdr;

			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			return apply_terms( null, fmls, args, body, env );
			break;
		}
		case CELL_FEXPR:
		{
			dprintf( "apply: $\n" );
			args = cons( null, car( null, args ), cons( null, env, null ) );

			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			return apply_terms( null, fmls, args, body, env );
			break;
		}
	}
	halt( 6 );
}

static cell * eval_list( cell * null, cell * lst, cell * env )
{
	cell * res = null;
	while( is_tuple( null, lst ) != null )
	{
		cell * ans = eval( null, lst->car, env );
		res = cons( null, ans->car, res );
		env = ans->cdr;
		lst = lst->cdr;
	}
	return cons( null, reverse( null, res ), env );
}

static cell * eval( cell * null, cell * exp, cell * env )
{
	switch( cell_type( exp ) )
	{
		case CELL_NULL:
			dprintf( "eval: null\n" );
			return cons( null, null, env );
			break;
		case CELL_TUPLE:
		{
			dprintf( "eval: list\n" );
			cell * first = exp->car;
			dprintf( "eval: list, first = %016lx\n", first->header );
			if( is_symbol( null, first ) != null )
			{
				dprintf( "eval: list, is special?\n" );
				switch( symbol_value( first ) )
				{
					case '!':
					{
						dprintf( "eval: !\n" );
						cell * key = exp->cdr->car;
						cell * ans = eval( null, exp->cdr->cdr->car, env );
						cell * val = ans->car;
						env = ans->cdr;

						cell * tuple = assq( null, key, env );
						if( is_null( null, tuple ) != null )
						{
							tuple = cons( null, key, val );
							env = cons( null, tuple, env );
						}
						else
						{
							set_cdr( null, tuple, val );
						}
						return cons( null, tuple, env );
						break;
					}
					case '?':
					{
						dprintf( "eval: ?\n" );
						cell * tst = exp->cdr->car;
						cell * con = exp->cdr->cdr->car;
						cell * alt = exp->cdr->cdr->cdr->car;

						cell * ans = eval( null, tst, env );
						tst = ans->car;
						env = ans->cdr;
						if( is_null( null, tst ) != null )
						{
							cell * ans = eval( null, alt, env );
							return cons( null, ans->car, ans->cdr );
						}
						else
						{
							cell * ans = eval( null, con, env );
							return cons( null, ans->car, ans->cdr );
						}
						break;
					}
					case '^':
					{
						dprintf( "eval: ^\n" );
						return cons( null, lambda( null, exp ), env );
						break;
					}
					case '$':
					{
						dprintf( "eval: $\n" );
						return cons( null, fexpr( null, exp ), env );
						break;
					}
				}
			}
			dprintf( "eval: not special, apply…\n" );
			return apply( null, first, exp->cdr, env );
			break;
		}
		case CELL_SYMBOL:
		{
			dprintf( "eval: symbol\n" );
			cell * tuple = assq( null, exp, env );
			if( is_tuple( null, tuple ) != null )
			{
				return cons( null, tuple->cdr, env );
			}
			else
			{
				return cons( null, null, env );
			}
			break;
		}
		case CELL_INTEGER:
			dprintf( "eval: integer\n" );
			return cons( null, exp, env );
			break;
	}
	halt( 4 );
}

static cell * repl( cell * null, cell * env )
{
	put_char( '>' ); put_char( ' ' );
	cell * ans = eval( null, read( null ), env );

	dprintf( "repl: ans: " );
	print( null, ans->car ); put_char( '\n' );
	dprintf( "repl: env: " ); print( null, ans->cdr ); put_char( '\n' );

	return repl( null, ans->cdr );
}

int main( )
{
	cell * null = sire( );
	cell * env  = null;

	repl( null, env );
	return 0;
}

