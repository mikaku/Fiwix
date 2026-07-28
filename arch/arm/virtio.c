/*
 * fiwix/arch/arm/virtio.c
 *
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/arm_devices.h>
#include <fiwix/arm_fdt.h>
#include <fiwix/asm.h>

#define VIRTIO_MAGIC			0x74726976U
#define VIRTIO_DEVICE_BLOCK		2U
#define VIRTIO_VERSION_LEGACY		1U
#define VIRTIO_VERSION_MODERN		2U
#define VIRTIO_F_VERSION_1		1U

#define MMIO_MAGIC			0x000
#define MMIO_VERSION			0x004
#define MMIO_DEVICE_ID			0x008
#define MMIO_DEVICE_FEATURES		0x010
#define MMIO_DEVICE_FEATURES_SEL	0x014
#define MMIO_DRIVER_FEATURES		0x020
#define MMIO_DRIVER_FEATURES_SEL	0x024
#define MMIO_GUEST_PAGE_SIZE		0x028
#define MMIO_QUEUE_SEL			0x030
#define MMIO_QUEUE_NUM_MAX		0x034
#define MMIO_QUEUE_NUM			0x038
#define MMIO_QUEUE_ALIGN		0x03c
#define MMIO_QUEUE_PFN			0x040
#define MMIO_QUEUE_READY		0x044
#define MMIO_QUEUE_NOTIFY		0x050
#define MMIO_INTERRUPT_STATUS		0x060
#define MMIO_INTERRUPT_ACK		0x064
#define MMIO_STATUS			0x070
#define MMIO_QUEUE_DESC_LOW		0x080
#define MMIO_QUEUE_DESC_HIGH		0x084
#define MMIO_QUEUE_AVAIL_LOW		0x090
#define MMIO_QUEUE_AVAIL_HIGH		0x094
#define MMIO_QUEUE_USED_LOW		0x0a0
#define MMIO_QUEUE_USED_HIGH		0x0a4
#define MMIO_CONFIG			0x100
#define MMIO_REQUIRED_SIZE		0x108

#define STATUS_ACKNOWLEDGE		1U
#define STATUS_DRIVER			2U
#define STATUS_DRIVER_OK		4U
#define STATUS_FEATURES_OK		8U
#define QUEUE_SIZE			8U
#define VIRTQ_DESC_F_NEXT		1U
#define VIRTQ_DESC_F_WRITE		2U
#define VIRTIO_BLK_T_IN			0U
#define VIRTIO_BLK_T_OUT		1U
#define SECTOR_SIZE			512U
#define POLL_LIMIT			10000000UL

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;

struct virtq_descriptor {
	u64 address;
	u32 length;
	u16 flags;
	u16 next;
};

struct virtq_available {
	u16 flags;
	u16 index;
	u16 ring[QUEUE_SIZE];
};

struct virtq_used_element {
	u32 id;
	u32 length;
};

struct virtq_used {
	u16 flags;
	u16 index;
	struct virtq_used_element ring[QUEUE_SIZE];
};

struct virtio_block_request {
	u32 type;
	u32 reserved;
	u64 sector;
};

typedef char virtq_descriptor_size_must_be_16[
	(sizeof(struct virtq_descriptor) == 16) ? 1 : -1];
typedef char block_request_size_must_be_16[
	(sizeof(struct virtio_block_request) == 16) ? 1 : -1];

static u8 queue_memory[8192] __attribute__((aligned(4096)));
static struct virtio_block_request request __attribute__((aligned(16)));
static volatile u8 request_status;
static volatile u32 *transport;
static u32 transport_version;
static u32 queue_size;

static void arm_virtio_barrier(void)
{
	arm_data_memory_barrier();
}

static u32 mmio_read(u32 offset)
{
	return transport[offset / sizeof(u32)];
}

static void mmio_write(u32 offset, u32 value)
{
	transport[offset / sizeof(u32)] = value;
}

static void set_address(u32 low_register, void *address)
{
	u64 physical;

	physical = (u64)(unsigned long)address;
	mmio_write(low_register, (u32)physical);
	mmio_write(low_register + 4, (u32)(physical >> 32));
}

static void clear_bytes(u8 *memory, unsigned long count)
{
	while(count--) {
		*memory++ = 0;
	}
}

static int find_transport(void)
{
	const struct arm_fdt_info *info;
	const struct arm_fdt_region *region;
	volatile u32 *candidate;
	unsigned int n;

	info = arm_boot_fdt_info();
	if(!info) {
		return -1;
	}
	for(n = 0; n < info->virtio_count; n++) {
		region = &info->virtio[n];
		if(region->size < MMIO_REQUIRED_SIZE ||
			(region->address & (sizeof(u32) - 1U))) {
			continue;
		}
		candidate = (volatile u32 *)(unsigned long)region->address;
		if(candidate[MMIO_MAGIC / 4] == VIRTIO_MAGIC &&
			candidate[MMIO_DEVICE_ID / 4] == VIRTIO_DEVICE_BLOCK) {
			transport = candidate;
			transport_version = candidate[MMIO_VERSION / 4];
			return 0;
		}
	}
	return -1;
}

static int setup_queue(void)
{
	u32 features;
	u32 maximum;
	void *available;
	void *descriptors;
	void *used;

	clear_bytes(queue_memory, sizeof(queue_memory));
	descriptors = queue_memory;
	mmio_write(MMIO_STATUS, 0);
	arm_virtio_barrier();
	mmio_write(MMIO_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER);
	if(transport_version == VIRTIO_VERSION_MODERN) {
		mmio_write(MMIO_DEVICE_FEATURES_SEL, 1);
		features = mmio_read(MMIO_DEVICE_FEATURES);
		if(!(features & VIRTIO_F_VERSION_1)) {
			return -1;
		}
		mmio_write(MMIO_DRIVER_FEATURES_SEL, 1);
		mmio_write(MMIO_DRIVER_FEATURES, VIRTIO_F_VERSION_1);
		mmio_write(MMIO_DRIVER_FEATURES_SEL, 0);
		mmio_write(MMIO_DRIVER_FEATURES, 0);
		mmio_write(MMIO_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER |
			STATUS_FEATURES_OK);
		if(!(mmio_read(MMIO_STATUS) & STATUS_FEATURES_OK)) {
			return -1;
		}
	} else if(transport_version == VIRTIO_VERSION_LEGACY) {
		mmio_write(MMIO_DRIVER_FEATURES_SEL, 0);
		mmio_write(MMIO_DRIVER_FEATURES, 0);
		mmio_write(MMIO_GUEST_PAGE_SIZE, 4096);
	} else {
		return -1;
	}

	mmio_write(MMIO_QUEUE_SEL, 0);
	maximum = mmio_read(MMIO_QUEUE_NUM_MAX);
	if(maximum < 3) {
		return -1;
	}
	queue_size = maximum < QUEUE_SIZE ? maximum : QUEUE_SIZE;
	available = queue_memory +
		queue_size * sizeof(struct virtq_descriptor);
	used = queue_memory + 4096;
	mmio_write(MMIO_QUEUE_NUM, queue_size);
	if(transport_version == VIRTIO_VERSION_MODERN) {
		if(mmio_read(MMIO_QUEUE_READY)) {
			return -1;
		}
		set_address(MMIO_QUEUE_DESC_LOW, descriptors);
		set_address(MMIO_QUEUE_AVAIL_LOW, available);
		set_address(MMIO_QUEUE_USED_LOW, used);
		mmio_write(MMIO_QUEUE_READY, 1);
		mmio_write(MMIO_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER |
			STATUS_FEATURES_OK | STATUS_DRIVER_OK);
	} else {
		mmio_write(MMIO_QUEUE_ALIGN, 4096);
		mmio_write(MMIO_QUEUE_PFN,
			(u32)((u64)(unsigned long)queue_memory >> 12));
		mmio_write(MMIO_STATUS, STATUS_ACKNOWLEDGE | STATUS_DRIVER |
			STATUS_DRIVER_OK);
	}
	arm_virtio_barrier();
	return 0;
}

static int transfer_sector(u64 sector, void *buffer, u32 type)
{
	struct virtq_descriptor *descriptors;
	volatile struct virtq_available *available;
	volatile struct virtq_used *used;
	u16 available_index;
	u16 used_before;
	u32 interrupts;
	unsigned long poll;

	if(!transport || !queue_size || !buffer) {
		return -1;
	}
	descriptors = (struct virtq_descriptor *)queue_memory;
	available = (volatile struct virtq_available *)(queue_memory +
		queue_size * sizeof(struct virtq_descriptor));
	used = (volatile struct virtq_used *)(queue_memory + 4096);
	request.type = type;
	request.reserved = 0;
	request.sector = sector;
	request_status = 0xff;
	if(type == VIRTIO_BLK_T_IN) {
		clear_bytes((u8 *)buffer, SECTOR_SIZE);
	}

	descriptors[0].address = (u64)(unsigned long)&request;
	descriptors[0].length = sizeof(request);
	descriptors[0].flags = VIRTQ_DESC_F_NEXT;
	descriptors[0].next = 1;
	descriptors[1].address = (u64)(unsigned long)buffer;
	descriptors[1].length = SECTOR_SIZE;
	descriptors[1].flags = VIRTQ_DESC_F_NEXT;
	if(type == VIRTIO_BLK_T_IN) {
		descriptors[1].flags |= VIRTQ_DESC_F_WRITE;
	}
	descriptors[1].next = 2;
	descriptors[2].address = (u64)(unsigned long)&request_status;
	descriptors[2].length = 1;
	descriptors[2].flags = VIRTQ_DESC_F_WRITE;
	descriptors[2].next = 0;

	arm_virtio_barrier();
	used_before = used->index;
	available_index = available->index;
	available->ring[available_index % queue_size] = 0;
	arm_virtio_barrier();
	available->index = available_index + 1;
	arm_virtio_barrier();
	mmio_write(MMIO_QUEUE_NOTIFY, 0);

	for(poll = 0; poll < POLL_LIMIT; poll++) {
		arm_virtio_barrier();
		if(used->index != used_before) {
			break;
		}
	}
	arm_virtio_barrier();
	if(poll == POLL_LIMIT || request_status != 0) {
		return -1;
	}
	interrupts = mmio_read(MMIO_INTERRUPT_STATUS);
	if(interrupts) {
		mmio_write(MMIO_INTERRUPT_ACK, interrupts);
	}
	return 0;
}

int arm_virtio_read_sector(u64 sector, void *buffer)
{
	return transfer_sector(sector, buffer, VIRTIO_BLK_T_IN);
}

int arm_virtio_write_sector(u64 sector, void *buffer)
{
	return transfer_sector(sector, buffer, VIRTIO_BLK_T_OUT);
}

int arm_virtio_transport_init(void)
{
	transport = 0;
	transport_version = 0;
	queue_size = 0;
	return find_transport() < 0 || setup_queue() < 0 ? -1 : 0;
}

u64 arm_virtio_capacity_sectors(void)
{
	u64 capacity;

	if(!transport || !queue_size) {
		return 0;
	}
	capacity = mmio_read(MMIO_CONFIG);
	capacity |= (u64)mmio_read(MMIO_CONFIG + 4) << 32;
	return capacity;
}

unsigned int arm_virtio_transport_version(void)
{
	return transport && queue_size ? transport_version : 0;
}
