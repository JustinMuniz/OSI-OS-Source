#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static const char sccsid[] = "@(#)pom.c       8.1 (Berkeley) 5/31/93";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h> 
#ifndef	PI
#define	PI	  3.14159265358979323846
#endif
#define	EPOCH	  85
#define	EPSILONg  279.611371	
#define	RHOg	  282.680403	
#define	ECCEN	  0.01671542	
#define	lzero	  18.251907	
#define	Pzero	  192.917585	
#define	Nzero	  55.204723	
#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)
static void adj360( double * );
static double dtor( double );
static double potm( double );
static void usage( char *progname );
int main( int argc, char **argv ) {
	time_t tt;
	struct tm GMT, tmd;
	double days, today, tomorrow;
	int ch, cnt, pflag = 0;
	char *odate = NULL, *otime = NULL;
	char *progname = argv[0];
	while ( ( ch = getopt( argc, argv, "d:pt:" ) ) != -1 )
		switch ( ch ) {
			case 'd':
				odate = optarg;
				break;
			case 'p':
				pflag = 1;
				break;
			case 't':
				otime = optarg;
				break;
			default:
				usage( progname );
		}
	argc -= optind;
	argv += optind;
	if ( argc ) usage( progname );
	time( &tt );
	if ( otime != NULL || odate != NULL ) {
		localtime_r( &tt, &tmd );
		if ( odate != NULL ) {
			tmd.tm_year = strtol( odate, NULL, 10 ) - 1900;
			tmd.tm_mon = strtol( odate + 5, NULL, 10 ) - 1;
			tmd.tm_mday = strtol( odate + 8, NULL, 10 );
			tmd.tm_hour = 0;
			tmd.tm_min = 0;
			tmd.tm_sec = 0;
		}
		if ( otime != NULL ) {
			tmd.tm_hour = strtol( otime, NULL, 10 );
			tmd.tm_min = strtol( otime + 3, NULL, 10 );
			tmd.tm_sec = strtol( otime + 6, NULL, 10 );
		}
		tt = mktime( &tmd );
	}
	gmtime_r( &tt, &GMT );
	days = ( GMT.tm_yday + 1 ) + ( ( GMT.tm_hour + ( GMT.tm_min / 60.0 ) + ( GMT.tm_sec / 3600.0 ) ) / 24.0 );
	for ( cnt = EPOCH; cnt < GMT.tm_year; ++cnt )
		days += isleap(1900 + cnt) ? 366 : 365;
	today = potm( days ) + .5;
	if ( pflag ) {
		(void) printf( "%1.0f\n", today );
		return ( 0 );
	}
	(void) printf( "The Moon is " );
	if ( (int) today == 100 ) (void) printf( "Full\n" );
	else if ( !(int) today ) (void) printf( "New\n" );
	else {
		tomorrow = potm( days + 1 );
		if ( (int) today == 50 ) (void) printf( "%s\n", tomorrow > today ? "at the First Quarter" : "at the Last Quarter" );
		else {
			(void) printf( "%s ", tomorrow > today ? "Waxing" : "Waning" );
			if ( today > 50 ) (void) printf( "Gibbous (%1.0f%% of Full)\n", today );
			else if ( today < 50 ) (void) printf( "Crescent (%1.0f%% of Full)\n", today );
		}
	}
	return 0;
}
static double potm( double days ) {
	double N, Msol, Ec, LambdaSol, l, Mm, Ev, Ac, A3, Mmprime;
	double A4, lprime, V, ldprime, D, Nm;
	N = 360 * days / 365.2422;
	adj360( &N );
	Msol = N + EPSILONg - RHOg;
	adj360( &Msol );
	Ec = 360 / PI * ECCEN * sin( dtor( Msol ) );
	LambdaSol = N + Ec + EPSILONg;
	adj360( &LambdaSol );
	l = 13.1763966 * days + lzero;
	adj360( &l );
	Mm = l - ( 0.1114041 * days ) - Pzero;
	adj360( &Mm );
	Nm = Nzero - ( 0.0529539 * days );
	adj360( &Nm );
	Ev = 1.2739 * sin( dtor( 2 * ( l - LambdaSol ) - Mm ) );
	Ac = 0.1858 * sin( dtor( Msol ) );
	A3 = 0.37 * sin( dtor( Msol ) );
	Mmprime = Mm + Ev - Ac - A3;
	Ec = 6.2886 * sin( dtor( Mmprime ) );
	A4 = 0.214 * sin( dtor( 2 * Mmprime ) );
	lprime = l + Ev + Ec - Ac + A4;
	V = 0.6583 * sin( dtor( 2 * ( lprime - LambdaSol ) ) );
	ldprime = lprime + V;
	D = ldprime - LambdaSol;
	return ( 50 * ( 1 - cos( dtor( D ) ) ) );
}
static double dtor( double deg ) {
	return ( deg * PI / 180 );
}
static void adj360( double *deg ) {
	for ( ;; )
		if ( *deg < 0 ) *deg += 360;
		else if ( *deg > 360 ) *deg -= 360;
		else break;
}
static void usage( char *progname ) {
	fprintf( stderr, "Usage: %s [-p] [-d yyyy.mm.dd] [-t hh:mm:ss]\n", progname );
	exit (EX_USAGE);
}
