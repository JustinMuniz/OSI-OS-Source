#if 0
#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1980, 1986, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char sccsid[] = "@(#)reboot.c	8.1 (Berkeley) 6/5/93";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/reboot.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <signal.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utmpx.h>
static void usage( void );
static u_int get_pageins( void );
static int dohalt;
int main( int argc, char *argv[] ) {
	struct utmpx utx;
	const struct passwd *pw;
	int ch, howto, i, fd, lflag, nflag, qflag, sverrno;
	u_int pageins;
	const char *user, *kernel = NULL;
	if ( strcmp( getprogname(), "halt" ) == 0 ) {
		dohalt = 1;
		howto = RB_HALT;
	} else howto = 0;
	lflag = nflag = qflag = 0;
	while ( ( ch = getopt( argc, argv, "dk:lnpq" ) ) != -1 )
		switch ( ch ) {
			case 'd':
				howto |= RB_DUMP;
				break;
			case 'k':
				kernel = optarg;
				break;
			case 'l':
				lflag = 1;
				break;
			case 'n':
				nflag = 1;
				howto |= RB_NOSYNC;
				break;
			case 'p':
				howto |= RB_POWEROFF;
				break;
			case 'q':
				qflag = 1;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( ( howto & ( RB_DUMP | RB_HALT ) ) == ( RB_DUMP | RB_HALT ) ) errx( 1, "cannot dump (-d) when halting; must reboot instead" );
	if ( geteuid() ) {
		errno = EPERM;
		err( 1, NULL );
	}
	if ( qflag ) {
		reboot( howto );
		err( 1, NULL );
	}
	if ( kernel != NULL ) {
		fd = open( "/boot/nextboot.conf", O_WRONLY | O_CREAT | O_TRUNC, 0444 );
		if ( fd > -1 ) {
			(void) write( fd, "nextboot_enable=\"YES\"\n", 22 );
			(void) write( fd, "kernel=\"", 8L );
			(void) write( fd, kernel, strlen( kernel ) );
			(void) write( fd, "\"\n", 2 );
			close( fd );
		}
	}
	if ( !lflag ) {
		if ( ( user = getlogin() ) == NULL ) user = ( pw = getpwuid( getuid() ) ) ? pw->pw_name : "???";
		if ( dohalt ) {
			openlog( "halt", 0, LOG_AUTH | LOG_CONS );
			syslog( LOG_CRIT, "halted by %s", user );
		} else {
			openlog( "reboot", 0, LOG_AUTH | LOG_CONS );
			syslog( LOG_CRIT, "rebooted by %s", user );
		}
	}
	utx.ut_type = SHUTDOWN_TIME;
	gettimeofday( &utx.ut_tv, NULL );
	pututxline( &utx );
	if ( !nflag ) sync();
	(void) signal( SIGHUP, SIG_IGN );
	(void) signal( SIGINT, SIG_IGN );
	(void) signal( SIGQUIT, SIG_IGN );
	(void) signal( SIGTERM, SIG_IGN );
	(void) signal( SIGTSTP, SIG_IGN );
	(void) signal( SIGPIPE, SIG_IGN );
	if ( kill( 1, SIGTSTP ) == -1 ) err( 1, "SIGTSTP init" );
	if ( kill( -1, SIGTERM ) == -1 && errno != ESRCH ) err( 1, "SIGTERM processes" );
	sleep( 2 );
	for ( i = 0; i < 20; i++ ) {
		pageins = get_pageins();
		if ( !nflag ) sync();
		sleep( 3 );
		if ( get_pageins() == pageins ) break;
	}
	for ( i = 1;; ++i ) {
		if ( kill( -1, SIGKILL ) == -1 ) {
			if ( errno == ESRCH ) break;
			goto restart;
		}
		if ( i > 5 ) {
			(void) fprintf( stderr, "WARNING: some process(es) wouldn't die\n" );
			break;
		}
		(void) sleep( 2 * i );
	}
	reboot( howto );
	restart: sverrno = errno;
	errx( 1, "%s%s", kill( 1, SIGHUP ) == -1 ? "(can't restart init): " : "", strerror( sverrno ) );
}
static void usage( void ) {
	(void) fprintf( stderr, dohalt ? "usage: halt [-lnpq] [-k kernel]\n" : "usage: reboot [-dlnpq] [-k kernel]\n" );
	exit( 1 );
}
static u_int get_pageins( void ) {
	u_int pageins;
	size_t len;
	len = sizeof( pageins );
	if ( sysctlbyname( "vm.stats.vm.v_swappgsin", &pageins, &len, NULL, 0 ) != 0 ) {
		warnx( "v_swappgsin" );
		return ( 0 );
	}
	return pageins;
}
