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
#include <sys/poll.h>
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

typedef struct _binding {
    struct _binding	*next;
    struct in_addr	addr;
    int			fd;
} binding_t;

binding_t *make_binding (char *arg)
{
    binding_t	*b;
    int		soopts = 1;
    
    b = malloc (sizeof (binding_t));
    if (!inet_aton (arg, &b->addr))
	losing (arg, "binding allocation");

    b->fd = socket (AF_INET, SOCK_DGRAM, 0);
    if (b->fd < 0)
	losing (arg, "socket creation");
    
    if (setsockopt (b->fd, SOL_SOCKET, SO_BROADCAST, 
		    (char *)&soopts, sizeof (soopts)) < 0)
	losing (arg, "setsockopt");
    
    return b;
}

void set_source (binding_t *b, int port)
{
    struct sockaddr_in	addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
    addr.sin_addr = b->addr;
    if (bind (b->fd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
	losing ("source", "bind");
}

void set_dest (binding_t *b, int port)
{
    struct sockaddr_in	addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons (port);
    addr.sin_addr = b->addr;
    if (connect (b->fd, (struct sockaddr *) &addr, sizeof (addr)) < 0)
	losing ("dest", "connect");
}

int main (int argc, char **argv)
{
    int			c;
    unsigned short	port = 0;
    binding_t		*source = 0;
    binding_t		*dest = 0;
    
    binding_t		*s, *d;
    
    char		packet[8192];
    int			n;
    int			i;
    int			nsource;
    int			soopts;
    int			verbose = 0;
    int			source_set = 0;
    int			dest_set = 0;
    struct pollfd	*fds;
    
    while ((c = getopt (argc, argv, "vp:s:d:")) >= 0)
    {
	switch (c) {
	case 'p':
	    port = atoi (optarg);
	    if (port <= 0)
		usage (argv[0]);
	    break;
	case 's':
	    s = make_binding (optarg);
	    s->next = source;
	    source = s;
	    break;
	case 'd':
	    d = make_binding (optarg);
	    d->next = dest;
	    dest = d;
	    break;
	case 'v':
	    verbose++;
	    break;
	default:
	    usage (argv[0]);
	    break;
	}
    }
    if (!port || !source || !dest)
	usage (argv[0]);

    nsource = 0;
    for (s = source; s; s = s->next)
    {
	set_source (s, port);
	if (verbose)
	    dump_addr (s->fd, "source", 0);
	nsource++;
    }
    
    if (nsource > 1)
    {
	fds = malloc (nsource * sizeof (struct pollfd));
	if (!fds)
	    losing (argv[0], "malloc fds");
    
	i = 0;
	for (s = source; s; s = s->next)
	{
	    fds[i].fd = s->fd;
	    fds[i].events = POLLIN;
	    fds[i].revents = 0;
	}
    }
    else
	fds = 0;
	
    for (d = dest; d; d = d->next)
    {
	set_dest (d, port);
	if (verbose)
	    dump_addr (d->fd, "dest", 1);
    }

    /* spend a while shipping packets around */
    for (;;) 
    {
	if (fds)
	{
	    i = poll (fds, nsource, -1);
	    if (i < 0)
		break;
	}

	i = 0;
	for (s = source, i = 0; s; s = s->next, i++)
	{
	    if (!fds || fds[i].revents & POLLIN)
	    {
		n = read (s->fd, packet, sizeof (packet));
		if (n < 0)
		    losing (argv[0], "read");
		if (verbose)
		    printf ("%d\n", n );
		for (d = dest; d; d = d->next)
		{
		    if (write (d->fd, packet, n) < n)
			losing (argv[0], "write");
		}
	    }
	}
    }
}
