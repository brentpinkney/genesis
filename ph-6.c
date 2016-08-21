//
// Pre-history 6. Change read, eval, print, describe to use a list of functions.
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
		struct { void * zero, * arena, * extent, * next; };
		struct { cell * car, * cdr; };
		struct { cell * operation; };
		struct { void * address; unsigned char bytes[]; };
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
	null->zero   = (void *) CELL_INTEGER;		// a cell that is not null

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

static cell * length( cell * null, cell * lst );
static cell * code( cell * null, unsigned long type, unsigned long nargs, void * address, cell * bytes )
{
	cell * f;
	unsigned long words;
	if( address )
	{
		words = 2;
		f = allocate( null, words );
		f->address = address;
	}
	else
	{
		unsigned long len = integer_value( length( null, bytes ) );
		words = 2 + ( ( len + WORD_SIZE ) / WORD_SIZE );
		f = allocate( null, words );
		f->address = &( f->bytes );

		for( unsigned long i = 0; i < len; i++ )
		{
			f->bytes[ i ] = (unsigned char) integer_value( bytes->car );
			bytes = bytes->cdr;
		}
	}
	f->header = ( words << 24 ) + ( ( nargs << 16 ) + type );
	return f;
}

static cell * expression( cell * null, unsigned long type, cell * exp )
{
	cell * i = allocate( null, 2 );
	i->header = type;
	i->operation = exp;
	return i;
}

static void toggle_writable( void * address, int writable )
{
	address -= (unsigned long) address % PAGE_SIZE;
	int flag = writable? PROT_WRITE : 0;
	if( mprotect( address, PAGE_SIZE * 2, PROT_READ | PROT_EXEC | flag ) == -1 )
	{
		quit( 1 );
	}
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

static cell * length( cell * null, cell * lst )
{
	unsigned long len = 0;
	while( lst != null )
	{
		len++;
		lst = lst->cdr;
	}
	return integer( null, len );
}

// describers…
typedef cell * (* describer)(cell *, cell *, cell *);

static cell * all_describers;

static cell * d_describe_null( cell * null, cell * env, cell * exp )
{
	unsigned long zero = integer_value( (cell *) &exp->zero );
	printf( "null\nzero:      0x%016lx\tarena:%16p\textent:%16p\tnext:%16p", zero, exp->arena, exp->extent, exp->next );
	printf( "\tused: %.2f%%\n", ( exp->next - exp->arena ) / ( exp->extent - exp->arena ) / 100.0 );
	return exp;
}
static cell * d_describe_tuple( cell * null, cell * env, cell * exp )
{
	printf( "tuple\ncar:     %16p\tcdr:  %16p\n", exp->car, exp->cdr );
	return exp;
}
static cell * d_describe_symbol( cell * null, cell * env, cell * exp )
{
	unsigned long value = symbol_value( exp );
	printf( "symbol\ncharacter: '%s'\t\t\tvalue:  0x%016lx (%ld)\n", (char *) &value, value, value );
	return exp;
}
static cell * d_describe_integer( cell * null, cell * env, cell * exp )
{
	unsigned long value = integer_value( exp );
	printf( "integer\nvalue:     0x%02lx (%ld)\n", value, value );
	return exp;
}
static cell * d_describe_code( cell * null, cell * env, cell * exp )
{
	unsigned long words = ( exp->header >> 24 ) - 2;
	unsigned long bytes = words * WORD_SIZE;
	printf( "%s\narity:     %ld\tsection: %s\taddress:%16p\tlength: %ld\n",
		operator_type( exp ) == CELL_PROCEDURE ? "procedure" : "function",
		integer_value( exp ),
		exp->address < null->arena ? "text" : "arena",
		exp->address,
		words );
	if( exp->address >= null->arena )
	{
		printf( "bytes:     " );
		for( unsigned long i = 0; i < bytes; )
		{
			printf( "%02x ", exp->bytes[ i ] ); 
			i++;
			if( ( i % 8 == 0 ) && ( i < bytes ) ) printf( "\n           " );
		}
		put_char( '\n' );
	}
	return exp;
}
static cell * d_describe_lambda( cell * null, cell * env, cell * exp )
{
	printf( "lambda\noperation: %p\n", exp->operation );
	return exp;
}
static cell * d_describe_fexpr( cell * null, cell * env, cell * exp )
{
	printf( "fexpr\noperation: %p\n", exp->operation );
	return exp;
}

static cell * describe( cell * null, cell * env, cell * exp )
{
	printf( "address: %16p\theader: %016lx\t", exp, exp->header );
	unsigned long type = cell_type( exp );
	cell * describers = all_describers;
	while( describers != null )
	{
		if( type == integer_value( describers->car->car ) )
		{
			cell * tuple = assq( null, describers->car->cdr, env );
			describer p = (void *) tuple->cdr->address;
			return p( null, env, exp );
		}
		describers = describers->cdr;
	}
	quit( 11 );
	return null;
}

static cell * link_callees( cell * null, cell * exp, cell * env )
{
	printf( "link_callees:\n" );
	if( exp->address >= null->arena )
	{
		unsigned long words = ( exp->header >> 24 ) - 2;
		unsigned long bytes = words * WORD_SIZE;
		for( unsigned long i = 0; i < bytes - 12; )
		{
			unsigned char * b = exp->bytes + i;
			if( ( *( b + 00 ) == 0x49 ) && ( *( b + 01 ) == 0xba ) &&				// movabs R10
			    ( *( b + 03 ) == 0x00 ) && ( *( b + 04 ) == 0x00 ) &&				// unlinked
			    ( *( b + 10 ) == 0x41 ) && ( *( b + 11 ) == 0xff ) && ( *( b + 12 ) == 0xd2 ) )	// call R10
			{
				printf( "[0x%02x → ", (unsigned char) *( b + 2 ) );
				cell * tuple = assq( null, symbol( null, *( b + 2 ) ), env );
				if( tuple != null )
				{
					printf( "%p] ", tuple->cdr->address );
					*( (void **) ( b + 2 ) ) = (void *) tuple->cdr->address;
				}
				else
				{
					printf( "FAIL ]" );
					quit( 6 );
				}
			}
			printf( "%02x ", *b );
			i++;
		}
		printf( "\n" );
	}
	return exp;
}

static cell * link_callers( cell * null, cell * key, cell * exp, cell * env )
{
	unsigned long val = symbol_value( key );
	printf( "link_callers: to '%c' (%016lx) at %p\n", (int) val, val, exp->address );
	cell * existing = assq( null, key, env );
	if( ( existing != null ) && ( equals( null, key, existing->car ) is_true ) )
	{
		unsigned long ext = symbol_value( existing->car );
		printf( "link_callers: existing '%c' (%016lx) at %p\n", (int) ext, ext, existing->cdr->address );
		while( env != null )
		{
			cell * tuple = env->car;
			cell * code  = tuple->cdr;
			if( ( operator_type( code ) == CELL_PROCEDURE ) || ( operator_type( code ) == CELL_FUNCTION ) )
			{
				unsigned long sym = symbol_value( tuple->car );
				unsigned char * b = code->address;
				while( *b != 0xc3 ) // ret
				{
					if( ( ( *b == 0x48 ) || ( *b == 0x49 ) ) && ( *( b + 10 ) == 0xff ) )		// movabsi, call   
					{
						void * p =  *( (void **) ( b + 2 ) );
						if( p == existing->cdr->address )
						{
							printf( "link_callers: re-linking '%c' (%016lx) %p → %p\n", (int) sym, sym, p, exp->address );
							toggle_writable( code->address, 1 );
							*( (void **) ( b + 2 ) ) = (void *) exp->address;
							toggle_writable( code->address, 0 );
						}
					}
					b++;
				}
			}
			env = env->cdr;
		}
	}
	return exp;
}

static cell * print_integer( cell * null, cell * exp, cell * env )
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

static cell * print( cell * null, cell * exp, cell * env );
static cell * print_list( cell * null, cell * lst, cell * env )
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
			print( null, c->car, env );

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
					print( null, c->car->car, env );
					put_char( ' ' );
				}
				else
				{
					print( null, c->car, env );
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

// printers…
typedef cell * (* printer)(cell *, cell *, cell *);

static cell * all_printers;

static cell * p_print_null( cell * null, cell * exp, cell * env )
{
	put_char( '(' ); put_char( ')' );
	return exp;
}
static cell * p_print_tuple( cell * null, cell * exp, cell * env )
{
	print_list( null, exp, env );
	return exp;
}
static cell * p_print_symbol( cell * null, cell * exp, cell * env )
{
	put_char( symbol_value( exp ) );
	return exp;
}
static cell * p_print_integer( cell * null, cell * exp, cell * env )
{
	print_integer( null, exp, env );
	return exp;
}
static cell * p_print_procedure( cell * null, cell * exp, cell * env )
{
	unsigned char p = ( exp->address < null->arena ) ? 'P' : 'p';
	put_char( p ); put_char( '0' + integer_value( exp ) );
	return exp;
}
static cell * p_print_function( cell * null, cell * exp, cell * env )
{
	unsigned char f = ( exp->address < null->arena ) ? 'F' : 'f';
	put_char( f ); put_char( '0' + integer_value( exp ) );
	return exp;
}
static cell * p_print_lambda( cell * null, cell * exp, cell * env )
{
	print( null, exp->operation, env );
	return exp;
}
static cell * p_print_fexpr( cell * null, cell * exp, cell * env )
{
	print( null, exp->operation, env );
	return exp;
}

static cell * print( cell * null, cell * exp, cell * env )
{
	unsigned long type = cell_type( exp );
	cell * printers = all_printers;
	while( printers != null )
	{
		if( type == integer_value( printers->car->car ) )
		{
			cell * tuple = assq( null, printers->car->cdr, env );
			printer p = (void *) tuple->cdr->address;
			return p( null, exp, env );
		}
		printers = printers->cdr;
	}
	quit( 10 );
	return null;
}

static cell * read_integer( cell * null, cell * env )
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

static cell * read( cell * null, cell * env );
static cell * read_list( cell * null, cell * env, cell * lst )
{
	cell * c = read( null, env );
	if( ( cell_type( c ) == CELL_SYMBOL ) && ( symbol_value( c ) == ')' ) )
	{
		return reverse( null, lst );
	}
	else
	{
		return read_list( null, env, cons( null, c, lst ) );
	}
}

// readers…
typedef int    (* tester)(cell *, unsigned long);
typedef cell * (* reader)(cell *, cell *, unsigned long);

static cell * all_readers;

int r_test_comment( cell * null, unsigned long c )
{
	return c == ';';
}
cell * r_read_comment( cell * null, cell * env, unsigned long c )
{
	while( c != '\n' ) c = get_char( );
	return read( null, env );
}
int r_test_whitespace( cell * null, unsigned long c )
{
	return ( c == ' ' ) || ( c == '\t' ) || ( c == '\n' );
}
cell * r_read_whitespace( cell * null, cell * env, unsigned long c )
{
	return read( null, env );
}
int r_test_hexadecimal( cell * null, unsigned long c )
{
	if( c == '0' ) // 0x.. ?
	{
		unsigned long d = get_char( );
		if( d == 'x' )
		{
			return 1;
		}
		quit( 4 );
	}
	return 0;
}
static cell * r_read_hexadecimal( cell * null, cell * env, unsigned long x )
{
	return read_integer( null, env );
}
int r_test_decimal( cell * null, unsigned long c )
{
	if( ( c >= '0' ) && ( c <= '9' ) ) // 0 - 9 sans 0x prefix
	{
		quit( 4 );
	}
	return 0;
}
static cell * r_read_decimal( cell * null, cell * env, unsigned long x )
{
	quit( 4 );
	return null;
}
int r_test_list( cell * null, unsigned long c )
{
	return c == '(';
}
static cell * r_read_list( cell * null, cell * env, unsigned long c )
{
	return read_list( null, env, null );
}
int r_test_symbol( cell * null, unsigned long c )
{
	return 1;
}
static cell * r_read_symbol( cell * null, cell * env, unsigned long c )
{
	return symbol( null, c );
}

static cell * read( cell * null, cell * env )
{
	unsigned long c = get_char( );

	cell * readers = all_readers;
	while( readers != null )
	{
		cell * tuple = assq( null, readers->car->car, env );
		tester t = (void *) tuple->cdr->address;
		if( t( null, c ) )
		{
			tuple = assq( null, readers->car->cdr, env );
			reader r = (void *) tuple->cdr->address;
			return r( null, env, c );
		}
		readers = readers->cdr;
	}
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
	switch( operator_type( op ) )
	{
		case CELL_PROCEDURE:
			quit( 9 );
			break;
		case CELL_FUNCTION:
		{
			int evaluated = 0;
			if( op->address == eval )
			{
				evaluated = 1;
				ans = eval( null, args->car, env );
			}
			else if( op->address == eval_list )
			{
				evaluated = 1;
			}
			else if( op->address == apply )
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
				cell * (* proc)(cell *) = (void *) op->address;
				ans = proc( null );
				break;
			}
			case 1:
			{
				cell * (* proc)(cell *, cell *) = (void *) op->address;
				ans = proc( null, args->car );
				break;
			}
			case 2:
			{
				cell * (* proc)(cell *, cell *, cell *) = (void *) op->address;
				ans = proc( null, args->car, args->cdr->car );
				break;
			}
			case 3:
			{
				cell * (* proc)(cell *, cell *, cell *, cell *) = (void *) op->address;
				ans = proc( null, args->car, args->cdr->car, args->cdr->cdr->car );
				break;
			}
			case 4:
			{
				cell * (* proc)(cell *, cell *, cell *, cell *, cell *) = (void *) op->address;
				ans = proc( null, args->car, args->cdr->car, args->cdr->cdr->car, args->cdr->cdr->cdr->car );
				break;
			}
			default:
				quit( 8 );
			}
			return evaluated ? ans : cons( null, ans, env );	// any set! will be lost on return
		}
		case CELL_LAMBDA:
		{
			ans  = eval_list( null, args, env );
			args = ans->car; env = ans->cdr;

			cell * fmls = op->operation->cdr->car;
			cell * body = op->operation->cdr->cdr;

			ans = apply_forms( null, fmls, args, body, env );
			return cons( null, ans->car, env );			// any set! will be lost on return
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

// evaluators…
typedef cell * (* evaluator)(cell *, cell *, cell *);

static cell * all_evaluators;

static cell * e_eval_null( cell * null, cell * env, cell * exp )
{
	return cons( null, null, env );
}
static cell * e_eval_tuple( cell * null, cell * env, cell * exp )
{
	cell * first = exp->car;
	if( is_symbol( null, first ) is_true )
	{
		switch( symbol_value( first ) )
		{
		case '!':
		{
			cell * key, * ans, * val;
			key = exp->cdr->car;
			if( cell_type( key ) != CELL_SYMBOL )
			{
				ans = eval( null, key, env );
				key = ans->car;
				env = ans->cdr;
				if( cell_type( key ) != CELL_SYMBOL ) quit( 5 );
			}
			ans = eval( null, exp->cdr->cdr->car, env );
			val = ans->car;
			env = ans->cdr;

			if( ( operator_type( val ) == CELL_PROCEDURE ) || ( operator_type( val ) == CELL_FUNCTION ) )
			{
				link_callers( null, key, val, env );
			}

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
			return cons( null, expression( null, CELL_LAMBDA, exp ), env );
		}
		case '$':
		{
			return cons( null, expression( null, CELL_FEXPR, exp ), env );
		}
		case '@':
		{
			cell * ans = eval( null, exp->cdr->car, env );
			unsigned long type = integer_value( ans->car );

			ans = eval( null, exp->cdr->cdr->car, env );
			unsigned long nargs = integer_value( ans->car );

			ans = eval_list( null, exp->cdr->cdr->cdr, env );
			cell * bytes = ans->car;

			if( ( type == CELL_PROCEDURE ) || ( type == CELL_FUNCTION ) )
			{
				ans = code( null, type, nargs, 0, bytes );
				ans = link_callees( null, ans, env );
				return cons( null, ans, env );
			}
			quit( 5 );
		}
		}
	}
	// else
	cell * ans  = eval( null, first, env );
	return apply( null, ans->car, exp->cdr, ans->cdr );
}
static cell * e_eval_symbol( cell * null, cell * env, cell * exp )
{
	cell * tuple = assq( null, exp, env );
	cell * value = ( is_tuple( null, tuple ) is_true ) ? tuple->cdr : null;
	return cons( null, value, env );
}
static cell * e_eval_integer( cell * null, cell * env, cell * exp )
{
	return cons( null, exp, env );
	return exp;
}

static cell * eval( cell * null, cell * exp, cell * env )
{
	unsigned long type = cell_type( exp );
	cell * evaluators = all_evaluators;
	while( evaluators != null )
	{
		if( type == integer_value( evaluators->car->car ) )
		{
			cell * tuple = assq( null, evaluators->car->cdr, env );
			evaluator e = (void *) tuple->cdr->address;
			return e( null, env, exp );
		}
		evaluators = evaluators->cdr;
	}
	quit( 5 );
	return null;
}

static cell * repl( cell * null, cell * env )
{
	put_char( '>' ); put_char( ' ' );
	cell * ans = eval( null, read( null, env ), env );

	print( null, ans->car, env ); put_char( '\n' );

	return repl( null, ans->cdr );
}

int main( )
{
	cell * null = sire( NUM_PAGES );
	cell * env  = null;

	// functions…
	env = cons( null, cons( null, symbol( null, '#'  ), code( null, CELL_FUNCTION,  1, car           , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '%'  ), code( null, CELL_FUNCTION,  1, cdr           , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '_'  ), code( null, CELL_FUNCTION,  1, is_null       , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 't'  ), code( null, CELL_FUNCTION,  1, is_tuple      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 's'  ), code( null, CELL_FUNCTION,  1, is_symbol     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'i'  ), code( null, CELL_FUNCTION,  1, is_integer    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '.'  ), code( null, CELL_FUNCTION,  2, cons          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '='  ), code( null, CELL_FUNCTION,  2, equals        , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'q'  ), code( null, CELL_FUNCTION,  2, assq          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '\\' ), code( null, CELL_FUNCTION,  1, reverse       , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'z'  ), code( null, CELL_FUNCTION,  1, length        , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa0 ), code( null, CELL_FUNCTION,  2, print_integer , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'p'  ), code( null, CELL_FUNCTION,  2, print         , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa1 ), code( null, CELL_FUNCTION,  1, read_integer  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa2 ), code( null, CELL_FUNCTION,  2, read_list     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'r'  ), code( null, CELL_FUNCTION,  1, read          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa3 ), code( null, CELL_FUNCTION,  4, apply_forms   , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '`'  ), code( null, CELL_FUNCTION,  3, apply         , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'o'  ), code( null, CELL_FUNCTION,  2, eval_list     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'e'  ), code( null, CELL_FUNCTION,  2, eval          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'd'  ), code( null, CELL_FUNCTION,  2, describe      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa4 ), code( null, CELL_FUNCTION,  2, link_callees  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa5 ), code( null, CELL_FUNCTION,  2, link_callers  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xa6 ), code( null, CELL_FUNCTION,  1, repl          , 0 ) ), env );

	// procedures…
	env = cons( null, cons( null, symbol( null, 0xff ), code( null, CELL_PROCEDURE, 1, quit          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfe ), code( null, CELL_PROCEDURE, 1, cell_type     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfd ), code( null, CELL_PROCEDURE, 1, operator_type , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfc ), code( null, CELL_PROCEDURE, 1, symbol_value  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfb ), code( null, CELL_PROCEDURE, 1, integer_value , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfa ), code( null, CELL_PROCEDURE, 0, get_char      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf9 ), code( null, CELL_PROCEDURE, 1, put_char      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf8 ), code( null, CELL_PROCEDURE, 2, allocate      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf7 ), code( null, CELL_PROCEDURE, 0, sire          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf6 ), code( null, CELL_PROCEDURE, 2, symbol        , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf5 ), code( null, CELL_PROCEDURE, 2, integer       , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf4 ), code( null, CELL_PROCEDURE, 5, code          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf3 ), code( null, CELL_PROCEDURE, 3, expression    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf2 ), code( null, CELL_PROCEDURE, 0, main          , 0 ) ), env );

	// readers…
	env = cons( null, cons( null, symbol( null, 0xef ), code( null, CELL_PROCEDURE, 2, r_test_comment    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xee ), code( null, CELL_PROCEDURE, 3, r_read_comment    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xed ), code( null, CELL_PROCEDURE, 2, r_test_whitespace , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xec ), code( null, CELL_PROCEDURE, 3, r_read_whitespace , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xeb ), code( null, CELL_PROCEDURE, 2, r_test_hexadecimal, 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xea ), code( null, CELL_PROCEDURE, 3, r_read_hexadecimal, 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe9 ), code( null, CELL_PROCEDURE, 2, r_test_decimal,     0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe8 ), code( null, CELL_PROCEDURE, 3, r_read_decimal,     0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe7 ), code( null, CELL_PROCEDURE, 2, r_test_list,        0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe6 ), code( null, CELL_PROCEDURE, 3, r_read_list,        0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe5 ), code( null, CELL_PROCEDURE, 2, r_test_symbol,      0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xe4 ), code( null, CELL_PROCEDURE, 3, r_read_symbol,      0 ) ), env );

	cell * r_comment     = cons( null, symbol( null, 0xef ), symbol( null, 0xee ) );
	cell * r_whitespace  = cons( null, symbol( null, 0xed ), symbol( null, 0xec ) );
	cell * r_hexadecimal = cons( null, symbol( null, 0xeb ), symbol( null, 0xea ) );
	cell * r_decimal     = cons( null, symbol( null, 0xe9 ), symbol( null, 0xe8 ) );
	cell * r_list        = cons( null, symbol( null, 0xe7 ), symbol( null, 0xe6 ) );
	cell * r_symbol      = cons( null, symbol( null, 0xe5 ), symbol( null, 0xe4 ) );

	all_readers = null;
	all_readers = cons( null, r_symbol,      all_readers );
	all_readers = cons( null, r_list,        all_readers );
	all_readers = cons( null, r_decimal,     all_readers );
	all_readers = cons( null, r_hexadecimal, all_readers );
	all_readers = cons( null, r_whitespace,  all_readers );
	all_readers = cons( null, r_comment,     all_readers );

	// printers…
	env = cons( null, cons( null, symbol( null, 0xdf ), code( null, CELL_PROCEDURE, 3, p_print_null	     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xde ), code( null, CELL_PROCEDURE, 3, p_print_tuple     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xdd ), code( null, CELL_PROCEDURE, 3, p_print_symbol    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xdc ), code( null, CELL_PROCEDURE, 3, p_print_integer   , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xdb ), code( null, CELL_PROCEDURE, 3, p_print_procedure , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xda ), code( null, CELL_PROCEDURE, 3, p_print_function  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xd9 ), code( null, CELL_PROCEDURE, 3, p_print_lambda    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xd8 ), code( null, CELL_PROCEDURE, 3, p_print_fexpr     , 0 ) ), env );

	cell * p_null	   = cons( null, integer( null, CELL_NULL      ), symbol( null, 0xdf ) );
	cell * p_tuple	   = cons( null, integer( null, CELL_TUPLE     ), symbol( null, 0xde ) );
	cell * p_symbol	   = cons( null, integer( null, CELL_SYMBOL    ), symbol( null, 0xdd ) );
	cell * p_integer   = cons( null, integer( null, CELL_INTEGER   ), symbol( null, 0xdc ) );
	cell * p_procedure = cons( null, integer( null, CELL_PROCEDURE ), symbol( null, 0xdb ) );
	cell * p_function  = cons( null, integer( null, CELL_FUNCTION  ), symbol( null, 0xda ) );
	cell * p_lambda	   = cons( null, integer( null, CELL_LAMBDA    ), symbol( null, 0xd9 ) );
	cell * p_fexpr     = cons( null, integer( null, CELL_FEXPR     ), symbol( null, 0xd8 ) );

	all_printers = null;
	all_printers = cons( null, p_fexpr,     all_printers );
	all_printers = cons( null, p_lambda,    all_printers );
	all_printers = cons( null, p_function,  all_printers );
	all_printers = cons( null, p_procedure, all_printers );
	all_printers = cons( null, p_integer,   all_printers );
	all_printers = cons( null, p_symbol,    all_printers );
	all_printers = cons( null, p_tuple,     all_printers );
	all_printers = cons( null, p_null,      all_printers );

	// describers…
	env = cons( null, cons( null, symbol( null, 0xcf ), code( null, CELL_PROCEDURE, 3, d_describe_null      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xce ), code( null, CELL_PROCEDURE, 3, d_describe_tuple     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xcd ), code( null, CELL_PROCEDURE, 3, d_describe_symbol    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xcc ), code( null, CELL_PROCEDURE, 3, d_describe_integer   , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xcb ), code( null, CELL_PROCEDURE, 3, d_describe_code      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xca ), code( null, CELL_PROCEDURE, 3, d_describe_code      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xc9 ), code( null, CELL_PROCEDURE, 3, d_describe_lambda    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xc8 ), code( null, CELL_PROCEDURE, 3, d_describe_fexpr     , 0 ) ), env );

	cell * d_null	   = cons( null, integer( null, CELL_NULL      ), symbol( null, 0xcf ) );
	cell * d_tuple	   = cons( null, integer( null, CELL_TUPLE     ), symbol( null, 0xce ) );
	cell * d_symbol	   = cons( null, integer( null, CELL_SYMBOL    ), symbol( null, 0xcd ) );
	cell * d_integer   = cons( null, integer( null, CELL_INTEGER   ), symbol( null, 0xcc ) );
	cell * d_procedure = cons( null, integer( null, CELL_PROCEDURE ), symbol( null, 0xcb ) );
	cell * d_function  = cons( null, integer( null, CELL_FUNCTION  ), symbol( null, 0xca ) );
	cell * d_lambda	   = cons( null, integer( null, CELL_LAMBDA    ), symbol( null, 0xc9 ) );
	cell * d_fexpr     = cons( null, integer( null, CELL_FEXPR     ), symbol( null, 0xc8 ) );

	all_describers = null;
	all_describers = cons( null, d_fexpr,     all_describers );
	all_describers = cons( null, d_lambda,    all_describers );
	all_describers = cons( null, d_function,  all_describers );
	all_describers = cons( null, d_procedure, all_describers );
	all_describers = cons( null, d_integer,   all_describers );
	all_describers = cons( null, d_symbol,    all_describers );
	all_describers = cons( null, d_tuple,     all_describers );
	all_describers = cons( null, d_null,      all_describers );

	// evaluators…
	env = cons( null, cons( null, symbol( null, 0xbf ), code( null, CELL_PROCEDURE, 3, e_eval_null    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xbe ), code( null, CELL_PROCEDURE, 3, e_eval_tuple   , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xbd ), code( null, CELL_PROCEDURE, 3, e_eval_symbol  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xbc ), code( null, CELL_PROCEDURE, 3, e_eval_integer , 0 ) ), env );

	cell * e_null	   = cons( null, integer( null, CELL_NULL    ), symbol( null, 0xbf ) );
	cell * e_tuple	   = cons( null, integer( null, CELL_TUPLE   ), symbol( null, 0xbe ) );
	cell * e_symbol	   = cons( null, integer( null, CELL_SYMBOL  ), symbol( null, 0xbd ) );
	cell * e_integer   = cons( null, integer( null, CELL_INTEGER ), symbol( null, 0xbc ) );

	all_evaluators = null;
	all_evaluators = cons( null, e_integer, all_evaluators );
	all_evaluators = cons( null, e_symbol,  all_evaluators );
	all_evaluators = cons( null, e_tuple,   all_evaluators );
	all_evaluators = cons( null, e_null,    all_evaluators );


	repl( null, env );
	return 0;
}
