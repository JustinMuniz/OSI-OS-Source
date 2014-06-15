#include <sys/cdefs.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef SHELL
#define main killcmd
#include "bltin/bltin.h"
#include "error.h"
#endif
static void nosig( const char * );
static void printsignals( FILE * );
static int signame_to_signum( const char * );
static void usage_error( void );
int main( int argc, char *argv[] ) {
	int errors, numsig, pid;
	char *ep;
	if ( argc < 2 ) usage_error();
	numsig = SIGTERM;
	argc--, argv++;
	if ( !strcmp( *argv, "-l" ) ) {
		argc--, argv++;
		if ( argc > 1 ) usage_error();
		if ( argc == 1 ) {
			if ( !isdigit( **argv ) ) usage_error();
			numsig = strtol( *argv, &ep, 10 );
			if ( !**argv || *ep ) errx( 2, "illegal signal number: %s", *argv );
			if ( numsig >= 128 ) numsig -= 128;
			if ( numsig <= 0 || numsig >= sys_nsig ) nosig( *argv );
			printf( "%s\n", sys_signame[numsig] );
			return ( 0 );
		}
		printsignals (stdout);
		return ( 0 );
	}
	if ( !strcmp( *argv, "-s" ) ) {
		argc--, argv++;
		if ( argc < 1 ) {
			warnx( "option requires an argument -- s" );
			usage_error();
		}
		if ( strcmp( *argv, "0" ) ) {
			if ( ( numsig = signame_to_signum( *argv ) ) < 0 ) nosig( *argv );
		} else numsig = 0;
		argc--, argv++;
	} else if ( **argv == '-' && *( *argv + 1 ) != '-' ) {
		++*argv;
		if ( isalpha( **argv ) ) {
			if ( ( numsig = signame_to_signum( *argv ) ) < 0 ) nosig( *argv );
		} else if ( isdigit( **argv ) ) {
			numsig = strtol( *argv, &ep, 10 );
			if ( !**argv || *ep ) errx( 2, "illegal signal number: %s", *argv );
			if ( numsig < 0 ) nosig( *argv );
		} else nosig( *argv );
		argc--, argv++;
	}
	if ( argc > 0 && strncmp( *argv, "--", 2 ) == 0 ) argc--, argv++;
	if ( argc == 0 ) usage_error();
	for ( errors = 0; argc; argc--, argv++ ) {
#ifdef SHELL
		if (**argv == '%')
		pid = getjobpgrp(*argv);
		else
#endif
		{
			pid = strtol( *argv, &ep, 10 );
			if ( !**argv || *ep ) errx( 2, "illegal process id: %s", *argv );
		}
		if ( kill( pid, numsig ) == -1 ) {
			warn( "%s", *argv );
			errors = 1;
		}
	}
	return ( errors );
}
static int signame_to_signum( const char *sig ) {
	int n;
	if ( strncasecmp( sig, "SIG", 3 ) == 0 ) sig += 3;
	for ( n = 1; n < sys_nsig; n++ ) {
		if ( !strcasecmp( sys_signame[n], sig ) ) return ( n );
	}
	return ( -1 );
}
static void nosig( const char *name ) {
	warnx( "unknown signal %s; valid signals:", name );
	printsignals (stderr);
#ifdef SHELL
	error(NULL);
#else
	exit( 2 );
#endif
}
static void printsignals( FILE *fp ) {
	int n;
	for ( n = 1; n < sys_nsig; n++ ) {
		(void) fprintf( fp, "%s", sys_signame[n] );
		if ( n == ( sys_nsig / 2 ) || n == ( sys_nsig - 1 ) ) (void) fprintf( fp, "\n" );
		else (void) fprintf( fp, " " );
	}
}
static void usage_error( void ) {
#ifdef SHELL
	error(NULL);
#else
	exit( 2 );
#endif
}
