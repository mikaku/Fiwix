/* QEMU virt Goldfish real-time clock support. */

#include <fiwix/asm.h>
#include <fiwix/timer.h>
#include <fiwix/types.h>

#define GOLDFISH_RTC_BASE	0x00101000UL
#define GOLDFISH_TIME_LOW	0x00
#define GOLDFISH_TIME_HIGH	0x04
#define NSEC_PER_SEC		1000000000ULL

extern void riscv64_fence_full(void);

__time_t riscv64_get_system_time(void)
{
	volatile __u32 *rtc;
	__u32 low;
	__u32 high;
	__u64 nanoseconds;

	rtc = (volatile __u32 *)GOLDFISH_RTC_BASE;
	/* TIME_LOW latches the value subsequently returned by TIME_HIGH. */
	low = rtc[GOLDFISH_TIME_LOW / sizeof(*rtc)];
	riscv64_fence_full();
	high = rtc[GOLDFISH_TIME_HIGH / sizeof(*rtc)];
	nanoseconds = ((__u64)high << 32) | low;
	return (__time_t)(nanoseconds / NSEC_PER_SEC);
}
