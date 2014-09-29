#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/param.h>
#include <sys/mdioctl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <assert.h>
#include <err.h>
#include <fcntl.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
typedef enum {
false, true}bool;
struct mtpt_info {
	uid_t mi_uid;
	bool mi_have_uid;
	gid_t mi_gid;
	bool mi_have_gid;
	mode_t mi_mode;
	bool mi_have_mode;
	bool mi_forced_pw;
};
static bool debug;
static bool loudsubs;
static bool norun;
static int unit;
static const char *mdname;
static const char *mdsuffix;
static size_t mdnamelen;
static const char *path_mdconfig = _PATH_MDCONFIG;
static void argappend( char **, const char *, ... )
__printflike(2, 3);
static void debugprintf( const char *, ... )
__printflike(1, 2);
static void do_mdconfig_attach( const char *, const enum md_types );
static void do_mdconfig_attach_au( const char *, const enum md_types );
static void do_mdconfig_detach( void );
static void do_mount( const char *, const char * );
static void do_mtptsetup( const char *, struct mtpt_info * );
static void do_newfs( const char * );
static void extract_ugid( const char *, struct mtpt_info * );
static int run( int *, const char *, ... )
__printflike(2, 3);
static void usage( void );
int main( int argc, char **argv ) {
struct mtpt_info mi;
char *mdconfig_arg, *newfs_arg, *mount_arg;
enum md_types mdtype;
bool have_mdtype;
bool detach, softdep, autounit, newfs;
char *mtpoint, *unitstr;
char *p;
int ch;
void *set;
unsigned long ul;
(void) memset( &mi, '\0', sizeof( mi ) );
detach = true;
softdep = true;
autounit = false;
newfs = true;
have_mdtype = false;
mdtype = MD_SWAP;
mdname = MD_NAME;
mdnamelen = strlen( mdname );
mdconfig_arg = strdup( "" );
newfs_arg = strdup( "" );
mount_arg = strdup( "" );
if ( strcmp( getprogname(), "mount_mfs" ) == 0 || strcmp( getprogname(), "mfs" ) == 0 ) {
	mi.mi_mode = 01777;
	mi.mi_have_mode = true;
}
while ( ( ch = getopt( argc, argv, "a:b:Cc:Dd:E:e:F:f:hi:LlMm:NnO:o:Pp:Ss:tUv:w:X" ) ) != -1 )
	switch ( ch ) {
		case 'a':
			argappend( &newfs_arg, "-a %s", optarg );
			break;
		case 'b':
			argappend( &newfs_arg, "-b %s", optarg );
			break;
		case 'C':
			break;
		case 'c':
			argappend( &newfs_arg, "-c %s", optarg );
			break;
		case 'D':
			detach = false;
			break;
		case 'd':
			argappend( &newfs_arg, "-d %s", optarg );
			break;
		case 'E':
			path_mdconfig = optarg;
			break;
		case 'e':
			argappend( &newfs_arg, "-e %s", optarg );
			break;
		case 'F':
			if ( have_mdtype ) usage();
			mdtype = MD_VNODE;
			have_mdtype = true;
			argappend( &mdconfig_arg, "-f %s", optarg );
			break;
		case 'f':
			argappend( &newfs_arg, "-f %s", optarg );
			break;
		case 'h':
			usage();
			break;
		case 'i':
			argappend( &newfs_arg, "-i %s", optarg );
			break;
		case 'L':
			loudsubs = true;
			break;
		case 'l':
			argappend( &newfs_arg, "-l" );
			break;
		case 'M':
			if ( have_mdtype ) usage();
			mdtype = MD_MALLOC;
			have_mdtype = true;
			break;
		case 'm':
			argappend( &newfs_arg, "-m %s", optarg );
			break;
		case 'N':
			norun = true;
			break;
		case 'n':
			argappend( &newfs_arg, "-n" );
			break;
		case 'O':
			argappend( &newfs_arg, "-o %s", optarg );
			break;
		case 'o':
			argappend( &mount_arg, "-o %s", optarg );
			break;
		case 'P':
			newfs = false;
			break;
		case 'p':
			if ( ( set = setmode( optarg ) ) == NULL ) usage();
			mi.mi_mode = getmode( set, S_IRWXU | S_IRWXG | S_IRWXO );
			mi.mi_have_mode = true;
			mi.mi_forced_pw = true;
			free( set );
			break;
		case 'S':
			softdep = false;
			break;
		case 's':
			argappend( &mdconfig_arg, "-s %s", optarg );
			break;
		case 't':
			argappend( &newfs_arg, "-t" );
			break;
		case 'U':
			softdep = true;
			break;
		case 'v':
			argappend( &newfs_arg, "-O %s", optarg );
			break;
		case 'w':
			extract_ugid( optarg, &mi );
			mi.mi_forced_pw = true;
			break;
		case 'X':
			debug = true;
			break;
		default:
			usage();
	}
argc -= optind;
argv += optind;
if ( argc < 2 ) usage();
unitstr = argv[0];
if ( strncmp( unitstr, "/dev/", 5 ) == 0 ) unitstr += 5;
if ( strncmp( unitstr, mdname, mdnamelen ) == 0 ) unitstr += mdnamelen;
if ( !isdigit( *unitstr ) ) {
	autounit = true;
	unit = -1;
	mdsuffix = unitstr;
} else {
	ul = strtoul( unitstr, &p, 10 );
	if ( ul == ULONG_MAX ) errx( 1, "bad device unit: %s", unitstr );
	unit = ul;
	mdsuffix = p;
}
mtpoint = argv[1];
if ( !have_mdtype ) mdtype = MD_SWAP;
if ( softdep ) argappend( &newfs_arg, "-U" );
if ( mdtype != MD_VNODE && !newfs ) errx( 1, "-P requires a vnode-backed disk" );
if ( detach && !autounit ) do_mdconfig_detach();
if ( autounit ) do_mdconfig_attach_au( mdconfig_arg, mdtype );
else do_mdconfig_attach( mdconfig_arg, mdtype );
if ( newfs ) do_newfs( newfs_arg );
do_mount( mount_arg, mtpoint );
do_mtptsetup( mtpoint, &mi );
return ( 0 );
}
static void argappend( char **dstp, const char *fmt, ... ) {
char *old, *new;
va_list ap;
old = *dstp;
assert( old != NULL );
va_start( ap, fmt );
if ( vasprintf(&new, fmt,ap) == -1 ) errx( 1, "vasprintf" );
va_end( ap );
*dstp = new;
if ( asprintf(&new, "%s %s", old, new) == -1 ) errx( 1, "asprintf" );
free( *dstp );
free (old);
*dstp = new;
}
static void debugprintf( const char *fmt, ... ) {
va_list ap;
if ( !debug ) return;
fprintf( stderr, "DEBUG: " );
va_start( ap, fmt );
vfprintf( stderr, fmt, ap );
va_end( ap );
fprintf( stderr, "\n" );
fflush (stderr);
}
static void do_mdconfig_attach( const char *args, const enum md_types mdtype ) {
int rv;
const char *ta;
switch ( mdtype ) {
case MD_SWAP:
	ta = "-t swap";
	break;
case MD_VNODE:
	ta = "-t vnode";
	break;
case MD_MALLOC:
	ta = "-t malloc";
	break;
default:
	abort();
}
rv = run( NULL, "%s -a %s%s -u %s%d", path_mdconfig, ta, args, mdname, unit );
if ( rv ) errx( 1, "mdconfig (attach) exited with error code %d", rv );
}
static void do_mdconfig_attach_au( const char *args, const enum md_types mdtype ) {
const char *ta;
char *linep, *linebuf;
int fd;
FILE *sfd;
int rv;
char *p;
size_t linelen;
unsigned long ul;
switch ( mdtype ) {
case MD_SWAP:
	ta = "-t swap";
	break;
case MD_VNODE:
	ta = "-t vnode";
	break;
case MD_MALLOC:
	ta = "-t malloc";
	break;
default:
	abort();
}
rv = run( &fd, "%s -a %s%s", path_mdconfig, ta, args );
if ( rv ) errx( 1, "mdconfig (attach) exited with error code %d", rv );
if ( norun ) {
unit = 0;
return;
}
sfd = fdopen( fd, "r" );
if ( sfd == NULL ) err( 1, "fdopen" );
linep = fgetln( sfd, &linelen );
if ( linep == NULL && linelen < mdnamelen + 1 ) errx( 1, "unexpected output from mdconfig (attach)" );
assert( strncmp( linep, mdname, mdnamelen ) == 0 );
linebuf = malloc( linelen - mdnamelen + 1 );
assert( linebuf != NULL );
strncpy( linebuf, linep + mdnamelen, linelen );
linebuf[linelen] = '\0';
ul = strtoul( linebuf, &p, 10 );
if ( ul == ULONG_MAX || *p != '\n' ) errx( 1, "unexpected output from mdconfig (attach)" );
unit = ul;
fclose( sfd );
close( fd );
}
static void do_mdconfig_detach( void ) {
int rv;
rv = run( NULL, "%s -d -u %s%d", path_mdconfig, mdname, unit );
if ( rv && debug ) warnx( "mdconfig (detach) exited with error code %d (ignored)", rv );
}
static void do_mount( const char *args, const char *mtpoint ) {
int rv;
rv = run( NULL, "%s%s /dev/%s%d%s %s", _PATH_MOUNT, args, mdname, unit, mdsuffix, mtpoint );
if ( rv ) errx( 1, "mount exited with error code %d", rv );
}
static void do_mtptsetup( const char *mtpoint, struct mtpt_info *mip ) {
struct statfs sfs;
if ( !mip->mi_have_mode && !mip->mi_have_uid && !mip->mi_have_gid ) return;
if ( !norun ) {
if ( statfs( mtpoint, &sfs ) == -1 ) {
	warn( "statfs: %s", mtpoint );
	return;
}
if ( ( sfs.f_flags & MNT_RDONLY ) != 0 ) {
	if ( mip->mi_forced_pw ) {
		warnx( "Not changing mode/owner of %s since it is read-only", mtpoint );
	} else {
		debugprintf( "Not changing mode/owner of %s since it is read-only", mtpoint );
	}
	return;
}
}
if ( mip->mi_have_mode ) {
debugprintf( "changing mode of %s to %o.", mtpoint, mip->mi_mode );
if ( !norun ) if ( chmod( mtpoint, mip->mi_mode ) == -1 ) err( 1, "chmod: %s", mtpoint );
}
if ( mip->mi_have_uid ) {
debugprintf( "changing owner (user) or %s to %u.", mtpoint, mip->mi_uid );
if ( !norun ) if ( chown( mtpoint, mip->mi_uid, -1 ) == -1 ) err( 1, "chown %s to %u (user)", mtpoint, mip->mi_uid );
}
if ( mip->mi_have_gid ) {
debugprintf( "changing owner (group) or %s to %u.", mtpoint, mip->mi_gid );
if ( !norun ) if ( chown( mtpoint, -1, mip->mi_gid ) == -1 ) err( 1, "chown %s to %u (group)", mtpoint, mip->mi_gid );
}
}
static void do_newfs( const char *args ) {
int rv;
rv = run( NULL, "%s%s /dev/%s%d", _PATH_NEWFS, args, mdname, unit );
if ( rv ) errx( 1, "newfs exited with error code %d", rv );
}
static void extract_ugid( const char *str, struct mtpt_info *mip ) {
char *ug;
char *user, *group;
struct passwd *pw;
struct group *gr;
char *p;
uid_t *uid;
gid_t *gid;
uid = &mip->mi_uid;
gid = &mip->mi_gid;
mip->mi_have_uid = mip->mi_have_gid = false;
ug = strdup( str );
assert( ug != NULL );
group = ug;
user = strsep( &group, ":" );
if ( user == NULL || group == NULL || *user == '\0' || *group == '\0' ) usage();
*uid = strtoul( user, &p, 10 );
if ( *uid == (uid_t) ULONG_MAX ) usage();
if ( *p != '\0' ) {
pw = getpwnam( user );
if ( pw == NULL ) errx( 1, "invalid user: %s", user );
*uid = pw->pw_uid;
}
mip->mi_have_uid = true;
*gid = strtoul( group, &p, 10 );
if ( *gid == (gid_t) ULONG_MAX ) usage();
if ( *p != '\0' ) {
gr = getgrnam( group );
if ( gr == NULL ) errx( 1, "invalid group: %s", group );
*gid = gr->gr_gid;
}
mip->mi_have_gid = true;
free( ug );
}
static int run( int *ofd, const char *cmdline, ... ) {
char **argv, **argvp;
int argc;
char *cmd;
int pid, status;
int pfd[2];
int nfd;
bool dup2dn;
va_list ap;
char *p;
int rv, i;
dup2dn = true;
va_start( ap, cmdline );
rv = vasprintf( &cmd, cmdline, ap );
if ( rv == -1 ) err( 1, "vasprintf" );
va_end( ap );
for ( argc = 1, p = cmd; ( p = strchr( p, ' ' ) ) != NULL; p++ )
argc++;
argv = (char **) malloc( sizeof( *argv ) * ( argc + 1 ) );
assert( argv != NULL );
for ( p = cmd, argvp = argv; ( *argvp = strsep( &p, " " ) ) != NULL; )
if ( **argvp != '\0' ) if ( ++argvp >= &argv[argc] ) {
	*argvp = NULL;
	break;
}
assert( *argv );
if ( debug ) {
(void) fprintf( stderr, "DEBUG: running:" );
for ( i = 0; argv[i] != NULL; i++ )
	(void) fprintf( stderr, " %s", argv[i] );
(void) fprintf( stderr, "\n" );
}
if ( ofd != NULL ) {
if ( pipe( &pfd[0] ) == -1 ) err( 1, "pipe" );
*ofd = pfd[0];
dup2dn = false;
}
pid = fork();
switch ( pid ) {
case 0:
	if ( norun ) _exit( 0 );
	if ( ofd != NULL ) if ( dup2( pfd[1], STDOUT_FILENO ) < 0 ) err( 1, "dup2" );
	if ( !loudsubs ) {
		nfd = open( _PATH_DEVNULL, O_RDWR );
		if ( nfd == -1 ) err( 1, "open: %s", _PATH_DEVNULL );
		if ( dup2( nfd, STDIN_FILENO ) < 0 ) err( 1, "dup2" );
		if ( dup2dn ) if ( dup2( nfd, STDOUT_FILENO ) < 0 ) err( 1, "dup2" );
		if ( dup2( nfd, STDERR_FILENO ) < 0 ) err( 1, "dup2" );
	}
	(void) execv( argv[0], argv );
	warn( "exec: %s", argv[0] );
	_exit( -1 );
case -1:
	err( 1, "fork" );
}
free( cmd );
free( argv );
while ( waitpid( pid, &status, 0 ) != pid )
;
return ( WEXITSTATUS( status ) );
}
static void usage( void ) {
fprintf( stderr, "usage: %s [-DLlMNnPStUX] [-a maxcontig] [-b block-size]\n"
	"\t[-c blocks-per-cylinder-group][-d max-extent-size] [-E path-mdconfig]\n"
	"\t[-e maxbpg] [-F file] [-f frag-size] [-i bytes] [-m percent-free]\n"
	"\t[-O optimization] [-o mount-options]\n"
	"\t[-p permissions] [-s size] [-v version] [-w user:group]\n"
	"\tmd-device mount-point\n", getprogname() );
exit( 1 );
}
