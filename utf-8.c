#include <stdio.h>

static unsigned long get_char( )
{
	unsigned char c = fgetc( stdin );
	short d, n = 0;
	do d = ( c << ++n ) & 0x0100; while( d );
	n = ( n < 2 ) ? 0 : n - 2;

	char m = 1;
	unsigned long e = c;
	while( n-- > 0 ) e = e + ( fgetc( stdin ) << ( 8 * m++ ) );
	return e;
}

static unsigned long put_char( unsigned long c )
{
	return fputs( (const char *) &c, stdout );
}

int main( )
{
	unsigned long c = get_char( );
	put_char( c );

	return 0;
}
