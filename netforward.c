/*
 * $Id$
 *
 * Copyright © 2004 Keith Packard
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Keith Packard <keithp@keithp.com>
 */

/*
 * netforward
 *
 * Forward UDP packets from one network to another, particularily
 * useful when used with broadcast addresses
 *
 * Usage:
 *
 *	netforward [-v] [-p port] [-s source-ip] [-d dest-ip]
 *
 *  Port is a decimal port number and is used for both receive and transmit.
 *  Source-ip is a dotted-decimal IP address which should match the IP
 *	of one of the interfaces in the local machine.
 *  Dest-ip is a dotted-decimal IP address to which packets will be sent.
 *	This can include broadcast addresses.  If packets sent to this port
 *	end up received at source-ip, a nice packet loop will result.
 *
 *  -v says to make output more verbose
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

extern char *optarg;
extern int optind, opterr, optopt;

/*
 * Bail for invalid arguments
 */
void
usage (char *program)
{
    fprintf (stderr, "usage: %s [-v] [-p port] [-s source-ip] [-d dest-ip]\n",
	     program);
    exit (1);
}

/*
 * Bail when something unexpected breaks 
 */
void
losing (char *program, char *reason)
{
    int	err = errno;
    
    fprintf (stderr, "%s: losing: %s: (%d) %s\n", program, reason, err, strerror (err));
    exit (1);
}

/*
 * Display local address for a socket
 */

void
dump_addr (int fd, char *name, int do_peer)
{
    struct sockaddr_in	self;
    socklen_t		self_len = sizeof (self);
    struct sockaddr_in	peer;
    socklen_t		peer_len = sizeof (peer);
    char		self_name[256];
    char		peer_name[256];

    if (getsockname (fd, (struct sockaddr *) &self, &self_len) < 0)
	losing (name, "dump_addr self");
    
    strcpy (self_name, inet_ntoa (self.sin_addr));
    
    if (do_peer)
    {
	if (getpeername (fd, (struct sockaddr *) &peer, &peer_len) < 0)
	    losing (name, "dump_addr peer");
	
	strcpy (peer_name, inet_ntoa (peer.sin_addr));
    }
    else
    {
	strcpy (peer_name, "none");
	peer.sin_port = 0;
    }
    
    printf ("socket %s: self %s:%d peer %s:%d\n",
	    name,
	    self_name, ntohs (self.sin_port),
	    peer_name, ntohs (peer.sin_port));
}

int
main (int argc, char **argv)
{
    int			c;
    unsigned short	port = 0;
    struct in_addr	source_ip;
    struct in_addr	dest_ip;
    int			source_fd;
    int			dest_fd;
    struct sockaddr_in	source_addr;
    struct sockaddr_in	dest_addr;
    char		packet[8192];
    int			n;
    int			soopts;
    int			verbose = 0;
    int			source_set = 0;
    int			dest_set = 0;
    
    while ((c = getopt (argc, argv, "vp:s:d:")) >= 0)
    {
	switch (c) {
	case 'p':
	    port = htons (atoi (optarg));
	    if (port <= 0)
		usage (argv[0]);
	    break;
	case 's':
	    if (!inet_aton (optarg, &source_ip))
		usage (argv[0]);
	    source_set = 1;
	    break;
	case 'd':
	    if (!inet_aton (optarg, &dest_ip))
		usage (argv[0]);
	    dest_set = 1;
	    break;
	case 'v':
	    verbose++;
	    break;
	default:
	    usage (argv[0]);
	    break;
	}
    }
    if (!port || !source_set || !dest_set)
	usage (argv[0]);

    /* Create source socket */
    source_addr.sin_family = AF_INET;
    source_addr.sin_port = port;
    source_addr.sin_addr = source_ip;

    source_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (source_fd < 0)
	losing (argv[0], "source socket creation");
	
    /* make sure broadcast addresses are legal */
    soopts = 1;
    if (setsockopt (source_fd, SOL_SOCKET, SO_BROADCAST, 
		    (char *)&soopts, sizeof (soopts)) < 0)
	losing (argv[0], "source socket setsockopt");

    if (bind (source_fd, (struct sockaddr *) &source_addr, sizeof (source_addr)) < 0)
	losing (argv[0], "source binding");
    
    /* Create dest socket */
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = port;
    dest_addr.sin_addr = dest_ip;

    dest_fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (dest_fd < 0)
	losing (argv[0], "dest socket creation");
    
    /* make sure broadcast addresses are legal */
    soopts = 1;
    if (setsockopt (dest_fd, SOL_SOCKET, SO_BROADCAST, 
		    (char *)&soopts, sizeof (soopts)) < 0)
	losing (argv[0], "dest socket setsockopt");

    if (connect (dest_fd, (struct sockaddr *) &dest_addr, sizeof (dest_addr)) < 0)
	losing (argv[0], "dest connect");

    if (verbose)
    {
	dump_addr (source_fd, "source", 0);
	dump_addr (dest_fd, "dest", 1);
	printf ("ready...\n");
    }

    /* spend a while shipping packets around */
    for (;;) 
    {
	n = read (source_fd, packet, sizeof (packet));
	if (n < 0)
	    losing (argv[0], "read");

	if (verbose)
	    printf ("%d\n", n );

	if (write (dest_fd, packet, n) < n)
	    losing (argv[0], "write");
    }
}
