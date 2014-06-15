#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static __dead2 void
errexit(const char *prog, const char *reason)
{	
	char *errstr = strerror(errno);
	write(STDERR_FILENO, prog, strlen(prog));
	write(STDERR_FILENO, ": ", 2);
	write(STDERR_FILENO, reason, strlen(reason));
	write(STDERR_FILENO, ": ", 2);
	write(STDERR_FILENO, errstr, strlen(errstr));
	write(STDERR_FILENO, "\n", 1);
	exit(1);
}
int main( int argc, char *argv[] ) {
	int nflag;
	int veclen;
	struct iovec *iov, *vp;
	char space[] = " ";
	char newline[] = "\n";
	char *progname = argv[0];
	if ( *++argv && !strcmp( *argv, "-n" ) ) {
		++argv;
		--argc;
		nflag = 1;
	} else nflag = 0;
	veclen = ( argc >= 2 ) ? ( argc - 2 ) * 2 + 1 : 0;
	if ( ( vp = iov = malloc( ( veclen + 1 ) * sizeof(struct iovec) ) ) == NULL ) errexit( progname, "malloc" );
	while ( argv[0] != NULL ) {
		size_t len;
		len = strlen( argv[0] );
		if ( argv[1] == NULL ) {
			if ( len >= 2 && argv[0][len - 2] == '\\' && argv[0][len - 1] == 'c' ) {
				len -= 2;
				nflag = 1;
			}
		}
		vp->iov_base = *argv;
		vp++->iov_len = len;
		if ( *++argv ) {
			vp->iov_base = space;
			vp++->iov_len = 1;
		}
	}
	if ( !nflag ) {
		veclen++;
		vp->iov_base = newline;
		vp++->iov_len = 1;
	}
	while ( veclen ) {
		int nwrite;
		nwrite = ( veclen > IOV_MAX ) ? IOV_MAX : veclen;
		if ( writev( STDOUT_FILENO, iov, nwrite ) == -1 ) errexit( progname, "write" );
		iov += nwrite;
		veclen -= nwrite;
	}
	return 0;
}
