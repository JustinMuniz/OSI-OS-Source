#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int may_have_nfs4acl( const FTSENT *ent, int hflag );
int main( int argc, char *argv[] ) {
	FTS *ftsp;
	FTSENT *p;
	mode_t *set;
	int Hflag, Lflag, Rflag, ch, error, fflag, fts_options, hflag, rval;
	int vflag;
	char *mode;
	mode_t newmode;
	set = NULL;
	Hflag = Lflag = Rflag = fflag = hflag = vflag = 0;
	while ( ( ch = getopt( argc, argv, "HLPRXfghorstuvwx" ) ) != -1 )
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
			case 'g':
			case 'o':
			case 'r':
			case 's':
			case 't':
			case 'u':
			case 'w':
			case 'X':
			case 'x':
				if ( argv[optind - 1][0] == '-' && argv[optind - 1][1] == ch && argv[optind - 1][2] == '\0' ) --optind;
				goto done;
			case 'v':
				vflag++;
				break;
			case '?':
			default:
				exit( 1 );
		}
	done: argv += optind;
	argc -= optind;
	if ( argc < 2 ) exit( 1 );
	if ( Rflag ) {
		fts_options = FTS_PHYSICAL;
		if ( hflag ) errx( 1, "the -R and -h options may not be specified together." );
		if ( Hflag ) fts_options |= FTS_COMFOLLOW;
		if ( Lflag ) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else fts_options = hflag ? FTS_PHYSICAL : FTS_LOGICAL;
	mode = *argv;
	if ( ( set = setmode( mode ) ) == NULL ) errx( 1, "invalid file mode: %s", mode );
	if ( ( ftsp = fts_open( ++argv, fts_options, 0 ) ) == NULL ) err( 1, "fts_open" );
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
		newmode = getmode( set, p->fts_statp->st_mode );
		if ( may_have_nfs4acl( p, hflag ) == 0 && ( newmode & ALLPERMS ) == ( p->fts_statp->st_mode & ALLPERMS ) ) continue;
		if ( hflag ) error = lchmod( p->fts_accpath, newmode );
		else error = chmod( p->fts_accpath, newmode );
		if ( error ) {
			if ( !fflag ) {
				warn( "%s", p->fts_path );
				rval = 1;
			}
		} else {
			if ( vflag ) {
				(void) printf( "%s", p->fts_path );
				if ( vflag > 1 ) {
					char m1[12], m2[12];
					strmode( p->fts_statp->st_mode, m1 );
					strmode( ( p->fts_statp->st_mode & S_IFMT ) | newmode, m2 );
					(void) printf( ": 0%o [%s] -> 0%o [%s]", p->fts_statp->st_mode, m1, ( p->fts_statp->st_mode & S_IFMT ) | newmode, m2 );
				}
				(void) printf( "\n" );
			}
		}
	}
	if ( errno ) err( 1, "fts_read" );
	exit( rval );
}
static int may_have_nfs4acl( const FTSENT *ent, int hflag ) {
	int ret;
	static dev_t previous_dev = NODEV;
	static int supports_acls = -1;
	if ( previous_dev != ent->fts_statp->st_dev ) {
		previous_dev = ent->fts_statp->st_dev;
		supports_acls = 0;
		if ( hflag ) ret = lpathconf( ent->fts_accpath, _PC_ACL_NFS4 );
		else ret = pathconf( ent->fts_accpath, _PC_ACL_NFS4 );
		if ( ret > 0 ) supports_acls = 1;
		else if ( ret < 0 && errno != EINVAL ) warn( "%s", ent->fts_path );
	}
	return ( supports_acls );
}
