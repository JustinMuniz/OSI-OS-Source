#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1992, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif
#ifndef lint
static char sccsid[] = "@(#)chflags.c	8.5 (Berkeley) 4/1/94";
#endif
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static void usage( void );
int main( int argc, char *argv[] ) {
	FTS *ftsp;
	FTSENT *p;
	u_long clear, newflags, set;
	long val;
	int Hflag, Lflag, Rflag, fflag, hflag, vflag;
	int ch, fts_options, oct, rval;
	char *flags, *ep;
	int (*change_flags)( const char *, unsigned long );
	Hflag = Lflag = Rflag = fflag = hflag = vflag = 0;
	while ( ( ch = getopt( argc, argv, "HLPRfhv" ) ) != -1 )
		switch ( ch ) {
			case 'H':
				Hflag = 1;
				Lflag = 0;
				break;
			case 'L':
				Lflag = 1;
				Hflag = 0;
				break;
			case 'P':
				Hflag = Lflag = 0;
				break;
			case 'R':
				Rflag = 1;
				break;
			case 'f':
				fflag = 1;
				break;
			case 'h':
				hflag = 1;
				break;
			case 'v':
				vflag++;
				break;
			case '?':
			default:
				usage();
		}
	argv += optind;
	argc -= optind;
	if ( argc < 2 ) usage();
	if ( Rflag ) {
		fts_options = FTS_PHYSICAL;
		if ( hflag ) errx( 1, "the -R and -h options "
				"may not be specified together" );
		if ( Hflag ) fts_options |= FTS_COMFOLLOW;
		if ( Lflag ) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else fts_options = hflag ? FTS_PHYSICAL : FTS_LOGICAL;
	change_flags = hflag ? lchflags : chflags;
	flags = *argv;
	if ( *flags >= '0' && *flags <= '7' ) {
		errno = 0;
		val = strtol( flags, &ep, 8 );
		if ( val < 0 ) errno = ERANGE;
		if ( errno ) err( 1, "invalid flags: %s", flags );
		if ( *ep ) errx( 1, "invalid flags: %s", flags );
		set = val;
		oct = 1;
	} else {
		if ( strtofflags( &flags, &set, &clear ) ) errx( 1, "invalid flag: %s", flags );
		clear = ~clear;
		oct = 0;
	}
	if ( ( ftsp = fts_open( ++argv, fts_options, 0 ) ) == NULL ) err( 1, NULL );
	for ( rval = 0; ( p = fts_read( ftsp ) ) != NULL; ) {
		switch ( p->fts_info ) {
			case FTS_D:
				if ( !Rflag ) fts_set( ftsp, p, FTS_SKIP );
				continue;
			case FTS_DNR:
				warnx( "%s: %s", p->fts_path, strerror( p->fts_errno ) );
				rval = 1;
				break;
			case FTS_ERR:
			case FTS_NS:
				warnx( "%s: %s", p->fts_path, strerror( p->fts_errno ) );
				rval = 1;
				continue;
			case FTS_SL:
			case FTS_SLNONE:
				if ( !hflag ) continue;
			default:
				break;
		}
		if ( oct ) newflags = set;
		else newflags = ( p->fts_statp->st_flags | set ) & clear;
		if ( newflags == p->fts_statp->st_flags ) continue;
		if ( ( *change_flags )( p->fts_accpath, newflags ) && !fflag ) {
			warn( "%s", p->fts_path );
			rval = 1;
		} else if ( vflag ) {
			(void) printf( "%s", p->fts_path );
			if ( vflag > 1 ) (void) printf( ": 0%lo -> 0%lo", (u_long) p->fts_statp->st_flags, newflags );
			(void) printf( "\n" );
		}
	}
	if ( errno ) err( 1, "fts_read" );
	exit( rval );
}
static void usage( void ) {
	(void) fprintf( stderr, "usage: chflags [-fhv] [-R [-H | -L | -P]] flags file ...\n" );
	exit( 1 );
}
