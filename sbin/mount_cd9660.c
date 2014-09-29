#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1992, 1993, 1994\n\
        The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/cdio.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/module.h>
#include <sys/iconv.h>
#include <sys/linker.h>
#include <arpa/inet.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>
#include "mntopts.h"
static struct mntopt mopts[] = { MOPT_STDOPTS, MOPT_UPDATE, MOPT_END };
static int get_ssector( const char *dev );
static int set_charset( struct iovec **, int *iovlen, const char * );
void usage( void );
int main( int argc, char **argv ) {
	struct iovec *iov;
	int iovlen;
	int ch, mntflags;
	char *dev, *dir, *p, *val, mntpath[MAXPATHLEN];
	int verbose;
	int ssector;
	char fstype[] = "cd9660";
	iov = NULL;
	iovlen = 0;
	mntflags = verbose = 0;
	ssector = -1;
	while ( ( ch = getopt( argc, argv, "begjo:rs:vC:" ) ) != -1 )
		switch ( ch ) {
			case 'b':
				build_iovec( &iov, &iovlen, "brokenjoliet", NULL, ( size_t ) - 1 );
				break;
			case 'e':
				build_iovec( &iov, &iovlen, "extatt", NULL, ( size_t ) - 1 );
				break;
			case 'g':
				build_iovec( &iov, &iovlen, "gens", NULL, ( size_t ) - 1 );
				break;
			case 'j':
				build_iovec( &iov, &iovlen, "nojoliet", NULL, ( size_t ) - 1 );
				break;
			case 'o':
				getmntopts( optarg, mopts, &mntflags, NULL );
				p = strchr( optarg, '=' );
				val = NULL;
				if ( p != NULL ) {
					*p = '\0';
					val = p + 1;
				}
				build_iovec( &iov, &iovlen, optarg, val, ( size_t ) - 1 );
				break;
			case 'r':
				build_iovec( &iov, &iovlen, "norrip", NULL, ( size_t ) - 1 );
				break;
			case 's':
				ssector = atoi( optarg );
				break;
			case 'v':
				verbose++;
				break;
			case 'C':
				if ( set_charset( &iov, &iovlen, optarg ) == -1 ) err( EX_OSERR, "cd9660_iconv" );
				build_iovec( &iov, &iovlen, "kiconv", NULL, ( size_t ) - 1 );
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
	if ( checkpath( dir, mntpath ) != 0 ) err( 1, "%s", mntpath );
	(void) rmslashes( dev, dev );
	if ( ssector == -1 ) {
		if ( ( ssector = get_ssector( dev ) ) == -1 ) {
			if ( verbose ) printf( "could not determine starting sector, "
					"using very first session\n" );
			ssector = 0;
		} else if ( verbose ) printf( "using starting sector %d\n", ssector );
	}
	mntflags |= MNT_RDONLY;
	build_iovec( &iov, &iovlen, "fstype", fstype, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "fspath", mntpath, ( size_t ) - 1 );
	build_iovec( &iov, &iovlen, "from", dev, ( size_t ) - 1 );
	build_iovec_argf( &iov, &iovlen, "ssector", "%d", ssector );
	if ( nmount( iov, iovlen, mntflags ) < 0 ) err( 1, "%s", dev );
	exit( 0 );
}
void usage( void ) {
	(void) fprintf( stderr, "usage: mount_cd9660 [-begjrv] [-C charset] [-o options] [-s startsector]\n"
			"                    special node\n" );
	exit (EX_USAGE);
}
static int get_ssector( const char *dev ) {
	struct ioc_toc_header h;
	struct ioc_read_toc_entry t;
	struct cd_toc_entry toc_buffer[100];
	int fd, ntocentries, i;
	if ( ( fd = open( dev, O_RDONLY ) ) == -1 ) return -1;
	if ( ioctl( fd, CDIOREADTOCHEADER, &h ) == -1 ) {
		close( fd );
		return -1;
	}
	ntocentries = h.ending_track - h.starting_track + 1;
	if ( ntocentries > 100 ) {
		close( fd );
		return -1;
	}
	t.address_format = CD_LBA_FORMAT;
	t.starting_track = 0;
	t.data_len = ntocentries * sizeof(struct cd_toc_entry);
	t.data = toc_buffer;
	if ( ioctl( fd, CDIOREADTOCENTRYS, (char *) &t ) == -1 ) {
		close( fd );
		return -1;
	}
	close( fd );
	for ( i = ntocentries - 1; i >= 0; i-- )
		if ( ( toc_buffer[i].control & 4 ) != 0 ) break;
	if ( i < 0 ) return -1;
	return ntohl( toc_buffer[i].addr.lba );
}
static int set_charset( struct iovec **iov, int *iovlen, const char *localcs ) {
	int error;
	char *cs_disk;
	char *cs_local;
	cs_disk = NULL;
	cs_local = NULL;
	if ( modfind( "cd9660_iconv" ) < 0 ) if ( kldload( "cd9660_iconv" ) < 0 || modfind( "cd9660_iconv" ) < 0 ) {
		warnx( "cannot find or load \"cd9660_iconv\" kernel module" );
		return ( -1 );
	}
	if ( ( cs_disk = malloc( ICONV_CSNMAXLEN ) ) == NULL ) return ( -1 );
	if ( ( cs_local = malloc( ICONV_CSNMAXLEN ) ) == NULL ) {
		free( cs_disk );
		return ( -1 );
	}
	strncpy( cs_disk, ENCODING_UNICODE, ICONV_CSNMAXLEN );
	strncpy( cs_local, kiconv_quirkcs( localcs, KICONV_VENDOR_MICSFT ), ICONV_CSNMAXLEN );
	error = kiconv_add_xlat16_cspairs( cs_disk, cs_local );
	if ( error ) return ( -1 );
	build_iovec( iov, iovlen, "cs_disk", cs_disk, ( size_t ) - 1 );
	build_iovec( iov, iovlen, "cs_local", cs_local, ( size_t ) - 1 );
	return ( 0 );
}
