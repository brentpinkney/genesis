//
// Pre-history 5. Add special form @ for user-defined, machine-code function calls (@) from the REPL.
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define PAGE_SIZE		4096
#define NUM_PAGES		1024
#define WORD_SIZE		8
#define CELL_NULL		0x01
#define CELL_TUPLE		0x02
#define CELL_SYMBOL		0x03
#define CELL_INTEGER		0x04
#define CELL_OPERATOR		0x05	// 0000 0011
#define CELL_PROCEDURE		0x05	// 0000 0011 machine language, REPL access
#define CELL_FUNCTION		0x25	// 0010 0011 machine language, no REPL access
#define CELL_LAMBDA		0x15	// 0001 0011 operative s-expressions
#define CELL_FEXPR		0x35	// 0011 0011 applicative s-expressions
#define MASK_TYPE		0x07
#define MASK_SYMBOL		0xff00
#define MASK_INTEGER		0xff00
#define MASK_INTEGER_HI		0xf000
#define MASK_INTEGER_LO		0x0f00
#define MASK_OPERATOR		0x00f5

int verbose = 1;
#define dprintf( ... ) if( verbose ) fprintf( stdout,  __VA_ARGS__ )
#define cell_type( c )     ( c->header & MASK_TYPE )
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
		struct { void * address; unsigned char bytes[]; };
	};
};

// procedures…
static void quit( unsigned long n ) { exit( n ); }

static unsigned char get_char( ) { return fgetc( stdin ); }

static unsigned char put_char( unsigned char c ) { return fputc( c, stdout ); }

static cell * allocate( cell * null, unsigned long words )
{
	cell * this = null->next;
	null->next = ( (void *) null->next ) + ( words * WORD_SIZE );
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
	null->next   = arena + ( 4 * WORD_SIZE );

	cell * size  = allocate( null, 1 );		// make the size integer (useful as 'not null')
	size->header = ( bytes << 8 ) + CELL_INTEGER;
	null->size   = size;
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

static cell * length( cell * null, cell * lst );
static cell * code( cell * null, unsigned char type, unsigned long nargs, void * address, cell * bytes )
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
	f->header = ( words << 16 ) + ( ( nargs << 8 ) + type );
	return f;
}

static cell * expression( cell * null, unsigned char type, cell * exp )
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
	if( mprotect( address, PAGE_SIZE, PROT_READ | PROT_EXEC | flag ) == -1 )
	{
		quit( 1 );
	}
}

// functions…
static cell * car( cell * null, cell * c ) { return c->car; }

static cell * cdr( cell * null, cell * c ) { return c->cdr; }

static cell * is_null( cell * null, cell * c )  { return ( cell_type( c ) == CELL_NULL )  ? null->size : null; }

static cell * is_tuple( cell * null, cell * c ) { return ( cell_type( c ) == CELL_TUPLE ) ? null->size : null; }

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
	return ( ( is_tuple( null, a ) is_true ) || ( is_tuple( null, b ) is_true ) )
		? ( a == b ) ? null->size : null
		: ( a->header == b->header ) ? null->size : null;
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

static cell * describe( cell * null, cell * exp )
{
	printf( "address: %16p\theader: %016lx\t", exp, exp->header );
	switch( cell_type( exp ) )
	{
		case CELL_NULL:
		{
			unsigned long size = exp->size->header >> 8;
			printf( "null\nsize:      0x%08lx\t\tarena:%16p\t\tnext:%16p", size, exp->arena, exp->next );
			printf( "\tused: %.2f%%\n", ( exp->next - exp->arena ) * 100.0 / size );
			break;
		}
		case CELL_TUPLE:
		{
			printf( "tuple\ncar:     %16p\tcdr:  %16p\n", exp->car, exp->cdr );
			break;
		}
		case CELL_SYMBOL:
		{
			unsigned char value = symbol_value( exp );
			printf( "symbol\ncharacter: '%c'\t\t\tvalue:  0x%02x (%d)\n", value, value, value );
			break;
		}
		case CELL_INTEGER:
		{
			unsigned long value = integer_value( exp );
			printf( "integer\nvalue:     0x%02lx (%ld)\n", value, value );
			break;
		}
		case CELL_OPERATOR:
			switch( operator_type( exp ) )
			{
				case CELL_PROCEDURE:
				case CELL_FUNCTION:
				{
					unsigned long words = ( exp->header >> 16 ) - 2;
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
					break;
				}
				case CELL_LAMBDA:
					printf( "lambda\noperation: %p\n", exp->operation );
					break;
				case CELL_FEXPR:
					printf( "fexpr\noperation: %p\n", exp->operation );
					break;
			}
			break;
	}
	return exp;
}

static cell * link_callees( cell * null, cell * exp, cell * env )
{
	dprintf( "link_callees:\n" );
	if( exp->address >= null->arena )
	{
		unsigned long words = ( exp->header >> 16 ) - 2;
		unsigned long bytes = words * WORD_SIZE;
		for( unsigned long i = 0; i < bytes; )
		{
			unsigned char * b = exp->bytes + i;
			if( ( *( b + 00 ) == 0x49 ) && ( *( b + 01 ) == 0xba ) &&				// movabs R10
			    ( *( b + 03 ) == 0x00 ) && ( *( b + 04 ) == 0x00 ) &&				// unlinked
			    ( *( b + 10 ) == 0x41 ) && ( *( b + 11 ) == 0xff ) && ( *( b + 12 ) == 0xd2 ) )	// call R10
			{
				dprintf( "[0x%02x → ", *( b + 2) );
				cell * tuple = assq( null, symbol( null, *( b + 2 ) ), env );
				if( tuple != null )
				{
					dprintf( "%p] ", tuple->cdr->address );
					memcpy( b + 2, &( tuple->cdr->address ), WORD_SIZE );
				}
				else
				{
					dprintf( "FAIL ]" );
					quit( 5 );
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
	dprintf( "link_callers: to '%c' at %p\n", (int) symbol_value( key ), exp->address );
	cell * existing = assq( null, key, env );
	if( existing != null )
	{
		dprintf( "link_callers: existing at %p\n", existing->cdr->address );
		while( env != null )
		{
			cell * tuple = env->car;
			cell * code  = tuple->cdr;
			if( ( cell_type( code ) == CELL_PROCEDURE ) || ( cell_type( code ) == CELL_FUNCTION ) )
			{
				unsigned char * b = code->address;
				while( *b != 0xc3 ) // ret
				{
					if( ( ( *( b + 00 ) == 0x48 ) && ( *( b + 01 ) == 0xb8 ) &&	// movabs rax,…
					      ( *( b + 10 ) == 0xff ) && ( *( b + 11 ) == 0xd0 ) )	// call   rax
					 || ( ( *( b + 00 ) == 0x49 ) && ( *( b + 01 ) == 0xba ) &&	// movabs r10,call R10
					      ( *( b + 10 ) == 0x41 ) && ( *( b + 11 ) == 0xff ) && ( *( b + 12 ) == 0xd2 ) ) )
					{
						void * p;
						memcpy( &p, b + 2, WORD_SIZE );
						if( p == existing->cdr->address )
						{
							unsigned long s = symbol_value( tuple->car );
							dprintf( "link_callers: re-linking '%c' (%ld) %p → %p\n",
								(int) s, s,
								p, exp->address );
							toggle_writable( code->address, 1 );
							memcpy( b + 2, &( exp->address ), WORD_SIZE );
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
				{
					unsigned char p = ( exp->address < null->arena ) ? 'P' : 'p';
					put_char( p ); put_char( '0' + integer_value( exp ) );
					break;
				}
				case CELL_FUNCTION:
				{
					unsigned char f = ( exp->address < null->arena ) ? 'F' : 'f';
					put_char( f ); put_char( '0' + integer_value( exp ) );
					break;
				}
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
			quit( 3 );
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
	if( ( c >= '!' ) && ( c <= '~' ) ) // printable
	{
		return symbol( null, c );
	}
	
	quit( 2 );
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
			quit( 8 );
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
				quit( 7 );
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
	quit( 6 );
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

					if( ( cell_type( val ) == CELL_PROCEDURE ) || ( cell_type( val ) == CELL_FUNCTION ) )
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
					quit( 4 );
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
	quit( 4 );
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
	env = cons( null, cons( null, symbol( null, 0x00 ), code( null, CELL_FUNCTION,  1, print_integer , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'p'  ), code( null, CELL_FUNCTION,  1, print         , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x01 ), code( null, CELL_FUNCTION,  1, read_integer  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x02 ), code( null, CELL_FUNCTION,  1, read_list     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'r'  ), code( null, CELL_FUNCTION,  0, read          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x03 ), code( null, CELL_FUNCTION,  4, apply_forms   , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, '`'  ), code( null, CELL_FUNCTION,  3, apply         , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'o'  ), code( null, CELL_FUNCTION,  2, eval_list     , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'e'  ), code( null, CELL_FUNCTION,  1, eval          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 'd'  ), code( null, CELL_FUNCTION,  1, describe      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x04 ), code( null, CELL_FUNCTION,  2, link_callees  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x05 ), code( null, CELL_FUNCTION,  2, link_callers  , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0x06 ), code( null, CELL_FUNCTION,  1, repl          , 0 ) ), env );

	// procedures…
	env = cons( null, cons( null, symbol( null, 0xff ), code( null, CELL_PROCEDURE, 1, quit          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfe ), code( null, CELL_PROCEDURE, 2, allocate      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfd ), code( null, CELL_PROCEDURE, 0, sire          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfc ), code( null, CELL_PROCEDURE, 2, symbol        , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfb ), code( null, CELL_PROCEDURE, 2, integer       , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xfa ), code( null, CELL_PROCEDURE, 5, code          , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf9 ), code( null, CELL_PROCEDURE, 3, expression    , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf8 ), code( null, CELL_PROCEDURE, 0, get_char      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf7 ), code( null, CELL_PROCEDURE, 1, put_char      , 0 ) ), env );
	env = cons( null, cons( null, symbol( null, 0xf6 ), code( null, CELL_PROCEDURE, 0, main          , 0 ) ), env );

	repl( null, env );
	return 0;
}
