#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 2000 Christoph Herrmann, Thomas-Henning von Kamptz\n\
Copyright (c) 1980, 1989, 1993 The Regents of the University of California.\n\
All rights reserved.\n";
#endif 
#ifndef lint
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/param.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <libufs.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "debug.h"
#ifdef FS_DEBUG
int _dbg_lvl_ = (DL_INFO);
#endif 
static struct uufsd disk;
#define sblock disk.d_fs
#define acg    disk.d_cg
static union {
		struct fs fs;
		char pad[SBLOCKSIZE];
} fsun;
#define osblock fsun.fs
static char i1blk[MAXBSIZE];
static char i2blk[MAXBSIZE];
static char i3blk[MAXBSIZE];
static struct csum *fscs;
static void usage( void );
static void dump_whole_ufs1_inode( ino_t, int );
static void dump_whole_ufs2_inode( ino_t, int );
#define DUMP_WHOLE_INODE(A,B) \
	( disk.d_ufs == 1 \
		? dump_whole_ufs1_inode((A),(B)) : dump_whole_ufs2_inode((A),(B)) )
int main( int argc, char **argv ) {
	DBG_FUNC( "main" )
	char *device, *special;
	int ch;
	size_t len;
	struct stat st;
	struct csum *dbg_csp;
	int dbg_csc;
	char dbg_line[80];
	int cylno, i;
	int cfg_cg, cfg_in, cfg_lv;
	int cg_start, cg_stop;
	ino_t in;
	char *out_file;
	DBG_ENTER;
	cfg_lv = 0xff;
	cfg_in = -2;
	cfg_cg = -2;
	out_file = strdup( "-" );
	while ( ( ch = getopt( argc, argv, "g:i:l:o:" ) ) != -1 ) {
		switch ( ch ) {
			case 'g':
				cfg_cg = strtol( optarg, NULL, 0 );
				if ( errno == EINVAL || errno == ERANGE ) err( 1, "%s", optarg );
				if ( cfg_cg < -1 ) usage();
				break;
			case 'i':
				cfg_in = strtol( optarg, NULL, 0 );
				if ( errno == EINVAL || errno == ERANGE ) err( 1, "%s", optarg );
				if ( cfg_in < 0 ) usage();
				break;
			case 'l':
				cfg_lv = strtol( optarg, NULL, 0 );
				if ( errno == EINVAL || errno == ERANGE ) err( 1, "%s", optarg );
				if ( cfg_lv < 0x1 || cfg_lv > 0x3ff ) usage();
				break;
			case 'o':
				free( out_file );
				out_file = strdup( optarg );
				if ( out_file == NULL ) errx( 1, "strdup failed" );
				break;
			case '?':
			default:
				usage();
		}
	}
	argc -= optind;
	argv += optind;
	if ( argc != 1 ) usage();
	device = *argv;
	if ( 0 == strrchr( device, '/' ) && stat( device, &st ) == -1 ) {
		len = strlen( device ) + strlen( _PATH_DEV ) + 2 + strlen( "vinum/" );
		special = (char *) malloc( len );
		if ( special == NULL ) errx( 1, "malloc failed" );
		snprintf( special, len, "%sr%s", _PATH_DEV, device );
		if ( stat( special, &st ) == -1 ) {
			snprintf( special, len, "%s%s", _PATH_DEV, device );
			if ( stat( special, &st ) == -1 ) {
				snprintf( special, len, "%svinum/r%s", _PATH_DEV, device );
				if ( stat( special, &st ) == -1 ) snprintf( special, len, "%svinum/%s", _PATH_DEV, device );
			}
		}
		device = special;
	}
	if ( ufs_disk_fillout( &disk, device ) == -1 ) err( 1, "ufs_disk_fillout(%s) failed: %s", device, disk.d_error );
	DBG_OPEN( out_file );
	if ( cfg_lv & 0x001 ) DBG_DUMP_FS( &sblock, "primary sblock" );
	if ( cfg_cg == -2 ) {
		cg_start = 0;
		cg_stop = sblock.fs_ncg;
	} else if ( cfg_cg == -1 ) {
		cg_start = sblock.fs_ncg - 1;
		cg_stop = sblock.fs_ncg;
	} else if ( cfg_cg < sblock.fs_ncg ) {
		cg_start = cfg_cg;
		cg_stop = cfg_cg + 1;
	} else {
		cg_start = sblock.fs_ncg;
		cg_stop = sblock.fs_ncg;
	}
	if ( cfg_lv & 0x004 ) {
		fscs = (struct csum *) calloc( (size_t) 1, (size_t) sblock.fs_cssize );
		if ( fscs == NULL ) errx( 1, "calloc failed" );
		for ( i = 0; i < sblock.fs_cssize; i += sblock.fs_bsize ) {
			if ( bread( &disk, fsbtodb( &sblock,
			sblock.fs_csaddr + numfrags( &sblock, i ) ), (void *) ( ( (char *) fscs ) + i ), ( size_t )( sblock.fs_cssize - i < sblock.fs_bsize ?
			sblock.fs_cssize - i : sblock.fs_bsize ) ) == -1 ) err( 1, "bread: %s", disk.d_error );
		}
		dbg_csp = fscs;
		for ( dbg_csc = 0; dbg_csc < sblock.fs_ncg; dbg_csc++ ) {
			snprintf( dbg_line, sizeof( dbg_line ), "%d. csum in fscs", dbg_csc );
			DBG_DUMP_CSUM( &sblock, dbg_line, dbg_csp++ );
		}
	}
	if ( cfg_lv & 0xf8 ) {
		for ( cylno = cg_start; cylno < cg_stop; cylno++ ) {
			snprintf( dbg_line, sizeof( dbg_line ), "cgr %d", cylno );
			if ( cfg_lv & 0x002 ) {
				if ( bread( &disk, fsbtodb( &sblock, cgsblock( &sblock, cylno ) ), (void *) &osblock, SBLOCKSIZE ) == -1 ) err( 1, "bread: %s", disk.d_error );
				DBG_DUMP_FS( &osblock, dbg_line );
			}
			if ( bread( &disk, fsbtodb( &sblock, cgtod( &sblock, cylno ) ), (void *) &acg, (size_t) sblock.fs_cgsize ) == -1 ) err( 1, "bread: %s", disk.d_error );
			if ( cfg_lv & 0x008 ) DBG_DUMP_CG( &sblock, dbg_line, &acg );
			if ( cfg_lv & 0x010 ) DBG_DUMP_INMAP( &sblock, dbg_line, &acg );
			if ( cfg_lv & 0x020 ) DBG_DUMP_FRMAP( &sblock, dbg_line, &acg );
			if ( cfg_lv & 0x040 ) {
				DBG_DUMP_CLMAP( &sblock, dbg_line, &acg );
				DBG_DUMP_CLSUM( &sblock, dbg_line, &acg );
			}
#ifdef NOT_CURRENTLY
			if (disk.d_ufs == 1 && cfg_lv & 0x080)
			DBG_DUMP_SPTBL(&sblock, dbg_line, &acg);
#endif
		}
	}
	if ( cfg_lv & 0x300 ) {
		if ( cfg_in != -2 )
		DUMP_WHOLE_INODE( (ino_t )cfg_in, cfg_lv );
		else {
			for ( in = cg_start * sblock.fs_ipg; in < (ino_t) cg_stop * sblock.fs_ipg; in++ )
				DUMP_WHOLE_INODE( in, cfg_lv );
		}
	}
	DBG_CLOSE;
	DBG_LEAVE;
	return 0;
}
void dump_whole_ufs1_inode( ino_t inode, int level ) {
	DBG_FUNC( "dump_whole_ufs1_inode" )
	struct ufs1_dinode *ino;
	int rb, mode;
	unsigned int ind2ctr, ind3ctr;
	ufs1_daddr_t *ind2ptr, *ind3ptr;
	char comment[80];
	DBG_ENTER;
	if ( getino( &disk, (void **) &ino, inode, &mode ) == -1 ) err( 1, "getino: %s", disk.d_error );
	if ( ino->di_nlink == 0 ) {
		DBG_LEAVE;
		return;
	}
	snprintf( comment, sizeof( comment ), "Inode 0x%08x", inode );
	if ( level & 0x100 ) {
		DBG_DUMP_INO( &sblock, comment, ino );
	}
	if ( !( level & 0x200 ) ) {
		DBG_LEAVE;
		return;
	}
	rb = howmany( ino->di_size, sblock.fs_bsize ) - NDADDR;
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[0] ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 0", inode );
		DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
		rb -= howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) );
	}
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[1] ), (void *) &i2blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 1", inode );
		DBG_DUMP_IBLK( &sblock, comment, i2blk, howmany( rb, howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) ) ) );
		for ( ind2ctr = 0; ( ( ind2ctr < howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) ) ) && ( rb > 0 ) ); ind2ctr++ ) {
			ind2ptr = &( (ufs1_daddr_t *) (void *) &i2blk )[ind2ctr];
			if ( bread( &disk, fsbtodb( &sblock, *ind2ptr ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
				err( 1, "bread: %s", disk.d_error );
			}
			snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 1->%d", inode, ind2ctr );
			DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
			rb -= howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) );
		}
	}
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[2] ), (void *) &i3blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2", inode );
#define SQUARE(a) ((a)*(a))
		DBG_DUMP_IBLK( &sblock, comment, i3blk, howmany( rb, SQUARE( howmany(sblock.fs_bsize, sizeof(ufs1_daddr_t)) ) ) );
#undef SQUARE
		for ( ind3ctr = 0; ( ( ind3ctr < howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) ) ) && ( rb > 0 ) ); ind3ctr++ ) {
			ind3ptr = &( (ufs1_daddr_t *) (void *) &i3blk )[ind3ctr];
			if ( bread( &disk, fsbtodb( &sblock, *ind3ptr ), (void *) &i2blk, (size_t) sblock.fs_bsize ) == -1 ) {
				err( 1, "bread: %s", disk.d_error );
			}
			snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2->%d", inode, ind3ctr );
			DBG_DUMP_IBLK( &sblock, comment, i2blk, howmany( rb, howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) ) ) );
			for ( ind2ctr = 0; ( ( ind2ctr < howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) ) ) && ( rb > 0 ) ); ind2ctr++ ) {
				ind2ptr = &( (ufs1_daddr_t *) (void *) &i2blk )[ind2ctr];
				if ( bread( &disk, fsbtodb( &sblock, *ind2ptr ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
					err( 1, "bread: %s", disk.d_error );
				}
				snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2->%d->%d", inode, ind3ctr, ind3ctr );
				DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
				rb -= howmany( sblock.fs_bsize, sizeof(ufs1_daddr_t) );
			}
		}
	}
	DBG_LEAVE;
	return;
}
void dump_whole_ufs2_inode( ino_t inode, int level ) {
	DBG_FUNC( "dump_whole_ufs2_inode" )
	struct ufs2_dinode *ino;
	int rb, mode;
	unsigned int ind2ctr, ind3ctr;
	ufs2_daddr_t *ind2ptr, *ind3ptr;
	char comment[80];
	DBG_ENTER;
	if ( getino( &disk, (void **) &ino, inode, &mode ) == -1 ) err( 1, "getino: %s", disk.d_error );
	if ( ino->di_nlink == 0 ) {
		DBG_LEAVE;
		return;
	}
	snprintf( comment, sizeof( comment ), "Inode 0x%08x", inode );
	if ( level & 0x100 ) {
		DBG_DUMP_INO( &sblock, comment, ino );
	}
	if ( !( level & 0x200 ) ) {
		DBG_LEAVE;
		return;
	}
	rb = howmany( ino->di_size, sblock.fs_bsize ) - NDADDR;
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[0] ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 0", inode );
		DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
		rb -= howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) );
	}
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[1] ), (void *) &i2blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 1", inode );
		DBG_DUMP_IBLK( &sblock, comment, i2blk, howmany( rb, howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) ) ) );
		for ( ind2ctr = 0; ( ( ind2ctr < howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) ) ) && ( rb > 0 ) ); ind2ctr++ ) {
			ind2ptr = &( (ufs2_daddr_t *) (void *) &i2blk )[ind2ctr];
			if ( bread( &disk, fsbtodb( &sblock, *ind2ptr ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
				err( 1, "bread: %s", disk.d_error );
			}
			snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 1->%d", inode, ind2ctr );
			DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
			rb -= howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) );
		}
	}
	if ( rb > 0 ) {
		if ( bread( &disk, fsbtodb( &sblock, ino->di_ib[2] ), (void *) &i3blk, (size_t) sblock.fs_bsize ) == -1 ) {
			err( 1, "bread: %s", disk.d_error );
		}
		snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2", inode );
#define SQUARE(a) ((a)*(a))
		DBG_DUMP_IBLK( &sblock, comment, i3blk, howmany( rb, SQUARE( howmany(sblock.fs_bsize, sizeof(ufs2_daddr_t)) ) ) );
#undef SQUARE
		for ( ind3ctr = 0; ( ( ind3ctr < howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) ) ) && ( rb > 0 ) ); ind3ctr++ ) {
			ind3ptr = &( (ufs2_daddr_t *) (void *) &i3blk )[ind3ctr];
			if ( bread( &disk, fsbtodb( &sblock, *ind3ptr ), (void *) &i2blk, (size_t) sblock.fs_bsize ) == -1 ) {
				err( 1, "bread: %s", disk.d_error );
			}
			snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2->%d", inode, ind3ctr );
			DBG_DUMP_IBLK( &sblock, comment, i2blk, howmany( rb, howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) ) ) );
			for ( ind2ctr = 0; ( ( ind2ctr < howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) ) ) && ( rb > 0 ) ); ind2ctr++ ) {
				ind2ptr = &( (ufs2_daddr_t *) (void *) &i2blk )[ind2ctr];
				if ( bread( &disk, fsbtodb( &sblock, *ind2ptr ), (void *) &i1blk, (size_t) sblock.fs_bsize ) == -1 ) {
					err( 1, "bread: %s", disk.d_error );
				}
				snprintf( comment, sizeof( comment ), "Inode 0x%08x: indirect 2->%d->%d", inode, ind3ctr, ind3ctr );
				DBG_DUMP_IBLK( &sblock, comment, i1blk, (size_t) rb );
				rb -= howmany( sblock.fs_bsize, sizeof(ufs2_daddr_t) );
			}
		}
	}
	DBG_LEAVE;
	return;
}
void usage( void ) {
	DBG_FUNC( "usage" )
	DBG_ENTER;
	fprintf( stderr, "usage: ffsinfo [-g cylinder_group] [-i inode] [-l level] "
			"[-o outfile]\n"
			"               special | file\n" );
	DBG_LEAVE;
	exit( 1 );
}
