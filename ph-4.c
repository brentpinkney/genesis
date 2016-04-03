//
// Pre-history 4: Implement procedure calls (primitives) from the REPL.
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
#define CELL_FUNCTION		0x05	// machine language, no REPL access
#define CELL_PROCEDURE		0x15	// machine language, REPL access
#define CELL_LAMBDA		0x25	// operative s-expressions
#define CELL_FEXPR		0x35	// applicative s-expressions
#define MASK_TYPE		0x07
#define MASK_SYMBOL		0xff00
#define MASK_INTEGER		0xff00
#define MASK_INTEGER_HI		0xf000
#define MASK_INTEGER_LO		0x0f00
#define MASK_OPERATOR		0x00f5

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c ) ( c->header & MASK_TYPE )
#define symbol_value( s )  ( ( s->header & MASK_SYMBOL )  >> 8 )
#define integer_value( s ) ( ( s->header & MASK_INTEGER ) >> 8 )
#define operator_type( c ) ( c->header & MASK_OPERATOR )

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
	unsigned long bytes = PAGE_SIZE * 1024;
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

// static cell * function( cell * null, cell * exp ) { halt( 99 ); return null; }

static cell * procedure( cell * null, unsigned long nargs, void * bytes )
{
	cell * p = allocate( null, 2 );
	p->header = ( nargs << 8 ) + CELL_PROCEDURE;	
	p->bytes  = bytes;
	return p;
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
		case CELL_OPERATOR:
			switch( operator_type( exp ) )
			{
				case CELL_FUNCTION:
					halt( 9 );
					break;
				case CELL_PROCEDURE:
					put_char( 'p' ); put_char( '0' + integer_value( exp ) );
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
	return null;
}

static cell * eval( cell * null, cell * exp, cell * env );
static cell * apply_forms( cell * null, cell * formals, cell * args, cell * terms, cell * env )
{
	cell * tmp  = env;
	while( is_tuple( null, formals ) is_true )
	{
		cell * arg;
		if( ( is_null( null, formals->cdr ) is_true )
			&& ( is_null( null, args->cdr ) is_false ) )
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
	switch( operator_type( op ) )
	{
		case CELL_FUNCTION:
			halt( 8 );
			break;
		case CELL_PROCEDURE:
		{
			int evaluated = 0;
			if( op->bytes == eval )
			{
				evaluated = 1;
				ans = eval( null, args->car, env );
			}
			else if( op->bytes == eval_list )
			{
				evaluated = 1;
			}
			else if( op->bytes == apply )
			{
				evaluated = 1;
				ans  = eval( null, args->car, env );
				op   = ans->car; env = ans->cdr;
				args = args->cdr->car;
				ans  = apply( null, op, args, env );
			}
			// now invoke…
			ans = eval_list( null, args, env );
			args = ans->car; env = ans->cdr;
			switch( integer_value( op ) )
			{
				case 0:
				{
					cell * (* proc)(cell *) = (void *) op->bytes;
					ans = proc( null );
					break;
				}
				case 1:
				{
					cell * (* proc)(cell *, cell *) = (void *) op->bytes;
					ans = proc( null, args->car );
					break;
				}
				case 2:
				{
					cell * (* proc)(cell *, cell *, cell *) = (void *) op->bytes;
					ans = proc( null, args->car, args->cdr->car );
					break;
				}
				case 3:
				{
					cell * (* proc)(cell *, cell *, cell *, cell *) = (void *) op->bytes;
					ans = proc( null, args->car, args->cdr->car, args->cdr->cdr->car );
					break;
				}
				case 4:
				{
					cell * (* proc)(cell *, cell *, cell *, cell *, cell *) = (void *) op->bytes;
					ans = proc( null, args->car, args->cdr->car, args->cdr->cdr->car, args->cdr->cdr->cdr->car );
					break;
				}
				default:
					halt( 7 );
			}
			return evaluated
				? ans
				: cons( null, ans, env );	// any set! will be lost on return
		}
		case CELL_LAMBDA:
		{
			ans  = eval_list( null, args, env );
			args = ans->car; env = ans->cdr;

			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			ans = apply_forms( null, fmls, args, body, env );
			return cons( null, ans->car, env );	// any set! will be lost on return
		}
		case CELL_FEXPR:
		{
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

			return cons( null, res, env );		// any set! will be lost on return
		}
	}
	halt( 6 );
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
			return cons( null, null, env );
		case CELL_TUPLE:
		{
			cell * first = exp->car;
			if( is_symbol( null, first ) is_true )
			{
				switch( symbol_value( first ) )
				{
					case '!':
					{
						cell * key = exp->cdr->car;
						cell * ans = eval( null, exp->cdr->cdr->car, env );
						cell * val = ans->car;
						env = ans->cdr;

						cell * tuple = assq( null, key, env );
						if( is_null( null, tuple ) is_true )
						{
							tuple = cons( null, key, val );
							env = cons( null, tuple, env );
						}
						else
						{
							set_cdr( null, tuple, val );
						}
						return cons( null, tuple, env );
					}
					case '?':
					{
						cell * tst = exp->cdr->car;
						cell * con = exp->cdr->cdr->car;
						cell * alt = exp->cdr->cdr->cdr->car;

						cell * ans = eval( null, tst, env );
						tst = ans->car;
						env = ans->cdr;
						if( is_null( null, tst ) is_true )
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
						return cons( null, lambda( null, exp ), env );
					}
					case '$':
					{
						return cons( null, fexpr( null, exp ), env );
					}
				}
			}
			// else
			cell * ans  = eval( null, first, env );
			return apply( null, ans->car, exp->cdr, ans->cdr );
		}
		case CELL_SYMBOL:
		{
			cell * tuple = assq( null, exp, env );
			cell * value = ( is_tuple( null, tuple ) is_true ) ? tuple->cdr : null;
			return cons( null, value, env );
		}
		case CELL_INTEGER:
			return cons( null, exp, env );
	}
	halt( 4 );
	return null;
}

static cell * repl( cell * null, cell * env )
{
	put_char( '>' ); put_char( ' ' );
	cell * ans = eval( null, read( null ), env );

	print( null, ans->car ); put_char( '\n' );

	return repl( null, ans->cdr );
}

int main( )
{
	cell * null = sire( );
	cell * env  = null;

	env = cons( null, cons( null, symbol( null, '#'  ), procedure( null, 1, car           ) ), env );
	env = cons( null, cons( null, symbol( null, '%'  ), procedure( null, 1, cdr           ) ), env );
	env = cons( null, cons( null, symbol( null, 0x00 ), procedure( null, 2, set_car       ) ), env );
	env = cons( null, cons( null, symbol( null, 0x01 ), procedure( null, 2, set_cdr       ) ), env );
	env = cons( null, cons( null, symbol( null, '_'  ), procedure( null, 1, is_null       ) ), env );
	env = cons( null, cons( null, symbol( null, 't'  ), procedure( null, 1, is_tuple      ) ), env );
	env = cons( null, cons( null, symbol( null, 'a'  ), procedure( null, 1, is_atom       ) ), env );
	env = cons( null, cons( null, symbol( null, 's'  ), procedure( null, 1, is_symbol     ) ), env );
	env = cons( null, cons( null, symbol( null, 'i'  ), procedure( null, 1, is_integer    ) ), env );
	env = cons( null, cons( null, symbol( null, '.'  ), procedure( null, 2, cons          ) ), env );
	env = cons( null, cons( null, symbol( null, 0x02 ), procedure( null, 1, lambda        ) ), env );
	env = cons( null, cons( null, symbol( null, 0x03 ), procedure( null, 1, fexpr         ) ), env );
	env = cons( null, cons( null, symbol( null, '='  ), procedure( null, 2, equals        ) ), env );
	env = cons( null, cons( null, symbol( null, 'q'  ), procedure( null, 2, assq          ) ), env );
	env = cons( null, cons( null, symbol( null, '\\' ), procedure( null, 1, reverse       ) ), env );
	env = cons( null, cons( null, symbol( null, 0x04 ), procedure( null, 1, print_integer ) ), env );
	env = cons( null, cons( null, symbol( null, 'p'  ), procedure( null, 1, print         ) ), env );
	env = cons( null, cons( null, symbol( null, 0x05 ), procedure( null, 1, read_integer  ) ), env );
	env = cons( null, cons( null, symbol( null, 0x06 ), procedure( null, 1, read_list     ) ), env );
	env = cons( null, cons( null, symbol( null, 'r'  ), procedure( null, 0, read          ) ), env );
	env = cons( null, cons( null, symbol( null, 0x07 ), procedure( null, 4, apply_forms   ) ), env );
	env = cons( null, cons( null, symbol( null, 'd'  ), procedure( null, 3, apply         ) ), env ); // do…
	env = cons( null, cons( null, symbol( null, 'o'  ), procedure( null, 2, eval_list     ) ), env ); // over…
	env = cons( null, cons( null, symbol( null, 'e'  ), procedure( null, 1, eval          ) ), env );
	env = cons( null, cons( null, symbol( null, 0x08 ), procedure( null, 1, repl          ) ), env );

	repl( null, env );
	return 0;
}
