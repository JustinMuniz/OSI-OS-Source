#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <ufs/ufs/ufsmount.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <limits.h>
#include <mntopts.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
static void usage( void ) {
	errx( EX_USAGE, "usage: mksnap_ffs snapshot_name" );
}
int main( int argc, char **argv ) {
	char errmsg[255], path[PATH_MAX];
	char *cp, *snapname;
	struct statfs stfsbuf;
	struct group *grp;
	struct stat stbuf;
	struct iovec *iov;
	int fd, iovlen;
	if ( argc == 2 ) snapname = argv[1];
	else if ( argc == 3 ) snapname = argv[2];
	else usage();
	if ( strlen( snapname ) >= PATH_MAX ) errx( 1, "pathname too long %s", snapname );
	cp = strrchr( snapname, '/' );
	if ( cp == NULL ) {
		strlcpy( path, ".", PATH_MAX );
	} else if ( cp == snapname ) {
		strlcpy( path, "/", PATH_MAX );
	} else {
		strlcpy( path, snapname, cp - snapname + 1 );
	}
	if ( statfs( path, &stfsbuf ) < 0 ) err( 1, "%s", path );
	if ( stat( path, &stbuf ) < 0 ) err( 1, "%s", path );
	if ( !S_ISDIR( stbuf.st_mode ) ) errx( 1, "%s: Not a directory", path );
	if ( access( path, W_OK ) < 0 ) err( 1, "Lack write permission in %s", path );
	if ( ( stbuf.st_mode & S_ISTXT ) && stbuf.st_uid != getuid() ) errx( 1, "Lack write permission in %s: Sticky bit set", path );
	if ( ( grp = getgrnam( "operator" ) ) == NULL ) errx( 1, "Cannot retrieve operator gid" );
	iov = NULL;
	iovlen = 0;
	build_iovec( &iov, &iovlen, "fstype", "ffs", 4 );
	build_iovec( &iov, &iovlen, "from", snapname, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "fspath", stfsbuf.f_mntonname, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "errmsg", errmsg, sizeof( errmsg ) );
	build_iovec( &iov, &iovlen, "update", NULL, 0 );
	build_iovec( &iov, &iovlen, "snapshot", NULL, 0 );
	*errmsg = '\0';
	if ( nmount( iov, iovlen, stfsbuf.f_flags ) < 0 ) {
		errmsg[sizeof( errmsg ) - 1] = '\0';
		err( 1, "Cannot create snapshot %s%s%s", snapname, *errmsg != '\0' ? ": " : "", errmsg );
	}
	if ( ( fd = open( snapname, O_RDONLY ) ) < 0 ) err( 1, "Cannot open %s", snapname );
	if ( fstat( fd, &stbuf ) != 0 ) err( 1, "Cannot stat %s", snapname );
	if ( ( stbuf.st_flags & SF_SNAPSHOT ) == 0 ) errx( 1, "File %s is not a snapshot", snapname );
	if ( fchown( fd, -1, grp->gr_gid ) != 0 ) err( 1, "Cannot chown %s", snapname );
	if ( fchmod( fd, S_IRUSR | S_IRGRP ) != 0 ) err( 1, "Cannot chmod %s", snapname );
	exit (EXIT_SUCCESS);
}
