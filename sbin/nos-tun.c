#ifndef lint
static const char rcsid[] = "$FreeBSD$";
#endif 
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
static struct ifaliasreq ifra;
static struct ifreq ifrq;
int net;
int tun;
static void usage( void );
static int Set_address( char *addr, struct sockaddr_in *sin ) {
	struct hostent *hp;
	bzero( (char *) sin, sizeof(struct sockaddr) );
	sin->sin_family = AF_INET;
	if ( ( sin->sin_addr.s_addr = inet_addr( addr ) ) == ( in_addr_t ) - 1 ) {
		hp = gethostbyname( addr );
		if ( !hp ) {
			syslog( LOG_ERR, "unknown host %s", addr );
			return 1;
		}
		sin->sin_family = hp->h_addrtype;
		bcopy( hp->h_addr, ( caddr_t ) & sin->sin_addr, hp->h_length );
	}
	return 0;
}
static int tun_open( char *dev_name, struct sockaddr *ouraddr, char *theiraddr ) {
	int s;
	struct sockaddr_in *sin;
	tun = open( dev_name, O_RDWR );
	if ( tun < 0 ) {
		syslog( LOG_ERR, "can't open %s - %m", dev_name );
		return ( 1 );
	}
	bzero( (char *) &ifra, sizeof( ifra ) );
	bzero( (char *) &ifrq, sizeof( ifrq ) );
	strncpy( ifrq.ifr_name, dev_name + 5, IFNAMSIZ );
	strncpy( ifra.ifra_name, dev_name + 5, IFNAMSIZ );
	s = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( s < 0 ) {
		syslog( LOG_ERR, "can't open socket - %m" );
		goto tunc_return;
	}
	if ( ioctl( s, SIOCDIFADDR, &ifra ) < 0 ) {
		syslog( LOG_ERR, "SIOCDIFADDR - %m" );
	}
	sin = (struct sockaddr_in *) &( ifra.ifra_addr );
	bcopy( ouraddr, sin, sizeof(struct sockaddr_in) );
	sin->sin_len = sizeof( *sin );
	sin = (struct sockaddr_in *) &( ifra.ifra_broadaddr );
	if ( Set_address( theiraddr, sin ) ) {
		syslog( LOG_ERR, "bad destination address: %s", theiraddr );
		goto stunc_return;
	}
	sin->sin_len = sizeof( *sin );
	if ( ioctl( s, SIOCAIFADDR, &ifra ) < 0 ) {
		syslog( LOG_ERR, "can't set interface address - %m" );
		goto stunc_return;
	}
	if ( ioctl( s, SIOCGIFFLAGS, &ifrq ) < 0 ) {
		syslog( LOG_ERR, "can't get interface flags - %m" );
		goto stunc_return;
	}
	ifrq.ifr_flags |= IFF_UP;
	if ( !( ioctl( s, SIOCSIFFLAGS, &ifrq ) < 0 ) ) {
		close( s );
		return ( 0 );
	}
	syslog( LOG_ERR, "can't set interface UP - %m" );
	stunc_return: close( s );
	tunc_return: close( tun );
	return ( 1 );
}
static void Finish( int signum ) {
	int s;
	syslog( LOG_INFO, "exiting" );
	close( net );
	s = socket( AF_INET, SOCK_DGRAM, 0 );
	if ( s < 0 ) {
		syslog( LOG_ERR, "can't open socket - %m" );
		goto closing_tun;
	}
	if ( ioctl( s, SIOCGIFFLAGS, &ifrq ) < 0 ) {
		syslog( LOG_ERR, "can't get interface flags - %m" );
		goto closing_fds;
	}
	ifrq.ifr_flags &= ~( IFF_UP | IFF_RUNNING );
	if ( ioctl( s, SIOCSIFFLAGS, &ifrq ) < 0 ) {
		syslog( LOG_ERR, "can't set interface DOWN - %m" );
		goto closing_fds;
	}
	bzero( &ifra.ifra_addr, sizeof( ifra.ifra_addr ) );
	bzero( &ifra.ifra_broadaddr, sizeof( ifra.ifra_addr ) );
	bzero( &ifra.ifra_mask, sizeof( ifra.ifra_addr ) );
	if ( ioctl( s, SIOCDIFADDR, &ifra ) < 0 ) {
		syslog( LOG_ERR, "can't delete interface's addresses - %m" );
	}
	closing_fds: close( s );
	closing_tun: close( tun );
	closelog();
	exit( signum );
}
int main( int argc, char **argv ) {
	int c, len, ipoff;
	char *dev_name = NULL;
	char *point_to = NULL;
	char *to_point = NULL;
	char *target;
	char *source = NULL;
	char *protocol = NULL;
	int protnum;
	struct sockaddr t_laddr;
	struct sockaddr whereto;
	struct sockaddr wherefrom;
	struct sockaddr_in *to;
	char buf[0x2000];
	struct ip *ip = (struct ip *) buf;
	fd_set rfds;
	int nfds;
	int lastfd;
	while ( ( c = getopt( argc, argv, "d:s:t:p:" ) ) != -1 ) {
		switch ( c ) {
			case 'd':
				to_point = optarg;
				break;
			case 's':
				point_to = optarg;
				break;
			case 't':
				dev_name = optarg;
				break;
			case 'p':
				protocol = optarg;
				break;
		}
	}
	argc -= optind;
	argv += optind;
	if ( ( argc != 1 && argc != 2 ) || ( dev_name == NULL ) || ( point_to == NULL ) || ( to_point == NULL ) ) {
		usage();
	}
	if ( protocol == NULL ) protnum = 94;
	else protnum = atoi( protocol );
	if ( argc == 1 ) {
		target = *argv;
	} else {
		source = *argv++;
		target = *argv;
	}
	openlog( "nos-tun", LOG_PID, LOG_DAEMON );
	if ( Set_address( point_to, (struct sockaddr_in *) &t_laddr ) ) {
		closelog();
		exit( 2 );
	}
	if ( tun_open( dev_name, &t_laddr, to_point ) ) {
		closelog();
		exit( 3 );
	}
	to = (struct sockaddr_in *) &whereto;
	if ( Set_address( target, to ) ) Finish( 4 );
	if ( ( net = socket( AF_INET, SOCK_RAW, protnum ) ) < 0 ) {
		syslog( LOG_ERR, "can't open socket - %m" );
		Finish( 5 );
	}
	if ( source ) {
		if ( Set_address( source, (struct sockaddr_in *) &wherefrom ) ) Finish( 9 );
		if ( bind( net, &wherefrom, sizeof( wherefrom ) ) < 0 ) {
			syslog( LOG_ERR, "can't bind source address - %m" );
			Finish( 10 );
		}
	}
	if ( connect( net, &whereto, sizeof(struct sockaddr_in) ) < 0 ) {
		syslog( LOG_ERR, "can't connect to target - %m" );
		close( net );
		Finish( 6 );
	}
	daemon( 0, 0 );
	(void) signal( SIGHUP, Finish );
	(void) signal( SIGINT, Finish );
	(void) signal( SIGTERM, Finish );
	if ( tun > net ) lastfd = tun;
	else lastfd = net;
	for ( ;; ) {
		FD_ZERO( &rfds );
		FD_SET( tun, &rfds );
		FD_SET( net, &rfds );
		nfds = select( lastfd + 1, &rfds, NULL, NULL, NULL );
		if ( nfds < 0 ) {
			syslog( LOG_ERR, "interrupted select" );
			close( net );
			Finish( 7 );
		}
		if ( nfds == 0 ) {
			syslog( LOG_ERR, "timeout in select" );
			close( net );
			Finish( 8 );
		}
		if ( FD_ISSET( net, &rfds ) ) {
			len = read( net, buf, sizeof( buf ) );
			if ( ( ip->ip_src ).s_addr == ( to->sin_addr ).s_addr ) {
				ipoff = ( ip->ip_hl << 2 );
				write( tun, buf + ipoff, len - ipoff );
			}
		}
		if ( FD_ISSET( tun, &rfds ) ) {
			len = read( tun, buf, sizeof( buf ) );
			if ( send( net, buf, len, 0 ) <= 0 ) {
				syslog( LOG_ERR, "can't send - %m" );
			}
		}
	}
}
static void usage( void ) {
	fprintf( stderr, "usage: nos-tun -t tunnel -s source -d destination -p protocol_number [source] target\n" );
	exit( 1 );
}
