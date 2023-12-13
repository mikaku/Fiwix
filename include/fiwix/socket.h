/*
 * fiwix/include/fiwix/socket.h
 *
 * Copyright 2023, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_SOCKET_H
#define _FIWIX_SOCKET_H

#include <fiwix/types.h>

/* domains (families) */
#define AF_UNIX		1	/* UNIX domain socket */

/* types */
#define SOCK_STREAM	1
#define SOCK_DGRAM	2

/* maximum queue length specifiable by listen() */
#define SOMAXCONN	5

/* states */
#define SS_UNCONNECTED		1
#define SS_CONNECTING		2
#define SS_CONNECTED		3
#define SS_DISCONNECTING	4

/* flags */
#define SO_ACCEPTCONN		0x10000

/* flags for send() and recv() */
#define MSG_DONTWAIT		0x40

typedef unsigned short int sa_family_t;


/* generic socket address structure */
struct sockaddr {
	sa_family_t sa_family;		/* address family: AF_xxx */
	char sa_data[14];		/* protocol specific address */
};

/* UNIX domain socket address structure */
struct sockaddr_un {
        sa_family_t sun_family;		/* AF_UNIX */
        char sun_path[108];		/* socket filename */
};

#endif /* _FIWIX_SOCKET_H */

#endif /* CONFIG_NET */
