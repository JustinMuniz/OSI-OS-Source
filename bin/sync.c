#if 0
#ifndef lint
static char const copyright[] =
"@(#) Copyright (c) 1987, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
static char sccsid[] = "@(#)sync.c	8.1 (Berkeley) 5/31/93";
#endif 
#endif
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <stdlib.h>
#include <unistd.h>
int
main(int argc __unused, char *argv[] __unused)
{	
	sync();
	exit(0);
}
