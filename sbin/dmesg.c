#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static const char sccsid[] = "@(#)dmesg.c	8.1 (Berkeley) 6/5/93";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/msgbuf.h>
#include <sys/sysctl.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <limits.h>
#include <locale.h>
#include <nlist.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <vis.h>
#include <sys/syslog.h>
static struct nlist nl[] = {
#define	X_MSGBUF	0
		{ "_msgbufp", 0, 0, 0, 0 }, { NULL, 0, 0, 0, 0 }, };
void usage( void )
__dead2;
#define	KREAD(addr, var) \
	kvm_read(kd, addr, &var, sizeof(var)) != sizeof(var)
int main( int argc, char *argv[] ) {
	struct msgbuf *bufp, cur;
	char *bp, *ep, *memf, *nextp, *nlistf, *p, *q, *visbp;
	kvm_t *kd;
	size_t buflen, bufpos;
	long pri;
	int ch, clear;
	bool all;
	all = false;
	clear = false;
	(void) setlocale( LC_CTYPE, "" );
	memf = nlistf = NULL;
	while ( ( ch = getopt( argc, argv, "acM:N:" ) ) != -1 )
		switch ( ch ) {
			case 'a':
				all = true;
				break;
			case 'c':
				clear = true;
				break;
			case 'M':
				memf = optarg;
				break;
			case 'N':
				nlistf = optarg;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	if ( argc != 0 ) usage();
	if ( memf == NULL ) {
		if ( sysctlbyname( "kern.msgbuf", NULL, &buflen, NULL, 0 ) == -1 ) err( 1, "sysctl kern.msgbuf" );
		if ( ( bp = malloc( buflen + 2 ) ) == NULL ) errx( 1, "malloc failed" );
		if ( sysctlbyname( "kern.msgbuf", bp, &buflen, NULL, 0 ) == -1 ) err( 1, "sysctl kern.msgbuf" );
		if ( clear ) if ( sysctlbyname( "kern.msgbuf_clear", NULL, NULL, &clear, sizeof(int) ) ) err( 1, "sysctl kern.msgbuf_clear" );
	} else {
		kd = kvm_open( nlistf, memf, NULL, O_RDONLY, "dmesg" );
		if ( kd == NULL ) exit( 1 );
		if ( kvm_nlist( kd, nl ) == -1 ) errx( 1, "kvm_nlist: %s", kvm_geterr( kd ) );
		if ( nl[X_MSGBUF].n_type == 0 ) errx( 1, "%s: msgbufp not found", nlistf ? nlistf : "namelist" );
		if ( KREAD(nl[X_MSGBUF].n_value, bufp) || KREAD( (long )bufp, cur ) ) errx( 1, "kvm_read: %s", kvm_geterr( kd ) );
		if ( cur.msg_magic != MSG_MAGIC ) errx( 1, "kernel message buffer has different magic "
				"number" );
		if ( ( bp = malloc( cur.msg_size + 2 ) ) == NULL ) errx( 1, "malloc failed" );
		bufpos = MSGBUF_SEQ_TO_POS( &cur, cur.msg_wseq );
		if ( kvm_read( kd, (long) &cur.msg_ptr[bufpos], bp, cur.msg_size - bufpos ) != ( ssize_t )( cur.msg_size - bufpos ) ) errx( 1, "kvm_read: %s", kvm_geterr( kd ) );
		if ( bufpos != 0 && kvm_read( kd, (long) cur.msg_ptr, &bp[cur.msg_size - bufpos], bufpos ) != (ssize_t) bufpos ) errx( 1, "kvm_read: %s", kvm_geterr( kd ) );
		kvm_close( kd );
		buflen = cur.msg_size;
	}
	if ( buflen == 0 || bp[buflen - 1] != '\n' ) bp[buflen++] = '\n';
	bp[buflen] = '\0';
	if ( ( visbp = malloc( 4 * buflen + 1 ) ) == NULL ) errx( 1, "malloc failed" );
	p = bp;
	ep = &bp[buflen];
	if ( *p == '\0' ) {
		while ( *p == '\0' )
			p++;
	} else if ( !all ) {
		p = memchr( p, '\n', ep - p );
		p++;
	}
	for ( ; p < ep; p = nextp ) {
		nextp = memchr( p, '\n', ep - p );
		nextp++;
		if ( *p == '<' && isdigit( *( p + 1 ) ) ) {
			errno = 0;
			pri = strtol( p + 1, &q, 10 );
			if ( *q == '>' && pri >= 0 && pri < INT_MAX && errno == 0 ) {
				if ( LOG_FAC( pri ) != LOG_KERN && !all ) continue;
				p = q + 1;
			}
		}
		(void) strvisx( visbp, p, nextp - p, 0 );
		(void) printf( "%s", visbp );
	}
	exit( 0 );
}
void usage( void ) {
	fprintf( stderr, "usage: dmesg [-ac] [-M core [-N system]]\n" );
	exit( 1 );
}
