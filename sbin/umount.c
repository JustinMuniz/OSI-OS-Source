#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1980, 1989, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
#if 0
static char sccsid[] = "@(#)umount.c	8.8 (Berkeley) 5/8/95";
#endif
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <rpc/rpc.h>
#include <rpcsvc/mount.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "mounttab.h"
typedef enum {
	FIND, REMOVE, CHECKUNIQUE
} dowhat;
static struct addrinfo *nfshost_ai = NULL;
static int fflag, vflag;
static char *nfshost;
struct statfs *checkmntlist( char * );
int checkvfsname( const char *, char ** );
struct statfs *getmntentry( const char *fromname, const char *onname, fsid_t *fsid, dowhat what );
char **makevfslist( const char * );
size_t mntinfo( struct statfs ** );
int namematch( struct addrinfo * );
int parsehexfsid( const char *hex, fsid_t *fsid );
int sacmp( void *, void * );
int umountall( char ** );
int checkname( char *, char ** );
int umountfs( struct statfs *sfs );
void usage( void );
int xdr_dir( XDR *, char * );
int main( int argc, char *argv[] ) {
	int all, errs, ch, mntsize, error;
	char **typelist = NULL;
	struct statfs *mntbuf, *sfs;
	struct addrinfo hints;
	all = errs = 0;
	while ( ( ch = getopt( argc, argv, "AaF:fh:t:v" ) ) != -1 )
		switch ( ch ) {
			case 'A':
				all = 2;
				break;
			case 'a':
				all = 1;
				break;
			case 'F':
				setfstab (optarg);
				break;
			case 'f':
				fflag = MNT_FORCE;
				break;
			case 'h':
				all = 2;
				nfshost = optarg;
				break;
			case 't':
				if ( typelist != NULL ) err( 1, "only one -t option may be specified" );
				typelist = makevfslist( optarg );
				break;
			case 'v':
				vflag = 1;
				break;
			default:
				usage();
		}
	argc -= optind;
	argv += optind;
	if ( ( fflag & MNT_FORCE ) == 0 ) sync();
	if ( ( argc == 0 && !all ) || ( argc != 0 && all ) ) usage();
	if ( ( nfshost != NULL ) && ( typelist == NULL ) ) typelist = makevfslist( "nfs" );
	if ( nfshost != NULL ) {
		memset( &hints, 0, sizeof hints );
		error = getaddrinfo( nfshost, NULL, &hints, &nfshost_ai );
		if ( error ) errx( 1, "%s: %s", nfshost, gai_strerror( error ) );
	}
	switch ( all ) {
		case 2:
			if ( ( mntsize = mntinfo( &mntbuf ) ) <= 0 ) break;
			for ( errs = 0, mntsize--; mntsize > 0; mntsize-- ) {
				sfs = &mntbuf[mntsize];
				if ( checkvfsname( sfs->f_fstypename, typelist ) ) continue;
				if ( strcmp( sfs->f_mntonname, "/dev" ) == 0 ) continue;
				if ( umountfs( sfs ) != 0 ) errs = 1;
			}
			free( mntbuf );
			break;
		case 1:
			if ( setfsent() == 0 ) err( 1, "%s", getfstab() );
			errs = umountall( typelist );
			break;
		case 0:
			for ( errs = 0; *argv != NULL; ++argv )
				if ( checkname( *argv, typelist ) != 0 ) errs = 1;
			break;
	}
	exit( errs );
}
int umountall( char **typelist ) {
	struct xvfsconf vfc;
	struct fstab *fs;
	int rval;
	char *cp;
	static int firstcall = 1;
	if ( ( fs = getfsent() ) != NULL ) firstcall = 0;
	else if ( firstcall ) errx( 1, "fstab reading failure" );
	else return ( 0 );
	do {
		if ( strcmp( fs->fs_file, "/" ) == 0 ) continue;
		if ( strcmp( fs->fs_type, FSTAB_RW ) && strcmp( fs->fs_type, FSTAB_RO ) && strcmp( fs->fs_type, FSTAB_RQ ) ) continue;
		if ( getvfsbyname( fs->fs_vfstype, &vfc ) == -1 ) continue;
		if ( checkvfsname( fs->fs_vfstype, typelist ) ) continue;
		if ( ( cp = malloc( (size_t) strlen( fs->fs_file ) + 1 ) ) == NULL ) err( 1, "malloc failed" );
		(void) strcpy( cp, fs->fs_file );
		rval = umountall( typelist );
		rval = checkname( cp, typelist ) || rval;
		free( cp );
		return ( rval );
	} while ( ( fs = getfsent() ) != NULL );
	return ( 0 );
}
int checkname( char *mntname, char **typelist ) {
	char buf[MAXPATHLEN];
	struct statfs sfsbuf;
	struct stat sb;
	struct statfs *sfs;
	char *delimp;
	dev_t dev;
	int len;
	sfs = checkmntlist( mntname );
	if ( sfs == NULL ) {
		len = strlen( mntname );
		while ( len > 1 && mntname[len - 1] == '/' )
			mntname[--len] = '\0';
		sfs = checkmntlist( mntname );
	}
	if ( sfs == NULL && ( delimp = strrchr( mntname, '@' ) ) != NULL ) {
		snprintf( buf, sizeof( buf ), "%s:%.*s", delimp + 1, (int) ( delimp - mntname ), mntname );
		len = strlen( buf );
		while ( len > 1 && buf[len - 1] == '/' )
			buf[--len] = '\0';
		sfs = checkmntlist( buf );
	}
	if ( sfs == NULL || ( getmntentry( NULL, mntname, NULL, FIND ) != NULL && getmntentry( NULL, mntname, NULL, CHECKUNIQUE ) == NULL ) ) {
		if ( statfs( mntname, &sfsbuf ) != 0 ) {
			warn( "%s: statfs", mntname );
		} else if ( stat( mntname, &sb ) != 0 ) {
			warn( "%s: stat", mntname );
		} else if ( S_ISDIR( sb.st_mode ) ) {
			dev = sb.st_dev;
			snprintf( buf, sizeof( buf ), "%s/..", mntname );
			if ( stat( buf, &sb ) != 0 ) {
				warn( "%s: stat", buf );
			} else if ( sb.st_dev == dev ) {
				warnx( "%s: not a file system root directory", mntname );
				return ( 1 );
			} else sfs = &sfsbuf;
		}
	}
	if ( sfs == NULL ) {
		warnx( "%s: unknown file system", mntname );
		return ( 1 );
	}
	if ( checkvfsname( sfs->f_fstypename, typelist ) ) return ( 1 );
	return ( umountfs( sfs ) );
}
int umountfs( struct statfs *sfs ) {
	char fsidbuf[64];
	enum clnt_stat clnt_stat;
	struct timeval try;
	struct addrinfo *ai, hints;
	int do_rpc;
	CLIENT *clp;
	char *nfsdirname, *orignfsdirname;
	char *hostp, *delimp;
	ai = NULL;
	do_rpc = 0;
	hostp = NULL;
	nfsdirname = delimp = orignfsdirname = NULL;
	memset( &hints, 0, sizeof hints );
	if ( strcmp( sfs->f_fstypename, "nfs" ) == 0 ) {
		if ( ( nfsdirname = strdup( sfs->f_mntfromname ) ) == NULL ) err( 1, "strdup" );
		orignfsdirname = nfsdirname;
		if ( *nfsdirname == '[' && ( delimp = strchr( nfsdirname + 1, ']' ) ) != NULL && *( delimp + 1 ) == ':' ) {
			hostp = nfsdirname + 1;
			nfsdirname = delimp + 2;
		} else if ( ( delimp = strrchr( nfsdirname, ':' ) ) != NULL ) {
			hostp = nfsdirname;
			nfsdirname = delimp + 1;
		}
		if ( hostp != NULL ) {
			*delimp = '\0';
			getaddrinfo( hostp, NULL, &hints, &ai );
			if ( ai == NULL ) {
				warnx( "can't get net id for host" );
			}
		}
		if ( getmntentry( sfs->f_mntfromname, NULL, NULL, CHECKUNIQUE ) != NULL ) do_rpc = 1;
	}
	if ( !namematch( ai ) ) {
		free( orignfsdirname );
		return ( 1 );
	}
	snprintf( fsidbuf, sizeof( fsidbuf ), "FSID:%d:%d", sfs->f_fsid.val[0], sfs->f_fsid.val[1] );
	if ( unmount( fsidbuf, fflag | MNT_BYFSID ) != 0 ) {
		if ( errno != ENOENT || sfs->f_fsid.val[0] != 0 || sfs->f_fsid.val[1] != 0 ) warn( "unmount of %s failed", sfs->f_mntonname );
		if ( errno != ENOENT ) {
			free( orignfsdirname );
			return ( 1 );
		}
		if ( sfs->f_fsid.val[0] != 0 || sfs->f_fsid.val[1] != 0 ) warnx( "retrying using path instead of file system ID" );
		if ( unmount( sfs->f_mntonname, fflag ) != 0 ) {
			warn( "unmount of %s failed", sfs->f_mntonname );
			free( orignfsdirname );
			return ( 1 );
		}
	}
	getmntentry( NULL, NULL, &sfs->f_fsid, REMOVE );
	if ( vflag ) (void) printf( "%s: unmount from %s\n", sfs->f_mntfromname, sfs->f_mntonname );
	if ( ai != NULL && !( fflag & MNT_FORCE ) && do_rpc ) {
		clp = clnt_create( hostp, MOUNTPROG, MOUNTVERS, "udp" );
		if ( clp == NULL ) {
			warnx( "%s: %s", hostp, clnt_spcreateerror( "MOUNTPROG" ) );
			free( orignfsdirname );
			return ( 1 );
		}
		clp->cl_auth = authsys_create_default();
		try.tv_sec = 20;
		try.tv_usec = 0;
		clnt_stat = clnt_call(clp, MOUNTPROC_UMNT, (xdrproc_t)xdr_dir,
				nfsdirname, (xdrproc_t)xdr_void, (caddr_t)0, try);
		if ( clnt_stat != RPC_SUCCESS ) {
			warnx( "%s: %s", hostp, clnt_sperror( clp, "RPCMNT_UMOUNT" ) );
			free( orignfsdirname );
			return ( 1 );
		}
		if ( read_mtab() ) {
			clean_mtab( hostp, nfsdirname, vflag );
			if ( !write_mtab( vflag ) ) warnx( "cannot remove mounttab entry %s:%s", hostp, nfsdirname );
			free_mtab();
		}
		auth_destroy( clp->cl_auth );
		clnt_destroy( clp );
	}
	free( orignfsdirname );
	return ( 0 );
}
struct statfs *
getmntentry( const char *fromname, const char *onname, fsid_t *fsid, dowhat what ) {
	static struct statfs *mntbuf;
	static size_t mntsize = 0;
	static char *mntcheck = NULL;
	struct statfs *sfs, *foundsfs;
	int i, count;
	if ( mntsize <= 0 ) {
		if ( ( mntsize = mntinfo( &mntbuf ) ) <= 0 ) return ( NULL );
	}
	if ( mntcheck == NULL ) {
		if ( ( mntcheck = calloc( mntsize + 1, sizeof(int) ) ) == NULL ) err( 1, "calloc" );
	}
	count = 0;
	foundsfs = NULL;
	for ( i = mntsize - 1; i >= 0; i-- ) {
		if ( mntcheck[i] ) continue;
		sfs = &mntbuf[i];
		if ( fromname != NULL && strcmp( sfs->f_mntfromname, fromname ) != 0 ) continue;
		if ( onname != NULL && strcmp( sfs->f_mntonname, onname ) != 0 ) continue;
		if ( fsid != NULL && bcmp( &sfs->f_fsid, fsid, sizeof( *fsid ) ) != 0 ) continue;
		switch ( what ) {
			case CHECKUNIQUE:
				foundsfs = sfs;
				count++;
				continue;
			case REMOVE:
				mntcheck[i] = 1;
				break;
			default:
				break;
		}
		return ( sfs );
	}
	if ( what == CHECKUNIQUE && count == 1 ) return ( foundsfs );
	return ( NULL );
}
int sacmp( void *sa1, void *sa2 ) {
	void *p1, *p2;
	int len;
	if ( ( (struct sockaddr *) sa1 )->sa_family != ( (struct sockaddr *) sa2 )->sa_family ) return ( 1 );
	switch ( ( (struct sockaddr *) sa1 )->sa_family ) {
		case AF_INET:
			p1 = &( (struct sockaddr_in *) sa1 )->sin_addr;
			p2 = &( (struct sockaddr_in *) sa2 )->sin_addr;
			len = 4;
			break;
		case AF_INET6:
			p1 = &( (struct sockaddr_in6 *) sa1 )->sin6_addr;
			p2 = &( (struct sockaddr_in6 *) sa2 )->sin6_addr;
			len = 16;
			if ( ( (struct sockaddr_in6 *) sa1 )->sin6_scope_id != ( (struct sockaddr_in6 *) sa2 )->sin6_scope_id ) return ( 1 );
			break;
		default:
			return ( 1 );
	}
	return memcmp( p1, p2, len );
}
int namematch( struct addrinfo *ai ) {
	struct addrinfo *aip;
	if ( nfshost == NULL || nfshost_ai == NULL ) return ( 1 );
	while ( ai != NULL ) {
		aip = nfshost_ai;
		while ( aip != NULL ) {
			if ( sacmp( ai->ai_addr, aip->ai_addr ) == 0 ) return ( 1 );
			aip = aip->ai_next;
		}
		ai = ai->ai_next;
	}
	return ( 0 );
}
struct statfs *
checkmntlist( char *mntname ) {
	struct statfs *sfs;
	fsid_t fsid;
	sfs = NULL;
	if ( parsehexfsid( mntname, &fsid ) == 0 ) sfs = getmntentry( NULL, NULL, &fsid, FIND );
	if ( sfs == NULL ) sfs = getmntentry( NULL, mntname, NULL, FIND );
	if ( sfs == NULL ) sfs = getmntentry( mntname, NULL, NULL, FIND );
	return ( sfs );
}
size_t mntinfo( struct statfs **mntbuf ) {
	static struct statfs *origbuf;
	size_t bufsize;
	int mntsize;
	mntsize = getfsstat( NULL, 0, MNT_NOWAIT );
	if ( mntsize <= 0 ) return ( 0 );
	bufsize = ( mntsize + 1 ) * sizeof(struct statfs);
	if ( ( origbuf = malloc( bufsize ) ) == NULL ) err( 1, "malloc" );
	mntsize = getfsstat( origbuf, (long) bufsize, MNT_NOWAIT );
	*mntbuf = origbuf;
	return ( mntsize );
}
int parsehexfsid( const char *hex, fsid_t *fsid ) {
	char hexbuf[3];
	int i;
	if ( strlen( hex ) != sizeof( *fsid ) * 2 ) return ( -1 );
	hexbuf[2] = '\0';
	for ( i = 0; i < (int) sizeof( *fsid ); i++ ) {
		hexbuf[0] = hex[i * 2];
		hexbuf[1] = hex[i * 2 + 1];
		if ( !isxdigit( hexbuf[0] ) || !isxdigit( hexbuf[1] ) ) return ( -1 );
		( (u_char *) fsid )[i] = strtol( hexbuf, NULL, 16 );
	}
	return ( 0 );
}
int xdr_dir( XDR *xdrsp, char *dirp ) {
	return ( xdr_string( xdrsp, &dirp, MNTPATHLEN ) );
}
void usage( void ) {
	(void) fprintf( stderr, "%s\n%s\n", "usage: umount [-fv] special ... | node ... | fsid ...", "       umount -a | -A [-F fstab] [-fv] [-h host] [-t type]" );
	exit( 1 );
}
