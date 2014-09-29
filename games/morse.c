#ifndef lint
static const char copyright[] = "@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif 
#ifndef lint
#if 0
static char sccsid[] = "@(#)morse.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/time.h>
#include <ctype.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#define SPEAKER "/dev/speaker"
#ifdef SPEAKER
#include <dev/speaker/speaker.h>
#endif
struct morsetab {
		const char inchar;
		const char *morse;
};
static const struct morsetab mtab[] = { { 'a', ".-" }, { 'b', "-..." }, { 'c', "-.-." }, { 'd', "-.." }, { 'e', "." }, { 'f', "..-." }, { 'g', "--." }, { 'h', "...." }, { 'i', ".." }, { 'j', ".---" }, { 'k', "-.-" }, { 'l', ".-.." }, { 'm', "--" }, { 'n', "-." }, { 'o', "---" }, { 'p', ".--." }, { 'q', "--.-" }, { 'r', ".-." }, { 's', "..." }, { 't', "-" }, { 'u', "..-" }, { 'v', "...-" }, { 'w', ".--" }, { 'x', "-..-" }, { 'y', "-.--" }, { 'z', "--.." }, { '0', "-----" }, { '1', ".----" }, { '2', "..---" }, { '3', "...--" }, { '4', "....-" }, { '5', "....." }, { '6', "-...." }, { '7', "--..." }, { '8', "---.." }, { '9', "----." }, { ',', "--..--" }, { '.', ".-.-.-" }, { '"', ".-..-." }, { '!', "..--." }, { '?', "..--.." }, { '/', "-..-." }, { '-', "-....-" }, { '=', "-...-" }, { ':', "---..." }, { ';', "-.-.-." }, { '(', "-.--." }, { ')', "-.--.-" }, { '$', "...-..-" }, { '+', ".-.-." }, { '@', ".--.-." }, { '#', ".-..." }, { '&', "...-.-" }, { '*', "...-." }, { '%', "-...-.-" }, {
		'\0', "" } };
static const struct morsetab iso8859_1tab[] = { { '\340', ".--.-" }, { '\341', ".--.-" }, { '\342', ".--.-" }, { '\344', ".-.-" }, { '\347', "-.-.." }, { '\350', "..-.." }, { '\351', "..-.." }, { '\352', "-..-." }, { '\366', "---." }, { '\374', "..--" }, { '\0', "" } };
static const struct morsetab iso8859_7tab[] = { { '\341', ".-" }, { '\334', ".-" }, { '\342', "-..." }, { '\343', "--." }, { '\344', "-.." }, { '\345', "." }, { '\335', "." }, { '\346', "--.." }, { '\347', "...." }, { '\336', "...." }, { '\350', "-.-." }, { '\351', ".." }, { '\337', ".." }, { '\372', ".." }, { '\300', ".." }, { '\352', "-.-" }, { '\353', ".-.." }, { '\354', "--" }, { '\355', "-." }, { '\356', "-..-" }, { '\357', "---" }, { '\374', "---" }, { '\360', ".--." }, { '\361', ".-." }, { '\363', "..." }, { '\362', "..." }, { '\364', "-" }, { '\365', "-.--" }, { '\375', "-.--" }, { '\373', "-.--" }, { '\340', "-.--" }, { '\366', "..-." }, { '\367', "----" }, { '\370', "--.-" }, { '\371', ".--" }, { '\376', ".--" }, { '\0', "" } };
static const struct morsetab koi8rtab[] = { { '\301', ".-" }, { '\302', "-..." }, { '\327', ".--" }, { '\307', "--." }, { '\304', "-.." }, { '\305', "." }, { '\243', "." }, { '\326', "...-" }, { '\332', "--.." }, { '\311', ".." }, { '\312', ".---" }, { '\313', "-.-" }, { '\314', ".-.." }, { '\315', "--" }, { '\316', "-." }, { '\317', "---" }, { '\320', ".--." }, { '\322', ".-." }, { '\323', "..." }, { '\324', "-" }, { '\325', "..-" }, { '\306', "..-." }, { '\310', "...." }, { '\303', "-.-." }, { '\336', "---." }, { '\333', "----" }, { '\335', "--.-" }, { '\331', "-.--" }, { '\330', "-..-" }, { '\334', "..-.." }, { '\300', "..--" }, { '\321', ".-.-" }, { '\0', "" } };
static void show( const char * ), play( const char * ), morse( char );
static void ttyout( const char * );
static void sighandler( int );
#define GETOPTOPTS "c:d:ef:lsw:"
#define USAGE \
"usage: morse [-els] [-d device] [-w speed] [-c speed] [-f frequency] [string ...]\n"
static int pflag, lflag, sflag, eflag;
static int wpm = 20;
static int cpm;
#define FREQUENCY 600
static int freq = FREQUENCY;
static char *device;
#define DASH_LEN 3
#define CHAR_SPACE 3
#define WORD_SPACE (7 - CHAR_SPACE - 1)
static float dot_clock;
static float cdot_clock;
static int spkr, line;
static struct termios otty, ntty;
static int olflags;
#ifdef SPEAKER
static tone_t sound;
#undef GETOPTOPTS
#define GETOPTOPTS "c:d:ef:lpsw:"
#undef USAGE
#define USAGE \
"usage: morse [-elps] [-d device] [-w speed] [-c speed] [-f frequency] [string ...]\n"
#endif
static const struct morsetab *hightab;
int main( int argc, char **argv ) {
	int ch, lflags;
	char *p, *codeset;
	while ( ( ch = getopt( argc, argv, GETOPTOPTS ) ) != -1 )
		switch ( (char) ch ) {
			case 'c':
				cpm = atoi( optarg );
				break;
			case 'd':
				device = optarg;
				break;
			case 'e':
				eflag = 1;
				setvbuf( stdout, 0, _IONBF, 0 );
				break;
			case 'f':
				freq = atoi( optarg );
				break;
			case 'l':
				lflag = 1;
				break;
#ifdef SPEAKER
			case 'p':
				pflag = 1;
				break;
#endif
			case 's':
				sflag = 1;
				break;
			case 'w':
				wpm = atoi( optarg );
				break;
			case '?':
			default:
				fputs( USAGE, stderr );
				exit( 1 );
		}
	if ( sflag && lflag ) {
		fputs( "morse: only one of -l and -s allowed\n", stderr );
		exit( 1 );
	}
	if ( ( pflag || device ) && ( sflag || lflag ) ) {
		fputs( "morse: only one of -p, -d and -l, -s allowed\n", stderr );
		exit( 1 );
	}
	if ( cpm == 0 ) cpm = wpm;
	if ( ( pflag || device ) && ( ( wpm < 1 ) || ( wpm > 60 ) || ( cpm < 1 ) || ( cpm > 60 ) ) ) {
		fputs( "morse: insane speed\n", stderr );
		exit( 1 );
	}
	if ( ( pflag || device ) && ( freq == 0 ) ) freq = FREQUENCY;
#ifdef SPEAKER
	if ( pflag ) {
		if ( ( spkr = open( SPEAKER, O_WRONLY, 0 ) ) == -1 ) {
			perror( SPEAKER );
			exit( 1 );
		}
	} else
#endif
	if ( device ) {
		if ( ( line = open( device, O_WRONLY | O_NONBLOCK ) ) == -1 ) {
			perror( "open tty line" );
			exit( 1 );
		}
		if ( tcgetattr( line, &otty ) == -1 ) {
			perror( "tcgetattr() failed" );
			exit( 1 );
		}
		ntty = otty;
		ntty.c_cflag |= CLOCAL;
		tcsetattr( line, TCSANOW, &ntty );
		lflags = fcntl( line, F_GETFL );
		lflags &= ~O_NONBLOCK;
		fcntl( line, F_SETFL, &lflags );
		ioctl( line, TIOCMGET, &lflags );
		lflags &= ~TIOCM_RTS;
		olflags = lflags;
		ioctl( line, TIOCMSET, &lflags );
		(void) signal( SIGHUP, sighandler );
		(void) signal( SIGINT, sighandler );
		(void) signal( SIGQUIT, sighandler );
		(void) signal( SIGTERM, sighandler );
	}
	if ( pflag || device ) {
		dot_clock = wpm / 2.4;
		dot_clock = 1 / dot_clock;
		dot_clock = dot_clock / 2;
		dot_clock = dot_clock * 100;
		cdot_clock = cpm / 2.4;
		cdot_clock = 1 / cdot_clock;
		cdot_clock = cdot_clock / 2;
		cdot_clock = cdot_clock * 100;
	}
	argc -= optind;
	argv += optind;
	if ( setlocale( LC_CTYPE, "" ) != NULL && *( codeset = nl_langinfo( CODESET ) ) != '\0' ) {
		if ( strcmp( codeset, "KOI8-R" ) == 0 ) hightab = koi8rtab;
		else if ( strcmp( codeset, "ISO8859-1" ) == 0 || strcmp( codeset, "ISO8859-15" ) == 0 ) hightab = iso8859_1tab;
		else if ( strcmp( codeset, "ISO8859-7" ) == 0 ) hightab = iso8859_7tab;
	}
	if ( lflag ) printf( "m" );
	if ( *argv ) {
		do {
			for ( p = *argv; *p; ++p ) {
				if ( eflag ) putchar( *p );
				morse( *p );
			}
			if ( eflag ) putchar( ' ' );
			morse( ' ' );
		} while ( *++argv );
	} else {
		while ( ( ch = getchar() ) != EOF ) {
			if ( eflag ) putchar( ch );
			morse( ch );
		}
	}
	if ( device ) tcsetattr( line, TCSANOW, &otty );
	exit( 0 );
}
static void morse( char c ) {
	const struct morsetab *m;
	if ( isalpha( (unsigned char) c ) ) c = tolower( (unsigned char) c );
	if ( ( c == '\r' ) || ( c == '\n' ) ) c = ' ';
	if ( c == ' ' ) {
		if ( pflag ) play( " " );
		else if ( device ) ttyout( " " );
		else if ( lflag ) printf( "\n" );
		else show( "" );
		return;
	}
	for ( m = ( (unsigned char) c < 0x80 ? mtab : hightab ); m != NULL && m->inchar != '\0'; m++ ) {
		if ( m->inchar == c ) {
			if ( pflag ) {
				play( m->morse );
			} else if ( device ) {
				ttyout( m->morse );
			} else show( m->morse );
		}
	}
}
static void show( const char *s ) {
	if ( lflag ) {
		printf( "%s ", s );
	} else if ( sflag ) {
		printf( " %s\n", s );
	} else {
		for ( ; *s; ++s )
			printf( " %s", *s == '.' ? *( s + 1 ) == '\0' ? "dit" : "di" : "dah" );
		printf( "\n" );
	}
}
static void play( const char *s ) {
#ifdef SPEAKER
	const char *c;
	for ( c = s; *c != '\0'; c++ ) {
		switch ( *c ) {
			case '.':
				sound.frequency = freq;
				sound.duration = dot_clock;
				break;
			case '-':
				sound.frequency = freq;
				sound.duration = dot_clock * DASH_LEN;
				break;
			case ' ':
				sound.frequency = 0;
				sound.duration = cdot_clock * WORD_SPACE;
				break;
			default:
				sound.duration = 0;
		}
		if ( sound.duration ) {
			if ( ioctl( spkr, SPKRTONE, &sound ) == -1 ) {
				perror( "ioctl play" );
				exit( 1 );
			}
		}
		sound.frequency = 0;
		sound.duration = dot_clock;
		if ( ioctl( spkr, SPKRTONE, &sound ) == -1 ) {
			perror( "ioctl rest" );
			exit( 1 );
		}
	}
	sound.frequency = 0;
	sound.duration = cdot_clock * CHAR_SPACE;
	ioctl( spkr, SPKRTONE, &sound );
#endif
}
static void ttyout( const char *s ) {
	const char *c;
	int duration, on, lflags;
	for ( c = s; *c != '\0'; c++ ) {
		switch ( *c ) {
			case '.':
				on = 1;
				duration = dot_clock;
				break;
			case '-':
				on = 1;
				duration = dot_clock * DASH_LEN;
				break;
			case ' ':
				on = 0;
				duration = cdot_clock * WORD_SPACE;
				break;
			default:
				on = 0;
				duration = 0;
		}
		if ( on ) {
			ioctl( line, TIOCMGET, &lflags );
			lflags |= TIOCM_RTS;
			ioctl( line, TIOCMSET, &lflags );
		}
		duration *= 10000;
		if ( duration ) usleep( duration );
		ioctl( line, TIOCMGET, &lflags );
		lflags &= ~TIOCM_RTS;
		ioctl( line, TIOCMSET, &lflags );
		duration = dot_clock * 10000;
		usleep( duration );
	}
	duration = cdot_clock * CHAR_SPACE * 10000;
	usleep( duration );
}
static void sighandler( int signo ) {
	ioctl( line, TIOCMSET, &olflags );
	tcsetattr( line, TCSANOW, &otty );
	signal( signo, SIG_DFL );
	(void) kill( getpid(), signo );
}
