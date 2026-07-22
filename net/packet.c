/*
 * fiwix/net/packet.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/net.h>
#include <fiwix/net/packet.h>
#include <fiwix/socket.h>

#ifdef CONFIG_NET
struct packet *peek_packet(struct packet *queue_head)
{
	return queue_head;
}

struct packet *remove_packet_from_queue(struct packet **queue_head)
{
	struct packet *p;

	if((p = *queue_head)) {
		*queue_head = (*queue_head)->next;
	}

	return p;
}

void append_packet_to_queue(struct packet *p, struct packet **queue_head)
{
	if(!(*queue_head)) {
		*queue_head = p;
	} else {
		p->prev = (*queue_head)->next;
		(*queue_head)->prev->next = p;
	}
	(*queue_head)->prev = p;
}
#endif /* CONFIG_NET */
