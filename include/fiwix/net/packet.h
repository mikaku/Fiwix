/*
 * fiwix/include/fiwix/net/packet.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET

#ifndef _FIWIX_NET_PACKET_H
#define _FIWIX_NET_PACKET_H

#include <fiwix/types.h>

struct packet {
	char *data;
	int len;
	__off_t offset;
	struct socket *socket;
	struct packet *prev;
	struct packet *next;
};

struct packet *peek_packet(struct packet *);
struct packet *remove_packet_from_queue(struct packet **);
void append_packet_to_queue(struct packet *, struct packet **);

#endif /* _FIWIX_NET_PACKET_H */

#endif /* CONFIG_NET */
