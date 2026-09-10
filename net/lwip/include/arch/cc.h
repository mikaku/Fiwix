/* Applies to i386. */

#include <fiwix/types.h>
#include <fiwix/stdio.h>
#include <fiwix/kernel.h>
#include <fiwix/rand.h>
#include <fiwix/ioctl.h>
#include <fiwix/if.h>

#include <fiwix/errno.h>
#define LWIP_ERRNO_INCLUDE <fiwix/errno.h>
#define LWIP_ERRNO_STDINCLUDE <fiwix/errno.h>
#define errno	current->errno

/* stdio.h interface as expected by lwIP */
#define printf printk
#define sprintf sprintk
#define snprintf(BUF, LEN, ...) sprintk(BUF, __VA_ARGS__)

#define BYTE_ORDER LITTLE_ENDIAN

#define LWIP_RAND() rand()

#define LWIP_NO_STDDEF_H 1
typedef __size_t size_t;

/* no stdint.h */
#define LWIP_NO_STDINT_H 1
#define LWIP_HAVE_INT64 1
typedef __u8 u8_t;
typedef __s8 s8_t;
typedef __u16 u16_t;
typedef __s16 s16_t;
typedef __u32 u32_t;
typedef __s32 s32_t;
typedef __u64 u64_t;
typedef __s64 s64_t;
typedef unsigned int mem_ptr_t;

/* or inttypes.h */
#define LWIP_NO_INTTYPES_H 1
#define X8_F "02x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

/* or limits.h */
#define LWIP_NO_LIMITS_H 1
#define INT_MAX 2147483647

/* ctypes.h does exist, but not in the format lwIP expects */
#define LWIP_NO_CTYPE_H 1

/* the warnings are ok for now */
#define LWIP_CONST_CAST(target_type, val) val

/* same format as default */
#define LWIP_PLATFORM_ASSERT(x) PANIC("Assertion \"%s\" failed at line %d in %s\n", \
                                     x, __LINE__, __FILE__)

/* specific lwIP data types (from include/lwip/sockets.h) */
#if !defined(POLLIN) && !defined(POLLOUT)
#define POLLIN     0x1
#define POLLOUT    0x2
#define POLLERR    0x4
#define POLLNVAL   0x8
/* Below values are unimplemented */
#define POLLRDNORM 0x10
#define POLLRDBAND 0x20
#define POLLPRI    0x40
#define POLLWRNORM 0x80
#define POLLWRBAND 0x100
#define POLLHUP    0x200
typedef unsigned int nfds_t;
struct pollfd
{
  int fd;
  short events;
  short revents;
};
#endif

struct sockaddr_storage {
  sa_family_t ss_family;
  char        s2_data1[2];
  u32_t       s2_data2[3];
#if LWIP_IPV6
  u32_t       s2_data3[3];
#endif /* LWIP_IPV6 */
};

#define inet_addr_from_ip4addr(target_inaddr, source_ipaddr) ((target_inaddr)->s_addr = ip4_addr_get_u32(source_ipaddr))
#define inet_addr_to_ip4addr(target_ipaddr, source_inaddr)   (ip4_addr_set_u32(target_ipaddr, (source_inaddr)->s_addr))

#define SIN_ZERO_LEN	__SIN_ZERO_SIZE__
typedef int msg_iovlen_t;

#if !defined IOV_MAX
#define IOV_MAX 0xFFFF
#elif IOV_MAX > 0xFFFF
#error "IOV_MAX larger than supported by LwIP"
#endif

#ifndef O_NONBLOCK
#define O_NONBLOCK	1
#endif
#ifndef O_NDELAY
#define O_NDELAY	O_NONBLOCK
#endif
#ifndef O_RDONLY
#define O_RDONLY	2
#endif
#ifndef O_WRONLY
#define O_WRONLY	4
#endif
#ifndef O_RDWR
#define O_RDWR      (O_RDONLY|O_WRONLY)
#endif

#ifndef F_GETFL
#define F_GETFL		3
#endif
#ifndef F_SETFL
#define F_SETFL		4
#endif

#ifndef SHUT_RD
#define SHUT_RD		0
#define SHUT_WR		1
#define SHUT_RDWR	2
#endif

#define TCP_KEEPALIVE	2
#define IPPROTO_UDPLITE	136
#define LWIP_SELECT_MAXNFDS FD_SETSIZE

