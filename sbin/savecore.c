#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/disk.h>
#include <sys/kerneldump.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#define	BUFFERSIZE	(1024*1024)
#define	STATUS_BAD	0
#define	STATUS_GOOD	1
#define	STATUS_UNKNOWN	2
static int checkfor, compress, clear, force, keep, verbose;
static int nfound, nsaved, nerr;
static int maxdumps;
extern FILE *zopen( const char *, const char * );
static sig_atomic_t got_siginfo;
static void infohandler( int );
static void printheader( FILE *f, const struct kerneldumpheader *h, const char *device, int bounds, const int status ) {
	uint64_t dumplen;
	time_t t;
	const char *stat_str;
	fprintf( f, "Dump header from device %s\n", device );
	fprintf( f, "  Architecture: %s\n", h->architecture );
	fprintf( f, "  Architecture Version: %u\n", dtoh32( h->architectureversion ) );
	dumplen = dtoh64( h->dumplength );
	fprintf( f, "  Dump Length: %lldB (%lld MB)\n", (long long) dumplen, (long long) ( dumplen >> 20 ) );
	fprintf( f, "  Blocksize: %d\n", dtoh32( h->blocksize ) );
	t = dtoh64( h->dumptime );
	fprintf( f, "  Dumptime: %s", ctime( &t ) );
	fprintf( f, "  Hostname: %s\n", h->hostname );
	fprintf( f, "  Magic: %s\n", h->magic );
	fprintf( f, "  Version String: %s", h->versionstring );
	fprintf( f, "  Panic String: %s\n", h->panicstring );
	fprintf( f, "  Dump Parity: %u\n", h->parity );
	fprintf( f, "  Bounds: %d\n", bounds );
	switch ( status ) {
		case STATUS_BAD:
			stat_str = "bad";
			break;
		case STATUS_GOOD:
			stat_str = "good";
			break;
		default:
			stat_str = "unknown";
	}
	fprintf( f, "  Dump Status: %s\n", stat_str );
	fflush( f );
}
static int getbounds( void ) {
	FILE *fp;
	char buf[6];
	int ret;
	ret = 0;
	if ( ( fp = fopen( "bounds", "r" ) ) == NULL ) {
		if ( verbose ) printf( "unable to open bounds file, using 0\n" );
		return ( ret );
	}
	if ( fgets( buf, sizeof buf, fp ) == NULL ) {
		syslog( LOG_WARNING, "unable to read from bounds, using 0" );
		fclose( fp );
		return ( ret );
	}
	errno = 0;
	ret = (int) strtol( buf, NULL, 10 );
	if ( ret == 0 && ( errno == EINVAL || errno == ERANGE ) ) syslog( LOG_WARNING, "invalid value found in bounds, using 0" );
	return ( ret );
}
static void writebounds( int bounds ) {
	FILE *fp;
	if ( ( fp = fopen( "bounds", "w" ) ) == NULL ) {
		syslog( LOG_WARNING, "unable to write to bounds file: %m" );
		return;
	}
	if ( verbose ) printf( "bounds number: %d\n", bounds );
	fprintf( fp, "%d\n", bounds );
	fclose( fp );
}
static off_t file_size( const char *path ) {
	struct stat sb;
	if ( stat( path, &sb ) == -1 ) return ( 0 );
	return ( sb.st_size );
}
static off_t saved_dump_size( int bounds ) {
	static char path[PATH_MAX];
	off_t dumpsize;
	dumpsize = 0;
	(void) snprintf( path, sizeof( path ), "info.%d", bounds );
	dumpsize += file_size( path );
	(void) snprintf( path, sizeof( path ), "vmcore.%d", bounds );
	dumpsize += file_size( path );
	(void) snprintf( path, sizeof( path ), "vmcore.%d.gz", bounds );
	dumpsize += file_size( path );
	(void) snprintf( path, sizeof( path ), "textdump.tar.%d", bounds );
	dumpsize += file_size( path );
	(void) snprintf( path, sizeof( path ), "textdump.tar.%d.gz", bounds );
	dumpsize += file_size( path );
	return ( dumpsize );
}
static void saved_dump_remove( int bounds ) {
	static char path[PATH_MAX];
	(void) snprintf( path, sizeof( path ), "info.%d", bounds );
	(void) unlink( path );
	(void) snprintf( path, sizeof( path ), "vmcore.%d", bounds );
	(void) unlink( path );
	(void) snprintf( path, sizeof( path ), "vmcore.%d.gz", bounds );
	(void) unlink( path );
	(void) snprintf( path, sizeof( path ), "textdump.tar.%d", bounds );
	(void) unlink( path );
	(void) snprintf( path, sizeof( path ), "textdump.tar.%d.gz", bounds );
	(void) unlink( path );
}
static void symlinks_remove( void ) {
	(void) unlink( "info.last" );
	(void) unlink( "vmcore.last" );
	(void) unlink( "vmcore.last.gz" );
	(void) unlink( "textdump.tar.last" );
	(void) unlink( "textdump.tar.last.gz" );
}
static int check_space( const char *savedir, off_t dumpsize, int bounds ) {
	FILE *fp;
	off_t minfree, spacefree, totfree, needed;
	struct statfs fsbuf;
	char buf[100];
	if ( statfs( ".", &fsbuf ) < 0 ) {
		syslog( LOG_ERR, "%s: %m", savedir );
		exit( 1 );
	}
	spacefree = ( (off_t) fsbuf.f_bavail * fsbuf.f_bsize ) / 1024;
	totfree = ( (off_t) fsbuf.f_bfree * fsbuf.f_bsize ) / 1024;
	if ( ( fp = fopen( "minfree", "r" ) ) == NULL ) minfree = 0;
	else {
		if ( fgets( buf, sizeof( buf ), fp ) == NULL ) minfree = 0;
		else minfree = atoi( buf );
		(void) fclose( fp );
	}
	needed = dumpsize / 1024 + 2;
	needed -= saved_dump_size( bounds );
	if ( ( minfree > 0 ? spacefree : totfree ) - needed < minfree ) {
		syslog( LOG_WARNING, "no dump, not enough free space on device (%lld available, need %lld)", (long long) ( minfree > 0 ? spacefree : totfree ), (long long) needed );
		return ( 0 );
	}
	if ( spacefree - needed < 0 ) syslog( LOG_WARNING, "dump performed, but free space threshold crossed" );
	return ( 1 );
}
#define BLOCKSIZE (1<<12)
#define BLOCKMASK (~(BLOCKSIZE-1))
static int DoRegularFile( int fd, off_t dumpsize, char *buf, const char *device, const char *filename, FILE *fp ) {
	int he, hs, nr, nw, wl;
	off_t dmpcnt, origsize;
	dmpcnt = 0;
	origsize = dumpsize;
	he = 0;
	while ( dumpsize > 0 ) {
		wl = BUFFERSIZE;
		if ( wl > dumpsize ) wl = dumpsize;
		nr = read( fd, buf, wl );
		if ( nr != wl ) {
			if ( nr == 0 ) syslog( LOG_WARNING, "WARNING: EOF on dump device" );
			else syslog( LOG_ERR, "read error on %s: %m", device );
			nerr++;
			return ( -1 );
		}
		if ( compress ) {
			nw = fwrite( buf, 1, wl, fp );
		} else {
			for ( nw = 0; nw < nr; nw = he ) {
				for ( hs = nw; hs < nr; hs += BLOCKSIZE ) {
					for ( he = hs; he < nr && buf[he] == 0; ++he )
						;
					if ( he >= hs + BLOCKSIZE ) break;
				}
				he &= BLOCKMASK;
				if ( hs + BLOCKSIZE > nr ) hs = he = nr;
				if ( hs > nw ) if ( fwrite( buf + nw, hs - nw, 1, fp ) != 1 ) break;
				if ( he > hs ) if ( fseeko( fp, he - hs, SEEK_CUR ) == -1 ) break;
			}
		}
		if ( nw != wl ) {
			syslog( LOG_ERR, "write error on %s file: %m", filename );
			syslog( LOG_WARNING, "WARNING: vmcore may be incomplete" );
			nerr++;
			return ( -1 );
		}
		if ( verbose ) {
			dmpcnt += wl;
			printf( "%llu\r", (unsigned long long) dmpcnt );
			fflush (stdout);
		}
		dumpsize -= wl;
		if ( got_siginfo ) {
			printf( "%s %.1lf%%\n", filename, ( 100.0 - ( 100.0 * (double) dumpsize / (double) origsize ) ) );
			got_siginfo = 0;
		}
	}
	return ( 0 );
}
static int DoTextdumpFile( int fd, off_t dumpsize, off_t lasthd, char *buf, const char *device, const char *filename, FILE *fp ) {
	int nr, nw, wl;
	off_t dmpcnt, totsize;
	totsize = dumpsize;
	dmpcnt = 0;
	wl = 512;
	if ( ( dumpsize % wl ) != 0 ) {
		syslog( LOG_ERR, "textdump uneven multiple of 512 on %s", device );
		nerr++;
		return ( -1 );
	}
	while ( dumpsize > 0 ) {
		nr = pread( fd, buf, wl, lasthd - ( totsize - dumpsize ) - wl );
		if ( nr != wl ) {
			if ( nr == 0 ) syslog( LOG_WARNING, "WARNING: EOF on dump device" );
			else syslog( LOG_ERR, "read error on %s: %m", device );
			nerr++;
			return ( -1 );
		}
		nw = fwrite( buf, 1, wl, fp );
		if ( nw != wl ) {
			syslog( LOG_ERR, "write error on %s file: %m", filename );
			syslog( LOG_WARNING, "WARNING: textdump may be incomplete" );
			nerr++;
			return ( -1 );
		}
		if ( verbose ) {
			dmpcnt += wl;
			printf( "%llu\r", (unsigned long long) dmpcnt );
			fflush (stdout);
		}
		dumpsize -= wl;
	}
	return ( 0 );
}
static void DoFile( const char *savedir, const char *device ) {
	static char infoname[PATH_MAX], corename[PATH_MAX], linkname[PATH_MAX];
	static char *buf = NULL;
	struct kerneldumpheader kdhf, kdhl;
	off_t mediasize, dumpsize, firsthd, lasthd;
	FILE *info, *fp;
	mode_t oumask;
	int fd, fdinfo, error;
	int bounds, status;
	u_int sectorsize;
	int istextdump;
	bounds = getbounds();
	mediasize = 0;
	status = STATUS_UNKNOWN;
	if ( maxdumps > 0 && bounds == maxdumps ) bounds = 0;
	if ( buf == NULL ) {
		buf = malloc( BUFFERSIZE );
		if ( buf == NULL ) {
			syslog( LOG_ERR, "%m" );
			return;
		}
	}
	if ( verbose ) printf( "checking for kernel dump on device %s\n", device );
	fd = open( device, ( checkfor || keep ) ? O_RDONLY : O_RDWR );
	if ( fd < 0 ) {
		syslog( LOG_ERR, "%s: %m", device );
		return;
	}
	error = ioctl( fd, DIOCGMEDIASIZE, &mediasize );
	if ( !error ) error = ioctl( fd, DIOCGSECTORSIZE, &sectorsize );
	if ( error ) {
		syslog( LOG_ERR, "couldn't find media and/or sector size of %s: %m", device );
		goto closefd;
	}
	if ( verbose ) {
		printf( "mediasize = %lld\n", (long long) mediasize );
		printf( "sectorsize = %u\n", sectorsize );
	}
	lasthd = mediasize - sectorsize;
	lseek( fd, lasthd, SEEK_SET );
	error = read( fd, &kdhl, sizeof kdhl );
	if ( error != sizeof kdhl ) {
		syslog( LOG_ERR, "error reading last dump header at offset %lld in %s: %m", (long long) lasthd, device );
		goto closefd;
	}
	istextdump = 0;
	if ( strncmp( kdhl.magic, TEXTDUMPMAGIC, sizeof kdhl ) == 0 ) {
		if ( verbose ) printf( "textdump magic on last dump header on %s\n", device );
		istextdump = 1;
		if ( dtoh32( kdhl.version ) != KERNELDUMP_TEXT_VERSION ) {
			syslog( LOG_ERR, "unknown version (%d) in last dump header on %s", dtoh32( kdhl.version ), device );
			status = STATUS_BAD;
			if ( force == 0 ) goto closefd;
		}
	} else if ( memcmp( kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic ) == 0 ) {
		if ( dtoh32( kdhl.version ) != KERNELDUMPVERSION ) {
			syslog( LOG_ERR, "unknown version (%d) in last dump header on %s", dtoh32( kdhl.version ), device );
			status = STATUS_BAD;
			if ( force == 0 ) goto closefd;
		}
	} else {
		if ( verbose ) printf( "magic mismatch on last dump header on %s\n", device );
		status = STATUS_BAD;
		if ( force == 0 ) goto closefd;
		if ( memcmp( kdhl.magic, KERNELDUMPMAGIC_CLEARED, sizeof kdhl.magic ) == 0 ) {
			if ( verbose ) printf( "forcing magic on %s\n", device );
			memcpy( kdhl.magic, KERNELDUMPMAGIC, sizeof kdhl.magic );
		} else {
			syslog( LOG_ERR, "unable to force dump - bad magic" );
			goto closefd;
		}
		if ( dtoh32( kdhl.version ) != KERNELDUMPVERSION ) {
			syslog( LOG_ERR, "unknown version (%d) in last dump header on %s", dtoh32( kdhl.version ), device );
			status = STATUS_BAD;
			if ( force == 0 ) goto closefd;
		}
	}
	nfound++;
	if ( clear ) goto nuke;
	if ( kerneldump_parity( &kdhl ) ) {
		syslog( LOG_ERR, "parity error on last dump header on %s", device );
		nerr++;
		status = STATUS_BAD;
		if ( force == 0 ) goto closefd;
	}
	dumpsize = dtoh64( kdhl.dumplength );
	firsthd = lasthd - dumpsize - sizeof kdhf;
	lseek( fd, firsthd, SEEK_SET );
	error = read( fd, &kdhf, sizeof kdhf );
	if ( error != sizeof kdhf ) {
		syslog( LOG_ERR, "error reading first dump header at offset %lld in %s: %m", (long long) firsthd, device );
		nerr++;
		goto closefd;
	}
	if ( verbose >= 2 ) {
		printf( "First dump headers:\n" );
		printheader( stdout, &kdhf, device, bounds, -1 );
		printf( "\nLast dump headers:\n" );
		printheader( stdout, &kdhl, device, bounds, -1 );
		printf( "\n" );
	}
	if ( memcmp( &kdhl, &kdhf, sizeof kdhl ) ) {
		syslog( LOG_ERR, "first and last dump headers disagree on %s", device );
		nerr++;
		status = STATUS_BAD;
		if ( force == 0 ) goto closefd;
	} else {
		status = STATUS_GOOD;
	}
	if ( checkfor ) {
		printf( "A dump exists on %s\n", device );
		close( fd );
		exit( 0 );
	}
	if ( kdhl.panicstring[0] ) syslog( LOG_ALERT, "reboot after panic: %s", kdhl.panicstring );
	else syslog( LOG_ALERT, "reboot" );
	if ( verbose ) printf( "Checking for available free space\n" );
	if ( !check_space( savedir, dumpsize, bounds ) ) {
		nerr++;
		goto closefd;
	}
	writebounds( bounds + 1 );
	saved_dump_remove( bounds );
	snprintf( infoname, sizeof( infoname ), "info.%d", bounds );
	fdinfo = open( infoname, O_WRONLY | O_CREAT | O_TRUNC, 0600 );
	if ( fdinfo < 0 ) {
		syslog( LOG_ERR, "%s: %m", buf );
		nerr++;
		goto closefd;
	}
	oumask = umask( S_IRWXG | S_IRWXO );
	if ( compress ) {
		snprintf( corename, sizeof( corename ), "%s.%d.gz", istextdump ? "textdump.tar" : "vmcore", bounds );
		fp = zopen( corename, "w" );
	} else {
		snprintf( corename, sizeof( corename ), "%s.%d", istextdump ? "textdump.tar" : "vmcore", bounds );
		fp = fopen( corename, "w" );
	}
	if ( fp == NULL ) {
		syslog( LOG_ERR, "%s: %m", corename );
		close( fdinfo );
		nerr++;
		goto closefd;
	}
	(void) umask( oumask );
	info = fdopen( fdinfo, "w" );
	if ( info == NULL ) {
		syslog( LOG_ERR, "fdopen failed: %m" );
		nerr++;
		goto closefd;
	}
	if ( verbose ) printheader( stdout, &kdhl, device, bounds, status );
	printheader( info, &kdhl, device, bounds, status );
	fclose( info );
	syslog( LOG_NOTICE, "writing %score to %s/%s", compress ? "compressed " : "", savedir, corename );
	if ( istextdump ) {
		if ( DoTextdumpFile( fd, dumpsize, lasthd, buf, device, corename, fp ) < 0 ) goto closeall;
	} else {
		if ( DoRegularFile( fd, dumpsize, buf, device, corename, fp ) < 0 ) goto closeall;
	}
	if ( verbose ) printf( "\n" );
	if ( fclose( fp ) < 0 ) {
		syslog( LOG_ERR, "error on %s: %m", corename );
		nerr++;
		goto closeall;
	}
	symlinks_remove();
	if ( symlink( infoname, "info.last" ) == -1 ) {
		syslog( LOG_WARNING, "unable to create symlink %s/%s: %m", savedir, "info.last" );
	}
	if ( compress ) {
		snprintf( linkname, sizeof( linkname ), "%s.last.gz", istextdump ? "textdump.tar" : "vmcore" );
	} else {
		snprintf( linkname, sizeof( linkname ), "%s.last", istextdump ? "textdump.tar" : "vmcore" );
	}
	if ( symlink( corename, linkname ) == -1 ) {
		syslog( LOG_WARNING, "unable to create symlink %s/%s: %m", savedir, linkname );
	}
	nsaved++;
	if ( verbose ) printf( "dump saved\n" );
	nuke: if ( !keep ) {
		if ( verbose ) printf( "clearing dump header\n" );
		memcpy( kdhl.magic, KERNELDUMPMAGIC_CLEARED, sizeof kdhl.magic );
		lseek( fd, lasthd, SEEK_SET );
		error = write( fd, &kdhl, sizeof kdhl );
		if ( error != sizeof kdhl ) syslog( LOG_ERR, "error while clearing the dump header: %m" );
	}
	close( fd );
	return;
	closeall: fclose( fp );
	closefd: close( fd );
}
static void usage( void ) {
	fprintf( stderr, "%s\n%s\n%s\n", "usage: savecore -c [-v] [device ...]", "       savecore -C [-v] [device ...]", "       savecore [-fkvz] [-m maxdumps] [directory [device ...]]" );
	exit( 1 );
}
int main( int argc, char **argv ) {
	const char *savedir = ".";
	struct fstab *fsp;
	int i, ch, error;
	checkfor = compress = clear = force = keep = verbose = 0;
	nfound = nsaved = nerr = 0;
	openlog( "savecore", LOG_PERROR, LOG_DAEMON );
	signal( SIGINFO, infohandler );
	while ( ( ch = getopt( argc, argv, "Ccfkm:vz" ) ) != -1 )
		switch ( ch ) {
			case 'C':
				checkfor = 1;
				break;
			case 'c':
				clear = 1;
				break;
			case 'f':
				force = 1;
				break;
			case 'k':
				keep = 1;
				break;
			case 'm':
				maxdumps = atoi( optarg );
				if ( maxdumps <= 0 ) {
					syslog( LOG_ERR, "Invalid maxdump value" );
					exit( 1 );
				}
				break;
			case 'v':
				verbose++;
				break;
			case 'z':
				compress = 1;
				break;
			case '?':
			default:
				usage();
		}
	if ( checkfor && ( clear || force || keep ) ) usage();
	if ( clear && ( compress || keep ) ) usage();
	if ( maxdumps > 0 && ( checkfor || clear ) ) usage();
	argc -= optind;
	argv += optind;
	if ( argc >= 1 && !checkfor && !clear ) {
		error = chdir( argv[0] );
		if ( error ) {
			syslog( LOG_ERR, "chdir(%s): %m", argv[0] );
			exit( 1 );
		}
		savedir = argv[0];
		argc--;
		argv++;
	}
	if ( argc == 0 ) {
		for ( ;; ) {
			fsp = getfsent();
			if ( fsp == NULL ) break;
			if ( strcmp( fsp->fs_vfstype, "swap" ) && strcmp( fsp->fs_vfstype, "dump" ) ) continue;
			DoFile( savedir, fsp->fs_spec );
		}
	} else {
		for ( i = 0; i < argc; i++ )
			DoFile( savedir, argv[i] );
	}
	if ( nfound == 0 ) {
		if ( checkfor ) {
			printf( "No dump exists\n" );
			exit( 1 );
		}
		syslog( LOG_WARNING, "no dumps found" );
	} else if ( nsaved == 0 ) {
		if ( nerr != 0 ) syslog( LOG_WARNING, "unsaved dumps found but not saved" );
		else syslog( LOG_WARNING, "no unsaved dumps found" );
	}
	return ( 0 );
}
static void
infohandler(int sig __unused)
{	
	got_siginfo = 1;
}
