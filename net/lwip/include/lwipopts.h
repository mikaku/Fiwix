#ifndef _LWIP_LWIPOPTS_H
#define _LWIP_LWIPOPTS_H

#include <fiwix/mm.h>

#undef NO_SYS

/* Use the existing allocator. */
#define MEM_CUSTOM_ALLOCATOR 1
#define MEM_CUSTOM_FREE(x) kfree((unsigned int)x)
#define MEM_CUSTOM_MALLOC(x) (void *)kmalloc(x)
#define MEM_CUSTOM_CALLOC my_calloc

#define memcpy memcpy_b
#define memset memset_b
#define FD_ZERO	__FD_ZERO
#define FD_SET __FD_SET
#define FD_ISSET __FD_ISSET

#define ptrdiff_t unsigned int
#define TCPIP_MBOX_SIZE	10
#define DEFAULT_RAW_RECVMBOX_SIZE 10
#define DEFAULT_UDP_RECVMBOX_SIZE 10
#define DEFAULT_TCP_RECVMBOX_SIZE 10
#define DEFAULT_ACCEPTMBOX_SIZE 10

/* Enable parts of lwIP */
#define TCPIP_THREAD_NAME "lwip"
#define LWIP_ETHERNET 1
#define LWIP_SOCKET 1
#define LWIP_ARP 1
#define LWIP_IPV4 1
#define IP_FORWARD 1
#define LWIP_ICMP 1
#define LWIP_DHCP 1
#define LWIP_DNS 1
#define LWIP_UDP 1
#define LWIP_TCP 1
#define LWIP_NETIF_API 1
#define LWIP_IPV6 0
#define LWIP_IPV6_DHCP6 0
#define LWIP_RAW 1

#define LWIP_SO_SNDTIMEO 1
#define LWIP_SO_RCVTIMEO 1
#define LWIP_SO_RCVBUF 1
#define SO_REUSE 1

#define ETH_PAD_SIZE 2
#define MEM_ALIGNMENT 4
#define MEMP_NUM_NETBUF 128
#define MIB2_STATS 1
#define TCP_MSS 1460
#define IP_DEFAULT_TTL 64

/* debug */
/*
#define LWIP_DEBUG 1
#define LWIP_DBG_MIN_LEVEL LWIP_DBG_LEVEL_ALL
#define ETHARP_DEBUG LWIP_DBG_OFF
#define SOCKETS_DEBUG LWIP_DBG_ON
#define ICMP_DEBUG LWIP_DBG_ON
#define INET_DEBUG LWIP_DBG_ON
#define IP_DEBUG LWIP_DBG_ON
#define RAW_DEBUG LWIP_DBG_ON
#define MEM_DEBUG LWIP_DBG_ON
#define MEMP_DEBUG LWIP_DBG_ON
#define SYS_DEBUG LWIP_DBG_ON
#define TIMERS_DEBUG LWIP_DBG_ON
#define TCP_DEBUG LWIP_DBG_ON
#define TCP_INPUT_DEBUG LWIP_DBG_ON
#define TCP_FR_DEBUG LWIP_DBG_ON
#define TCP_RTO_DEBUG LWIP_DBG_ON
#define TCP_CWND_DEBUG LWIP_DBG_ON
#define TCP_WND_DEBUG LWIP_DBG_ON
#define TCP_OUTPUT_DEBUG LWIP_DBG_ON
#define TCP_RST_DEBUG LWIP_DBG_ON
#define TCP_QLEN_DEBUG LWIP_DBG_ON
#define UDP_DEBUG LWIP_DBG_ON
#define TCPIP_DEBUG LWIP_DBG_ON
#define DHCP_DEBUG LWIP_DBG_ON
#define AUTOIP_DEBUG LWIP_DBG_ON
#define DNS_DEBUG LWIP_DBG_ON
#define NETIF_DEBUG LWIP_DBG_OFF
#define PBUF_DEBUG LWIP_DBG_OFF
#define API_LIB_DEBUG LWIP_DBG_OFF
#define API_MSG_DEBUG LWIP_DBG_OFF
*/

#define SA_FAMILY_T_DEFINED 1

/* avoids touching net/lwip/include/lwip/sockets.h */
#define LWIP_SOCKET_EXTERNAL_HEADERS 1
#define LWIP_SOCKET_EXTERNAL_HEADER_SOCKETS_H <fiwix/socket.h>
#define LWIP_SOCKET_EXTERNAL_HEADER_INET_H <fiwix/in.h>


/* have a loopback interface */
/*
 * At some point, this should be removed or interfaced with the kernel
 * network API.
 */
#define LWIP_HAVE_LOOPIF 1
#define LWIP_NETIF_LOOPBACK 1

#endif /* _LWIP_LWIPOPTS_H */
