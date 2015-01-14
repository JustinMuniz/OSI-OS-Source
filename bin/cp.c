#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <errno.h>
#include <fts.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {
		char *p_end;
		char *target_end;
		char p_path[PATH_MAX];
} PATH_T;
extern PATH_T to;
extern int fflag, iflag, lflag, nflag, pflag, vflag;
extern volatile sig_atomic_t info;
__BEGIN_DECLS
int copy_fifo( struct stat *, int );
int copy_file( const FTSENT *, int );
int copy_link( const FTSENT *, int );
int copy_special( struct stat *, int );
int setfile( struct stat *, int );
int preserve_dir_acls( struct stat *, char *, char * );
int preserve_fd_acls( int, int );
void usage( void );
__END_DECLS
#define	STRIP_TRAILING_SLASH(p) {					\
        while ((p).p_end > (p).p_path + 1 && (p).p_end[-1] == '/')	\
                *--(p).p_end = 0;					\
}
static char emptystring[] = "";
PATH_T to = { to.p_path, emptystring, "" };
int fflag, iflag, lflag, nflag, pflag, vflag;
static int Rflag, rflag;
volatile sig_atomic_t info;
enum op {
	FILE_TO_FILE, FILE_TO_DIR, DIR_TO_DNE
};
static int copy( char *[], enum op, int );
static int mastercmp( const FTSENT * const *, const FTSENT * const * );
static void siginfo( int __unused );
int main( int argc, char *argv[] ) {
	struct stat to_stat, tmp_stat;
	enum op type;
	int Hflag, Lflag, ch, fts_options, r, have_trailing_slash;
	char *target;
	fts_options = FTS_NOCHDIR | FTS_PHYSICAL;
	Hflag = Lflag = 0;
	while ( ( ch = getopt( argc, argv, "HLPRafilnprvx" ) ) != -1 )
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
			case 'a':
				pflag = 1;
				Rflag = 1;
				Hflag = Lflag = 0;
				break;
			case 'f':
				fflag = 1;
				iflag = nflag = 0;
				break;
			case 'i':
				iflag = 1;
				fflag = nflag = 0;
				break;
			case 'l':
				lflag = 1;
				break;
			case 'n':
				nflag = 1;
				fflag = iflag = 0;
				break;
			case 'p':
				pflag = 1;
				break;
			case 'r':
				rflag = Lflag = 1;
				Hflag = 0;
				break;
			case 'v':
				vflag = 1;
				break;
			case 'x':
				fts_options |= FTS_XDEV;
				break;
			default:
				usage();
				break;
		}
	argc -= optind;
	argv += optind;
	if ( argc < 2 ) usage();
	if ( Rflag && rflag ) errx( 1, "the -R and -r options may not be specified together" );
	if ( rflag ) Rflag = 1;
	if ( Rflag ) {
		if ( Hflag ) fts_options |= FTS_COMFOLLOW;
		if ( Lflag ) {
			fts_options &= ~FTS_PHYSICAL;
			fts_options |= FTS_LOGICAL;
		}
	} else {
		fts_options &= ~FTS_PHYSICAL;
		fts_options |= FTS_LOGICAL | FTS_COMFOLLOW;
	}
	(void) signal( SIGINFO, siginfo );
	target = argv[--argc];
	if ( strlcpy( to.p_path, target, sizeof( to.p_path ) ) >= sizeof( to.p_path ) ) errx( 1, "%s: name too long", target );
	to.p_end = to.p_path + strlen( to.p_path );
	if ( to.p_path == to.p_end ) {
		*to.p_end++ = '.';
		*to.p_end = 0;
	}
	have_trailing_slash = ( to.p_end[-1] == '/' );
	if ( have_trailing_slash )
	STRIP_TRAILING_SLASH( to );
	to.target_end = to.p_end;
	argv[argc] = NULL;
	r = stat( to.p_path, &to_stat );
	if ( r == -1 && errno != ENOENT ) err( 1, "%s", to.p_path );
	if ( r == -1 || !S_ISDIR( to_stat.st_mode ) ) {
		if ( argc > 1 ) errx( 1, "%s is not a directory", to.p_path );
		if ( r == -1 ) {
			if ( Rflag && ( Lflag || Hflag ) ) stat( *argv, &tmp_stat );
			else lstat( *argv, &tmp_stat );
			if ( S_ISDIR( tmp_stat.st_mode ) && Rflag ) type = DIR_TO_DNE;
			else type = FILE_TO_FILE;
		} else type = FILE_TO_FILE;
		if ( have_trailing_slash && type == FILE_TO_FILE ) {
			if ( r == -1 ) errx( 1, "directory %s does not exist", to.p_path );
			else errx( 1, "%s is not a directory", to.p_path );
		}
	} else type = FILE_TO_DIR;
	exit( copy( argv, type, fts_options ) );
}
static int copy( char *argv[], enum op type, int fts_options ) {
	struct stat to_stat;
	FTS *ftsp;
	FTSENT *curr;
	int base = 0, dne, badcp, rval;
	size_t nlen;
	char *p, *target_mid;
	mode_t mask, mode;
	mask = ~umask( 0777 );
	umask( ~mask );
	if ( ( ftsp = fts_open( argv, fts_options, mastercmp ) ) == NULL ) err( 1, "fts_open" );
	for ( badcp = rval = 0; ( curr = fts_read( ftsp ) ) != NULL; badcp = 0 ) {
		switch ( curr->fts_info ) {
			case FTS_NS:
			case FTS_DNR:
			case FTS_ERR:
				warnx( "%s: %s", curr->fts_path, strerror( curr->fts_errno ) );
				badcp = rval = 1;
				continue;
			case FTS_DC:
				warnx( "%s: directory causes a cycle", curr->fts_path );
				badcp = rval = 1;
				continue;
			default:
				;
		}
		if ( type != FILE_TO_FILE ) {
			if ( curr->fts_level == FTS_ROOTLEVEL ) {
				if ( type != DIR_TO_DNE ) {
					p = strrchr( curr->fts_path, '/' );
					base = ( p == NULL ) ? 0 : (int) ( p - curr->fts_path + 1 );
					if ( !strcmp( &curr->fts_path[base], ".." ) ) base += 1;
				} else base = curr->fts_pathlen;
			}
			p = &curr->fts_path[base];
			nlen = curr->fts_pathlen - base;
			target_mid = to.target_end;
			if ( *p != '/' && target_mid[-1] != '/' ) *target_mid++ = '/';
			*target_mid = 0;
			if ( target_mid - to.p_path + nlen >= PATH_MAX ) {
				warnx( "%s%s: name too long (not copied)", to.p_path, p );
				badcp = rval = 1;
				continue;
			}
			(void) strncat( target_mid, p, nlen );
			to.p_end = target_mid + nlen;
			*to.p_end = 0;
			STRIP_TRAILING_SLASH( to );
		}
		if ( curr->fts_info == FTS_DP ) {
			if ( !curr->fts_number ) continue;
			if ( pflag ) {
				if ( setfile( curr->fts_statp, -1 ) ) rval = 1;
				if ( preserve_dir_acls( curr->fts_statp, curr->fts_accpath, to.p_path ) != 0 ) rval = 1;
			} else {
				mode = curr->fts_statp->st_mode;
				if ( ( mode & ( S_ISUID | S_ISGID | S_ISTXT ) ) || ( ( mode | S_IRWXU ) & mask ) != ( mode & mask ) ) if ( chmod( to.p_path, mode & mask ) != 0 ) {
					warn( "chmod: %s", to.p_path );
					rval = 1;
				}
			}
			continue;
		}
		if ( stat( to.p_path, &to_stat ) == -1 ) dne = 1;
		else {
			if ( to_stat.st_dev == curr->fts_statp->st_dev && to_stat.st_ino == curr->fts_statp->st_ino ) {
				warnx( "%s and %s are identical (not copied).", to.p_path, curr->fts_path );
				badcp = rval = 1;
				if ( S_ISDIR( curr->fts_statp->st_mode ) ) (void) fts_set( ftsp, curr, FTS_SKIP );
				continue;
			}
			if ( !S_ISDIR( curr->fts_statp->st_mode ) && S_ISDIR( to_stat.st_mode ) ) {
				warnx( "cannot overwrite directory %s with "
						"non-directory %s", to.p_path, curr->fts_path );
				badcp = rval = 1;
				continue;
			}
			dne = 0;
		}
		switch ( curr->fts_statp->st_mode & S_IFMT ) {
			case S_IFLNK:
				if ( ( fts_options & FTS_LOGICAL ) || ( ( fts_options & FTS_COMFOLLOW ) && curr->fts_level == 0 ) ) {
					if ( copy_file( curr, dne ) ) badcp = rval = 1;
				} else {
					if ( copy_link( curr, !dne ) ) badcp = rval = 1;
				}
				break;
			case S_IFDIR:
				if ( !Rflag ) {
					warnx( "%s is a directory (not copied).", curr->fts_path );
					(void) fts_set( ftsp, curr, FTS_SKIP );
					badcp = rval = 1;
					break;
				}
				if ( dne ) {
					if ( mkdir( to.p_path, curr->fts_statp->st_mode | S_IRWXU ) < 0 ) err( 1, "%s", to.p_path );
				} else if ( !S_ISDIR( to_stat.st_mode ) ) {
					errno = ENOTDIR;
					err( 1, "%s", to.p_path );
				}
				curr->fts_number = pflag || dne;
				break;
			case S_IFBLK:
			case S_IFCHR:
				if ( Rflag ) {
					if ( copy_special( curr->fts_statp, !dne ) ) badcp = rval = 1;
				} else {
					if ( copy_file( curr, dne ) ) badcp = rval = 1;
				}
				break;
			case S_IFSOCK:
				warnx( "%s is a socket (not copied).", curr->fts_path );
				break;
			case S_IFIFO:
				if ( Rflag ) {
					if ( copy_fifo( curr->fts_statp, !dne ) ) badcp = rval = 1;
				} else {
					if ( copy_file( curr, dne ) ) badcp = rval = 1;
				}
				break;
			default:
				if ( copy_file( curr, dne ) ) badcp = rval = 1;
				break;
		}
		if ( vflag && !badcp ) (void) printf( "%s -> %s\n", curr->fts_path, to.p_path );
	}
	if ( errno ) err( 1, "fts_read" );
	fts_close( ftsp );
	return ( rval );
}
static int mastercmp( const FTSENT * const *a, const FTSENT * const *b ) {
	int a_info, b_info;
	a_info = ( *a )->fts_info;
	if ( a_info == FTS_ERR || a_info == FTS_NS || a_info == FTS_DNR ) return ( 0 );
	b_info = ( *b )->fts_info;
	if ( b_info == FTS_ERR || b_info == FTS_NS || b_info == FTS_DNR ) return ( 0 );
	if ( a_info == FTS_D ) return ( -1 );
	if ( b_info == FTS_D ) return ( 1 );
	return ( 0 );
}
static void
siginfo(int sig __unused)
{	
	info = 1;
}
#ifndef lint
#if 0
static char sccsid[] = "@(#)utils.c	8.3 (Berkeley) 4/1/94";
#endif
#endif 
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <sys/types.h>
#include <sys/acl.h>
#include <sys/param.h>
#include <sys/stat.h>
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
#include <sys/mman.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>
#include "extern.h"
#define	cp_pct(x, y)	((y == 0) ? 0 : (int)(100.0 * (x) / (y)))
#define PHYSPAGES_THRESHOLD (32*1024)
#define BUFSIZE_MAX (2*1024*1024)
#define BUFSIZE_SMALL (MAXPHYS)
int copy_file( const FTSENT *entp, int dne ) {
	static char *buf = NULL;
	static size_t bufsize;
	struct stat *fs;
	ssize_t wcount;
	size_t wresid;
	off_t wtotal;
	int ch, checkch, from_fd = 0, rcount, rval, to_fd = 0;
	char *bufp;
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
	char *p;
#endif
	if ( ( from_fd = open( entp->fts_path, O_RDONLY, 0 ) ) == -1 ) {
		warn( "%s", entp->fts_path );
		return ( 1 );
	}
	fs = entp->fts_statp;
	if ( !dne ) {
#define YESNO "(y/n [n]) "
		if ( nflag ) {
			if ( vflag ) printf( "%s not overwritten\n", to.p_path );
			(void) close( from_fd );
			return ( 1 );
		} else if ( iflag ) {
			(void) fprintf( stderr, "overwrite %s? %s", to.p_path, YESNO );
			checkch = ch = getchar();
			while ( ch != '\n' && ch != EOF )
				ch = getchar();
			if ( checkch != 'y' && checkch != 'Y' ) {
				(void) close( from_fd );
				(void) fprintf( stderr, "not overwritten\n" );
				return ( 1 );
			}
		}
		if ( fflag ) {
			(void) unlink( to.p_path );
			if ( !lflag ) to_fd = open( to.p_path, O_WRONLY | O_TRUNC | O_CREAT, fs->st_mode & ~( S_ISUID | S_ISGID ) );
		} else {
			if ( !lflag ) to_fd = open( to.p_path, O_WRONLY | O_TRUNC, 0 );
		}
	} else {
		if ( !lflag ) to_fd = open( to.p_path, O_WRONLY | O_TRUNC | O_CREAT, fs->st_mode & ~( S_ISUID | S_ISGID ) );
	}
	if ( to_fd == -1 ) {
		warn( "%s", to.p_path );
		(void) close( from_fd );
		return ( 1 );
	}
	rval = 0;
	if ( !lflag ) {
#ifdef VM_AND_BUFFER_CACHE_SYNCHRONIZED
		if (S_ISREG(fs->st_mode) && fs->st_size > 0 &&
				fs->st_size <= 8 * 1024 * 1024 &&
				(p = mmap(NULL, (size_t)fs->st_size, PROT_READ,
								MAP_SHARED, from_fd, (off_t)0)) != MAP_FAILED) {
			wtotal = 0;
			for (bufp = p, wresid = fs->st_size;;
					bufp += wcount, wresid -= (size_t)wcount) {
				wcount = write(to_fd, bufp, wresid);
				if (wcount <= 0)
				break;
				wtotal += wcount;
				if (info) {
					info = 0;
					(void)fprintf(stderr,
							"%s -> %s %3d%%\n",
							entp->fts_path, to.p_path,
							cp_pct(wtotal, fs->st_size));
				}
				if (wcount >= (ssize_t)wresid)
				break;
			}
			if (wcount != (ssize_t)wresid) {
				warn("%s", to.p_path);
				rval = 1;
			}
			if (munmap(p, fs->st_size) < 0) {
				warn("%s", entp->fts_path);
				rval = 1;
			}
		} else
#endif
		{
			if ( buf == NULL ) {
				if ( sysconf( _SC_PHYS_PAGES ) >
				PHYSPAGES_THRESHOLD ) bufsize = MIN( BUFSIZE_MAX, MAXPHYS * 8 );
				else bufsize = BUFSIZE_SMALL;
				buf = malloc( bufsize );
				if ( buf == NULL ) err( 1, "Not enough memory" );
			}
			wtotal = 0;
			while ( ( rcount = read( from_fd, buf, bufsize ) ) > 0 ) {
				for ( bufp = buf, wresid = rcount;; bufp += wcount, wresid -= wcount ) {
					wcount = write( to_fd, bufp, wresid );
					if ( wcount <= 0 ) break;
					wtotal += wcount;
					if ( info ) {
						info = 0;
						(void) fprintf( stderr, "%s -> %s %3d%%\n", entp->fts_path, to.p_path, cp_pct( wtotal, fs->st_size ) );
					}
					if ( wcount >= (ssize_t) wresid ) break;
				}
				if ( wcount != (ssize_t) wresid ) {
					warn( "%s", to.p_path );
					rval = 1;
					break;
				}
			}
			if ( rcount < 0 ) {
				warn( "%s", entp->fts_path );
				rval = 1;
			}
		}
	} else {
		if ( link( entp->fts_path, to.p_path ) ) {
			warn( "%s", to.p_path );
			rval = 1;
		}
	}
	if ( !lflag ) {
		if ( pflag && setfile( fs, to_fd ) ) rval = 1;
		if ( pflag && preserve_fd_acls( from_fd, to_fd ) != 0 ) rval = 1;
		if ( close( to_fd ) ) {
			warn( "%s", to.p_path );
			rval = 1;
		}
	}
	(void) close( from_fd );
	return ( rval );
}
int copy_link( const FTSENT *p, int exists ) {
	int len;
	char llink[PATH_MAX];
	if ( exists && nflag ) {
		if ( vflag ) printf( "%s not overwritten\n", to.p_path );
		return ( 1 );
	}
	if ( ( len = readlink( p->fts_path, llink, sizeof( llink ) - 1 ) ) == -1 ) {
		warn( "readlink: %s", p->fts_path );
		return ( 1 );
	}
	llink[len] = '\0';
	if ( exists && unlink( to.p_path ) ) {
		warn( "unlink: %s", to.p_path );
		return ( 1 );
	}
	if ( symlink( llink, to.p_path ) ) {
		warn( "symlink: %s", llink );
		return ( 1 );
	}
	return ( pflag ? setfile( p->fts_statp, -1 ) : 0 );
}
int copy_fifo( struct stat *from_stat, int exists ) {
	if ( exists && nflag ) {
		if ( vflag ) printf( "%s not overwritten\n", to.p_path );
		return ( 1 );
	}
	if ( exists && unlink( to.p_path ) ) {
		warn( "unlink: %s", to.p_path );
		return ( 1 );
	}
	if ( mkfifo( to.p_path, from_stat->st_mode ) ) {
		warn( "mkfifo: %s", to.p_path );
		return ( 1 );
	}
	return ( pflag ? setfile( from_stat, -1 ) : 0 );
}
int copy_special( struct stat *from_stat, int exists ) {
	if ( exists && nflag ) {
		if ( vflag ) printf( "%s not overwritten\n", to.p_path );
		return ( 1 );
	}
	if ( exists && unlink( to.p_path ) ) {
		warn( "unlink: %s", to.p_path );
		return ( 1 );
	}
	if ( mknod( to.p_path, from_stat->st_mode, from_stat->st_rdev ) ) {
		warn( "mknod: %s", to.p_path );
		return ( 1 );
	}
	return ( pflag ? setfile( from_stat, -1 ) : 0 );
}
int setfile( struct stat *fs, int fd ) {
	static struct timeval tv[2];
	struct stat ts;
	int rval, gotstat, islink, fdval;
	rval = 0;
	fdval = fd != -1;
	islink = !fdval && S_ISLNK( fs->st_mode );
	fs->st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
	TIMESPEC_TO_TIMEVAL( &tv[0], &fs->st_atim );
	TIMESPEC_TO_TIMEVAL( &tv[1], &fs->st_mtim );
	if ( islink ? lutimes( to.p_path, tv ) : utimes( to.p_path, tv ) ) {
		warn( "%sutimes: %s", islink ? "l" : "", to.p_path );
		rval = 1;
	}
	if ( fdval ? fstat( fd, &ts ) : ( islink ? lstat( to.p_path, &ts ) : stat( to.p_path, &ts ) ) ) gotstat = 0;
	else {
		gotstat = 1;
		ts.st_mode &= S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO;
	}
	if ( !gotstat || fs->st_uid != ts.st_uid || fs->st_gid != ts.st_gid ) if ( fdval ? fchown( fd, fs->st_uid, fs->st_gid ) : ( islink ? lchown( to.p_path, fs->st_uid, fs->st_gid ) : chown( to.p_path, fs->st_uid, fs->st_gid ) ) ) {
		if ( errno != EPERM ) {
			warn( "chown: %s", to.p_path );
			rval = 1;
		}
		fs->st_mode &= ~( S_ISUID | S_ISGID );
	}
	if ( !gotstat || fs->st_mode != ts.st_mode ) if ( fdval ? fchmod( fd, fs->st_mode ) : ( islink ? lchmod( to.p_path, fs->st_mode ) : chmod( to.p_path, fs->st_mode ) ) ) {
		warn( "chmod: %s", to.p_path );
		rval = 1;
	}
	if ( !gotstat || fs->st_flags != ts.st_flags ) if ( fdval ? fchflags( fd, fs->st_flags ) : ( islink ? lchflags( to.p_path, fs->st_flags ) : chflags( to.p_path, fs->st_flags ) ) ) {
		warn( "chflags: %s", to.p_path );
		rval = 1;
	}
	return ( rval );
}
int preserve_fd_acls( int source_fd, int dest_fd ) {
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;
	ret = fpathconf( source_fd, _PC_ACL_NFS4 );
	if ( ret > 0 ) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if ( ret < 0 && errno != EINVAL ) {
		warn( "fpathconf(..., _PC_ACL_NFS4) failed for %s", to.p_path );
		return ( 1 );
	}
	if ( acl_supported == 0 ) {
		ret = fpathconf( source_fd, _PC_ACL_EXTENDED );
		if ( ret > 0 ) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if ( ret < 0 && errno != EINVAL ) {
			warn( "fpathconf(..., _PC_ACL_EXTENDED) failed for %s", to.p_path );
			return ( 1 );
		}
	}
	if ( acl_supported == 0 ) return ( 0 );
	acl = acl_get_fd_np( source_fd, acl_type );
	if ( acl == NULL ) {
		warn( "failed to get acl entries while setting %s", to.p_path );
		return ( 1 );
	}
	if ( acl_is_trivial_np( acl, &trivial ) ) {
		warn( "acl_is_trivial() failed for %s", to.p_path );
		acl_free( acl );
		return ( 1 );
	}
	if ( trivial ) {
		acl_free( acl );
		return ( 0 );
	}
	if ( acl_set_fd_np( dest_fd, acl, acl_type ) < 0 ) {
		warn( "failed to set acl entries for %s", to.p_path );
		acl_free( acl );
		return ( 1 );
	}
	acl_free( acl );
	return ( 0 );
}
int preserve_dir_acls( struct stat *fs, char *source_dir, char *dest_dir ) {
	acl_t (*aclgetf)( const char *, acl_type_t );
	int (*aclsetf)( const char *, acl_type_t, acl_t );
	struct acl *aclp;
	acl_t acl;
	acl_type_t acl_type;
	int acl_supported = 0, ret, trivial;
	ret = pathconf( source_dir, _PC_ACL_NFS4 );
	if ( ret > 0 ) {
		acl_supported = 1;
		acl_type = ACL_TYPE_NFS4;
	} else if ( ret < 0 && errno != EINVAL ) {
		warn( "fpathconf(..., _PC_ACL_NFS4) failed for %s", source_dir );
		return ( 1 );
	}
	if ( acl_supported == 0 ) {
		ret = pathconf( source_dir, _PC_ACL_EXTENDED );
		if ( ret > 0 ) {
			acl_supported = 1;
			acl_type = ACL_TYPE_ACCESS;
		} else if ( ret < 0 && errno != EINVAL ) {
			warn( "fpathconf(..., _PC_ACL_EXTENDED) failed for %s", source_dir );
			return ( 1 );
		}
	}
	if ( acl_supported == 0 ) return ( 0 );
	if ( S_ISLNK( fs->st_mode ) ) {
		aclgetf = acl_get_link_np;
		aclsetf = acl_set_link_np;
	} else {
		aclgetf = acl_get_file;
		aclsetf = acl_set_file;
	}
	if ( acl_type == ACL_TYPE_ACCESS ) {
		acl = aclgetf( source_dir, ACL_TYPE_DEFAULT );
		if ( acl == NULL ) {
			warn( "failed to get default acl entries on %s", source_dir );
			return ( 1 );
		}
		aclp = &acl->ats_acl;
		if ( aclp->acl_cnt != 0 && aclsetf( dest_dir, ACL_TYPE_DEFAULT, acl ) < 0 ) {
			warn( "failed to set default acl entries on %s", dest_dir );
			acl_free( acl );
			return ( 1 );
		}
		acl_free( acl );
	}
	acl = aclgetf( source_dir, acl_type );
	if ( acl == NULL ) {
		warn( "failed to get acl entries on %s", source_dir );
		return ( 1 );
	}
	if ( acl_is_trivial_np( acl, &trivial ) ) {
		warn( "acl_is_trivial() failed on %s", source_dir );
		acl_free( acl );
		return ( 1 );
	}
	if ( trivial ) {
		acl_free( acl );
		return ( 0 );
	}
	if ( aclsetf( dest_dir, acl_type, acl ) < 0 ) {
		warn( "failed to set acl entries on %s", dest_dir );
		acl_free( acl );
		return ( 1 );
	}
	acl_free( acl );
	return ( 0 );
}
void usage( void ) {
	(void) fprintf( stderr, "%s\n%s\n", "usage: cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpvx] source_file target_file", "       cp [-R [-H | -L | -P]] [-f | -i | -n] [-alpvx] source_file ... "
			"target_directory" );
	exit (EX_USAGE);
}
