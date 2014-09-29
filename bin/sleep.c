#include <sys/cdefs.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static volatile sig_atomic_t report_requested;
static void report_request(int signo __unused) {
	report_requested = 1;
}
int main( int argc, char *argv[] ) {
	struct timespec time_to_sleep;
	double d;
	time_t original;
	char buf[2];
	if ( argc != 2 ) exit( 1 );
	if ( sscanf( argv[1], "%lf%1s", &d, buf ) != 1 ) exit( 1 );
	if ( d > INT_MAX ) exit( 1 );
	if ( d <= 0 ) return ( 0 );
	original = time_to_sleep.tv_sec = (time_t) d;
	time_to_sleep.tv_nsec = 1e9 * ( d - time_to_sleep.tv_sec );
	signal( SIGINFO, report_request );
	while ( nanosleep( &time_to_sleep, &time_to_sleep ) != 0 ) {
		if ( report_requested ) {
			warnx( "about %d second(s) left out of the original %d", (int) time_to_sleep.tv_sec, (int) original );
			report_requested = 0;
		} else if ( errno != EINTR ) err( 1, "nanosleep" );
	}
	return ( 0 );
}
