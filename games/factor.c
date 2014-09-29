#ifndef lint
#include <sys/cdefs.h>
#ifdef __COPYRIGHT
__COPYRIGHT("@(#) Copyright (c) 1989, 1993\
	The Regents of the University of California.  All rights reserved.");
#endif
#ifdef __SCCSID
__SCCSID("@(#)factor.c	8.4 (Berkeley) 5/4/95");
#endif
#ifdef __RCSID
__RCSID("$NetBSD: factor.c,v 1.19 2009/08/12 05:54:31 dholland Exp $");
#endif
#ifdef __FBSDID
__FBSDID("$FreeBSD$");
#endif
#endif 
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "primes.h"
#ifdef HAVE_OPENSSL
#include <openssl/bn.h>
#define	PRIME_CHECKS	5
static void pollard_pminus1(BIGNUM *);
#else
typedef ubig BIGNUM;
typedef u_long BN_ULONG;
#define BN_CTX			int
#define BN_CTX_new()		NULL
#define BN_new()		((BIGNUM *)calloc(sizeof(BIGNUM), 1))
#define BN_is_zero(v)		(*(v) == 0)
#define BN_is_one(v)		(*(v) == 1)
#define BN_mod_word(a, b)	(*(a) % (b))
static int BN_dec2bn( BIGNUM **a, const char *str );
static int BN_hex2bn( BIGNUM **a, const char *str );
static BN_ULONG BN_div_word( BIGNUM *, BN_ULONG );
static void BN_print_fp( FILE *, const BIGNUM * );
#endif
static void BN_print_dec_fp( FILE *, const BIGNUM * );
static void pr_fact( BIGNUM * );
static void pr_print( BIGNUM * );
static void usage( void );
static BN_CTX *ctx;
static int hflag;
int main( int argc, char *argv[] ) {
	BIGNUM *val;
	int ch;
	char *p, buf[LINE_MAX];
	ctx = BN_CTX_new();
	val = BN_new();
	if ( val == NULL ) errx( 1, "can't initialise bignum" );
	while ( ( ch = getopt( argc, argv, "h" ) ) != -1 )
		switch ( ch ) {
			case 'h':
				hflag++;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc == 0 ) for ( ;; ) {
		if ( fgets( buf, sizeof( buf ), stdin ) == NULL ) {
			if ( ferror (stdin) ) err( 1, "stdin" );
			exit( 0 );
		}
		for ( p = buf; isblank( *p ); ++p )
			;
		if ( *p == '\n' || *p == '\0' ) continue;
		if ( *p == '-' ) errx( 1, "negative numbers aren't permitted." );
		if ( BN_dec2bn( &val, buf ) == 0 && BN_hex2bn( &val, buf ) == 0 ) errx( 1, "%s: illegal numeric format.", buf );
		pr_fact( val );
	}
	else for ( ; *argv != NULL; ++argv ) {
		if ( argv[0][0] == '-' ) errx( 1, "negative numbers aren't permitted." );
		if ( BN_dec2bn( &val, argv[0] ) == 0 && BN_hex2bn( &val, argv[0] ) == 0 ) errx( 1, "%s: illegal numeric format.", argv[0] );
		pr_fact( val );
	}
	exit( 0 );
}
static void pr_fact( BIGNUM *val ) {
	const ubig *fact;
	if ( BN_is_zero( val ) ) exit( 0 );
	if ( BN_is_one( val ) ) {
		printf( "1: 1\n" );
		return;
	}
	if ( hflag ) {
		fputs( "0x", stdout );
		BN_print_fp( stdout, val );
	} else BN_print_dec_fp( stdout, val );
	putchar( ':' );
	for ( fact = &prime[0]; !BN_is_one( val ); ++fact ) {
		do {
			if ( BN_mod_word(val, (BN_ULONG)*fact) == 0 ) break;
		} while ( ++fact <= pr_limit );
		if ( fact > pr_limit ) {
#ifdef HAVE_OPENSSL
			BIGNUM *bnfact;
			bnfact = BN_new();
			BN_set_word(bnfact, *(fact - 1));
			if (!BN_sqr(bnfact, bnfact, ctx))
			errx(1, "error in BN_sqr()");
			if (BN_cmp(bnfact, val) > 0 ||
					BN_is_prime(val, PRIME_CHECKS,
							NULL, NULL, NULL) == 1)
			pr_print(val);
			else
			pollard_pminus1(val);
#else
			pr_print( val );
#endif
			break;
		}
		do {
			printf( hflag ? " 0x%lx" : " %lu", *fact );
			BN_div_word( val, (BN_ULONG) *fact );
		} while ( BN_mod_word(val, (BN_ULONG)*fact) == 0 );
		fflush (stdout);
	}
	putchar( '\n' );
}
static void pr_print( BIGNUM *val ) {
	if ( hflag ) {
		fputs( " 0x", stdout );
		BN_print_fp( stdout, val );
	} else {
		putchar( ' ' );
		BN_print_dec_fp( stdout, val );
	}
}
static void usage( void ) {
	fprintf( stderr, "usage: factor [-h] [value ...]\n" );
	exit( 1 );
}
#ifdef HAVE_OPENSSL
static void
pollard_pminus1(BIGNUM *val)
{	
	BIGNUM *base, *rbase, *num, *i, *x;
	base = BN_new();
	rbase = BN_new();
	num = BN_new();
	i = BN_new();
	x = BN_new();
	BN_set_word(rbase, 1);
	newbase:
	if (!BN_add_word(rbase, 1))
	errx(1, "error in BN_add_word()");
	BN_set_word(i, 2);
	BN_copy(base, rbase);
	for (;;) {
		BN_mod_exp(base, base, i, val, ctx);
		if (BN_is_one(base))
		goto newbase;
		BN_copy(x, base);
		BN_sub_word(x, 1);
		if (!BN_gcd(x, x, val, ctx))
		errx(1, "error in BN_gcd()");
		if (!BN_is_one(x)) {
			if (BN_is_prime(x, PRIME_CHECKS, NULL, NULL,
							NULL) == 1)
			pr_print(x);
			else
			pollard_pminus1(x);
			fflush(stdout);
			BN_div(num, NULL, val, x, ctx);
			if (BN_is_one(num))
			return;
			if (BN_is_prime(num, PRIME_CHECKS, NULL, NULL,
							NULL) == 1) {
				pr_print(num);
				fflush(stdout);
				return;
			}
			BN_copy(val, num);
		}
		if (!BN_add_word(i, 1))
		errx(1, "error in BN_add_word()");
	}
}
static void
BN_print_dec_fp(FILE *fp, const BIGNUM *num)
{	
	char *buf;
	buf = BN_bn2dec(num);
	if (buf == NULL)
	return;
	fprintf(fp, "%s", buf);
	free(buf);
}
#else
static void BN_print_fp( FILE *fp, const BIGNUM *num ) {
	fprintf( fp, "%lx", (unsigned long) *num );
}
static void BN_print_dec_fp( FILE *fp, const BIGNUM *num ) {
	fprintf( fp, "%lu", (unsigned long) *num );
}
static int BN_dec2bn( BIGNUM **a, const char *str ) {
	char *p;
	errno = 0;
	**a = strtoul( str, &p, 10 );
	return ( errno == 0 && ( *p == '\n' || *p == '\0' ) );
}
static int BN_hex2bn( BIGNUM **a, const char *str ) {
	char *p;
	errno = 0;
	**a = strtoul( str, &p, 16 );
	return ( errno == 0 && ( *p == '\n' || *p == '\0' ) );
}
static BN_ULONG BN_div_word( BIGNUM *a, BN_ULONG b ) {
	BN_ULONG mod;
	mod = *a % b;
	*a /= b;
	return mod;
}
#endif
