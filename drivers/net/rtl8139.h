/*
 * fiwix/drivers/net/rtl8139.h
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifdef CONFIG_NET
#ifdef CONFIG_PCI

#include <fiwix/sleep.h>
#include <lwip/netif.h>

/* register offsets */
#define IDR		0x00	/* MAC address */
#define MAR0		0x08	/* Multicast Register 0 */
#define TSD0		0x10	/* Transmit Status of Descriptor 0 */
#define TSAD0		0x20	/* Transmit Start Address of Descriptor 0 */
#define RBSTART		0x30	/* Receive (Rx) Buffer Start address */
#define CMD		0x37	/* Command register */
#define CAPR		0x38	/* Current Address of Packet Read */
#define IMR		0x3C	/* Interrupt Mask Register */
#define ISR		0x3E	/* Interrupt Status Register */
#define TCR		0x40	/* Transmit Configuration Register */
#define RCR		0x44	/* Receive Configuration Register */
#define MPC		0x4C	/* Missed Packet Counter */
#define CR9346		0x50	/* 93C46 Command Register */
#define CONFIG1		0x52	/* Configuration register 1 */
#define MSR		0x58	/* Media Status Register */
#define BMCR		0x62	/* Basic Mode Control Register */

/* bit masks */
#define TSD_TXFIFO_THR	(8 << 16)	/* 8 * 32 = 256 bytes */
#define TSD_OWN		0x2000		/* OWN */
#define TSD_TUN		0x4000		/* Transmit FIFO Underrun */
#define TSD_TOK		0x8000		/* Transmit OK */
#define TSD_OWC		0x20000000	/* Out of Window Collision */
#define TSD_TABT	0x40000000	/* Transmit Abort */
#define TSD_CRS		0x80000000	/* Carrier Sense Lost */
#define CMD_RXBUFEMPTY	0x01		/* buffer empty */
#define CMD_TXENABLE	0x04		/* transmitter enable */
#define CMD_RXENABLE	0x08		/* receiver enable */
#define CMD_RESET	0x10		/* reset */
#define IMR_ROK		0x01		/* Receive OK Interrupt */
#define IMR_RER		0x02		/* Receive Error Interrupt */
#define IMR_TOK		0x04		/* Transmit OK Interrupt */
#define IMR_TER		0x08		/* Transmit Error Interrupt */
#define IMR_RXOVW	0x10		/* RX Buffer Overflow Interrupt */
#define IMR_PUN		0x20		/* Packet Underrun Interrupt */
#define IMR_FOVW	0x40		/* RX FIFO Overflow Interrupt */
#define IMR_LENCHG	0x2000		/* Cable Length Change Interrupt */
#define IMR_TIMEOUT	0x4000		/* Time Out Interrupt */
#define IMR_SERR	0x8000		/* System Error Interrupt */
#define IMR_ALLINT	(IMR_ROK | IMR_RER | IMR_TOK | IMR_TER | IMR_RXOVW | \
			IMR_PUN | IMR_FOVW | IMR_TIMEOUT | IMR_SERR)
#define TCR_HWVERID_A	26
#define TCR_HWVERID_B	22
#define RCR_APM		0x02		/* Accept Physical Match packets */
#define RCR_AM		0x04		/* Accept Multicast packets */
#define RCR_AB		0x08		/* Accept Broadcast packets */
#define RCR_RXFIFO_THR	(7 << 13)	/* 111 = no RX FIFO threshold */
#define ISR_ROK		0x01		/* Receive (Rx) OK */
#define ISR_TOK		0x04		/* Transmit (Tx) OK */
#define CR9346_EEM10	0xC0		/* CONFIG regiters write enable */
#define MSR_SPEED10	0x08		/* 0=100Mbps, 1=10Mbps */
#define BMCR_DUPLEXMODE	0x100		/* 0=half, 1=full */
#define ST_RX_ROK	0x01		/* Receive OK */
#define ST_RX_FAE	0x02		/* Frame Alignment Error */
#define ST_RX_CRC	0x04		/* CRC Error */
#define ST_RX_LONG	0x08		/* Long Packet */
#define ST_RX_RUNT	0x10		/* Runt Packet Received */
#define ST_RX_ISE	0x20		/* Invalid Symbol Error */

#define DMA_BURST	(6 << 8)	/* TX/RX DMA burst size (1024 bytes) */
#define RX_BUFFER_SIZE	8192
#define TX_BUFFER_SIZE	1536
#define NUM_TX_DESC	4

struct rtl8139 {
	struct netdevice *nd;
	unsigned char rx_buffer[RX_BUFFER_SIZE + 16];
	unsigned char tx_buffer[NUM_TX_DESC][TX_BUFFER_SIZE];
	unsigned int tx_index, tx_sent;
	int rx_index, tx_full;
	struct resource send;
/*	struct rtl8139 *next;*/
};

int rtl8139_open(struct netdevice *);
int rtl8139_close(struct netdevice *);
void irq_rtl8139(int, struct sigcontext *);
err_t rtl8139_lwip_init(struct netif *);

#endif /* CONFIG_PCI */
#endif /* CONFIG_NET */
