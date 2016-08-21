//
// Pre-history 3: add eval, no functions from the REPL yet."
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
#define CELL_OPERATOR		0x05	// 0000 0101
#define CELL_PROCEDURE		0x05	// 0000 0101 machine language, REPL access
#define CELL_FUNCTION		0x25	// 0010 0101 machine language, no REPL access
#define CELL_LAMBDA		0x15	// 0001 0101 operative s-expressions
#define CELL_FEXPR		0x35	// 0011 0101 applicative s-expressions
#define MASK_TYPE		0x0f	// 0000 1111
#define MASK_OPERATOR		0x00f5	// 1111 0101
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
		struct { cell * zero; void * arena, * extent, * next; };
		struct { cell * car, * cdr; };
		struct { cell * operation; };
		struct { void * address; };
	};
};

// procedures…
static void quit( unsigned long n ) { exit( n ); }

static unsigned long cell_type( cell * c )     { return c->header & MASK_TYPE; }
static unsigned long operator_type( cell * c ) { return c->header & MASK_OPERATOR; }
static unsigned long symbol_value( cell * s )  { return ( s->header >> 16 ) & 0xff; }
static unsigned long integer_value( cell * s ) { return ( s->header >> 16 ) & 0xff; }

static unsigned long get_char( ) { return fgetc( stdin ); }
static unsigned long put_char( unsigned long c ) { return fputc( c, stdout ); }

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
	if( null->next > null->extent ) quit( 2 );
	return this;
}

static cell * sire( unsigned long pages )
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
	null->zero   = (cell *) CELL_INTEGER;		// a cell that is not null

	return null;
}

static cell * symbol( cell * null, unsigned long c )
{
	cell * i = allocate( null, 1 );
	i->header = ( c << 16 ) + CELL_SYMBOL;
	return i;
}

static cell * integer( cell * null, unsigned long n )
{
	cell * i = allocate( null, 1 );
	i->header = ( n << 16 ) + CELL_INTEGER;
	return i;
}

static cell * expression( cell * null, unsigned char type, cell * exp )
{
	cell * i = allocate( null, 2 );
	i->header = type;
	i->operation = exp;
	return i;
}

// functions…
static cell * car( cell * null, cell * c ) { return c->car; }

static cell * cdr( cell * null, cell * c ) { return c->cdr; }

static cell * is_null( cell * null, cell * c )  { return ( cell_type( c ) == CELL_NULL )  ? null->zero : null; }

static cell * is_tuple( cell * null, cell * c ) { return ( cell_type( c ) == CELL_TUPLE ) ? null->zero : null; }

static cell * is_symbol( cell * null, cell * c )  { return ( cell_type( c ) == CELL_SYMBOL ) ? null->zero : null; }

static cell * is_integer( cell * null, cell * c ) { return ( cell_type( c ) == CELL_INTEGER ) ? null->zero : null; }

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
	return ( ( is_tuple( null, a ) is_true ) || ( is_tuple( null, b ) is_true ) )
		? ( a == b ) ? null->zero : null
		: ( a->header == b->header ) ? null->zero : null;
}

static cell * assq( cell * null, cell * key, cell * alist )
{
	return ( alist == null )
		? null
		: ( equals( null, key, alist->car->car ) is_true )
			? alist->car
			: assq( null, key, alist->cdr );
}

static cell * reverse( cell * null, cell * lst )
{
	cell * rev = null;
	while( lst != null )
	{
		rev = cons( null, lst->car, rev );
		lst = lst->cdr;
	}
	return rev;
}

static cell * print_integer( cell * null, cell * exp )
{
	unsigned long i;
	put_char( '0' ); put_char( 'x' );
	i = ( exp->header & MASK_INTEGER_HI ) >> 20;
	i += ( i > 0x09 ) ? ( 'a' - 10 ) : '0';
	put_char( i );

	i = ( exp->header & MASK_INTEGER_LO ) >> 16;
	i += ( i > 0x09 ) ? ( 'a' - 10 ) : '0';
	put_char( i );

	return exp;
}

static cell * print( cell * null, cell * exp );
static cell * print_list( cell * null, cell * lst )
{
	cell * c = lst;
	cell * queue = null;
	while( 1 )
	{
		queue = cons( null, c, queue );
		if( is_tuple( null, c ) is_true ) c = c->cdr; else break;
	}

	if( queue->car == null )
	{
		put_char( '(' );
		c = lst;
		while( 1 )
		{
			print( null, c->car );

			if( c->cdr == null )
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
			if( c == null )
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
			put_char( ')' );
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
		case CELL_OPERATOR:
			switch( operator_type( exp ) )
			{
				case CELL_PROCEDURE:
					quit( 10 );
					break;
				case CELL_FUNCTION:
					put_char( 'F' ); put_char( '0' + integer_value( exp ) );
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

static cell * read_integer( cell * null )
{
	unsigned long c = get_char( );
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
			quit( 4 );
		}
	}
	c = c << 4;
	unsigned long d = get_char( );
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
	return integer( null, c );
}

static cell * read( cell * null );
static cell * read_list( cell * null, cell * lst )
{
	cell * c = read( null );
	if( ( cell_type( c ) == CELL_SYMBOL ) && ( symbol_value( c ) == ')' ) )
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
	unsigned long c, d;

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
			quit( 3 );
		}
	}
	if( ( c >= '0' ) && ( c <= '9' ) ) // 0 - 9 sans 0x prefix
	{
		quit( 3 );
	}
	if( c == '(' )
	{
		return read_list( null, null );
	}
	if( c == ')' )
	{
		return symbol( null, c );
	}
	return symbol( null, c ); // may not be printable
	
	quit( 3 );
	return null;
}

static cell * eval( cell * null, cell * exp, cell * env );
static cell * apply_forms( cell * null, cell * formals, cell * args, cell * terms, cell * env )
{
	cell * tmp  = env;
	while( is_tuple( null, formals ) is_true )
	{
		cell * arg;
		if( ( formals->cdr == null ) && ( args->cdr != null ) )
		{
			arg = args;
		}
		else
		{
			if( is_tuple( null, args ) is_true )
			{
				arg  = args->car;
				args = args->cdr;
			}
			else
			{
				arg = null;
			}
		}
		tmp = cons( null, cons( null, formals->car, arg ), tmp );
		formals = formals->cdr;
	}

	cell * ans, * res = null;
	while( is_tuple( null, terms ) is_true )
	{
		ans = eval( null, terms->car, tmp );
		res = ans->car;
		tmp = ans->cdr;
		terms = terms->cdr;
	}

	return cons( null, res, tmp );
}

static cell * eval_list( cell * null, cell * lst, cell * env );
static cell * apply( cell * null, cell * op, cell * args, cell * env )
{
	cell * ans;
	printf( "apply: " ); print( null, op ); printf( " to " ); print( null, args ); put_char( '\n' );
	switch( operator_type( op ) )
	{
		case CELL_PROCEDURE:
			quit( 9 );
			break;
		case CELL_FUNCTION:
			quit( 9 );
			break;
		case CELL_LAMBDA:
		{
			printf( "apply: ^\n" );
			ans  = eval_list( null, args, env );
			args = ans->car; env = ans->cdr;

			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			ans = apply_forms( null, fmls, args, body, env );
			return cons( null, ans->car, env );			// any set! will be lost on return
		}
		case CELL_FEXPR:
		{
			printf( "apply: $\n" );
			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			cell * tmp = env;
			tmp = cons( null, cons( null, fmls->car, args ), tmp );		// E
			tmp = cons( null, cons( null, fmls->cdr->car, env ), tmp );	// V

			cell * ans, * res = null;
			cell * terms = body;
			while( is_tuple( null, terms ) is_true )
			{
				ans = eval( null, terms->car, tmp );
				res = ans->car;
				tmp = ans->cdr;
				terms = terms->cdr;
			}

			return cons( null, res, env );				// any set! will be lost on return
		}
	}
	quit( 7 );
	return null;
}

static cell * eval_list( cell * null, cell * lst, cell * env )
{
	cell * res = null;
	while( is_tuple( null, lst ) is_true )
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
			printf( "eval: null\n" );
			return cons( null, null, env );
		case CELL_TUPLE:
		{
			printf( "eval: list\n" );
			cell * first = exp->car;
			if( is_symbol( null, first ) is_true )
			{
				printf( "eval: list, is special?\n" );
				switch( symbol_value( first ) )
				{
				case '!':
				{
					printf( "eval: !\n" );
					cell * key = exp->cdr->car;
					cell * ans = eval( null, exp->cdr->cdr->car, env );
					cell * val = ans->car;
					env = ans->cdr;
					cell * tuple = cons( null, key, val );

					cell * head = null;
					cell * rest = env;
					while( rest != null )
					{
						if( equals( null, key, rest->car->car ) is_true ) break;
						head = cons( null, rest->car, head );
						rest = rest->cdr;
					}
					if( rest == null )
					{
						env = cons( null, tuple, env );
					}
					else
					{
						env = cons( null, tuple, rest->cdr );

						while( head != null )
						{
							env = cons( null, head->car, env );
							head = head->cdr;
						}
					}

					return cons( null, tuple, env );
				}
				case '?':
				{
					printf( "eval: ?\n" );
					cell * tst = exp->cdr->car;
					cell * con = exp->cdr->cdr->car;
					cell * alt = exp->cdr->cdr->cdr->car;

					cell * ans = eval( null, tst, env );
					tst = ans->car;
					env = ans->cdr;
					if( tst == null )
					{
						cell * ans = eval( null, alt, env );
						return cons( null, ans->car, ans->cdr );
					}
					else
					{
						cell * ans = eval( null, con, env );
						return cons( null, ans->car, ans->cdr );
					}
				}
				case '^':
				{
					printf( "eval: ^\n" );
					return cons( null, expression( null, CELL_LAMBDA, exp ), env );
				}
				case '$':
				{
					printf( "eval: $\n" );
					return cons( null, expression( null, CELL_FEXPR, exp ), env );
				}
			}
			}
			// else
			printf( "eval: applying " ); print( null, first ); printf( " to " ); print( null, exp->cdr ); put_char( '\n' );
			cell * ans = eval( null, first, env );
			printf( "eval: about to apply " ); print( null, ans->car ); put_char( '\n' );
			return apply( null, ans->car, exp->cdr, ans->cdr );
		}
		case CELL_SYMBOL:
		{
			printf( "eval: symbol\n" );
			cell * tuple = assq( null, exp, env );
			cell * value = ( is_tuple( null, tuple ) is_true ) ? tuple->cdr : null;
			return cons( null, value, env );
		}
		case CELL_INTEGER:
			printf( "eval: integer\n" );
			return cons( null, exp, env );
	}
	quit( 5 );
	return null;
}

static cell * repl( cell * null, cell * env )
{
	put_char( '>' ); put_char( ' ' );
	cell * ans = eval( null, read( null ), env );

	printf( "repl: ans: " );
	print( null, ans->car ); put_char( '\n' );
	printf( "repl: env: " ); print( null, ans->cdr ); put_char( '\n' );

	return repl( null, ans->cdr );
}

int main( )
{
	cell * null = sire( NUM_PAGES );
	cell * env  = null;

	repl( null, env );
	return 0;
}
