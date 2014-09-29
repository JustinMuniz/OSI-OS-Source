#include <sys/cdio.h>
#include <sys/file.h>
#include <sys/iconv.h>
#include <sys/param.h>
#include <sys/linker.h>
#include <sys/module.h>
#include <sys/mount.h>
#include <sys/uio.h>
#include <fs/udf/udf_mount.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "mntopts.h"
static struct mntopt mopts[] = { MOPT_STDOPTS, MOPT_UPDATE, MOPT_END };
int set_charset( char **, char **, const char * );
void usage( void );
int main( int argc, char **argv ) {
	char mntpath[MAXPATHLEN];
	char fstype[] = "udf";
	struct iovec *iov;
	char *cs_disk, *cs_local, *dev, *dir;
	int ch, i, iovlen, mntflags, udf_flags, verbose;
	i = iovlen = mntflags = udf_flags = verbose = 0;
	cs_disk = cs_local = NULL;
	iov = NULL;
	while ( ( ch = getopt( argc, argv, "o:vC:" ) ) != -1 )
		switch ( ch ) {
			case 'o':
				getmntopts( optarg, mopts, &mntflags, NULL );
				break;
			case 'v':
				verbose++;
				break;
			case 'C':
				if ( set_charset( &cs_disk, &cs_local, optarg ) == -1 ) err( EX_OSERR, "udf_iconv" );
				udf_flags |= UDFMNT_KICONV;
				break;
			case '?':
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( argc != 2 ) usage();
	dev = argv[0];
	dir = argv[1];
	if ( checkpath( dir, mntpath ) != 0 ) err( EX_USAGE, "%s", mntpath );
	(void) rmslashes( dev, dev );
	mntflags |= MNT_RDONLY;
	build_iovec( &iov, &iovlen, "fstype", fstype, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "fspath", mntpath, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "from", dev, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "flags", &udf_flags, sizeof( udf_flags ) );
	if ( udf_flags & UDFMNT_KICONV ) {
		build_iovec( &iov, &iovlen, "cs_disk", cs_disk, ( size_t ) - 1 );
		build_iovec( &iov, &iovlen, "cs_local", cs_local, ( size_t ) - 1 );
	}
	if ( nmount( iov, i, mntflags ) < 0 ) err( 1, "%s", dev );
	exit( 0 );
}
int set_charset( char **cs_disk, char **cs_local, const char *localcs ) {
	int error;
	if ( modfind( "udf_iconv" ) < 0 ) if ( kldload( "udf_iconv" ) < 0 || modfind( "udf_iconv" ) < 0 ) {
		warnx( "cannot find or load \"udf_iconv\" kernel module" );
		return ( -1 );
	}
	if ( ( *cs_disk = malloc( ICONV_CSNMAXLEN ) ) == NULL ) return ( -1 );
	if ( ( *cs_local = malloc( ICONV_CSNMAXLEN ) ) == NULL ) return ( -1 );
	strncpy( *cs_disk, ENCODING_UNICODE, ICONV_CSNMAXLEN );
	strncpy( *cs_local, localcs, ICONV_CSNMAXLEN );
	error = kiconv_add_xlat16_cspairs( *cs_disk, *cs_local );
	if ( error ) return ( -1 );
	return ( 0 );
}
void usage( void ) {
	(void) fprintf( stderr, "usage: mount_udf [-v] [-o options] [-C charset] special node\n" );
	exit (EX_USAGE);
}
