/*
 * fiwix/drivers/net/rtl8139.c
 *
 * Copyright 2026, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/config.h>
#include <fiwix/irq.h>
#include <fiwix/pic.h>
#include <fiwix/sleep.h>
#include <fiwix/pci.h>
#include <fiwix/netdevice.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include "rtl8139.h"

#ifdef CONFIG_NET
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <lwip/prot/ethernet.h>
#include <lwip/snmp.h>
#include <lwip/dhcp.h>

#ifdef CONFIG_PCI
static struct interrupt irq_config_rtl8139 = { 0, NULL, &irq_rtl8139, NULL };
static struct rtl8139 *rtl8139_active = NULL;
/* FIXME: this should be allocated dynamically */
static struct rtl8139 nic = { 0 };

static struct netdevice *rtl8139_enable(struct pci_device *pci_dev)
{
	struct netdevice *nd;
	struct netif *netif;
	struct rtl8139 **nicp;
	int n, speed, mode, ver, hwid;

	if(!(nd = netdevice_alloc())) {
		printk("WARNING: %s(): unable to allocate memory for netdevice structure.", __FUNCTION__);
		return NULL;
	}
	if(!(netif = (struct netif *)kmalloc(sizeof(struct netif)))) {
		printk("WARNING: %s(): unable to allocate memory for netif structure.", __FUNCTION__);
		return NULL;
	}
	memset_b(netif, 0, sizeof(struct netif));

	sprintk(nd->name, "eth%d", ether_count);
	nd->num = if_count;
	nd->flags = IFF_BROADCAST | IFF_RUNNING; /* IFF_UP */
	nd->type = ARPHRD_ETHER;
	nd->family = AF_INET;
	nd->lwip_netif = (void *)netif;
	nd->pci_dev = pci_dev;
	nd->open = &rtl8139_open;
	nd->close = &rtl8139_close;

	nd->ioaddr = pci_dev->bar[0];
	speed = inport_b(nd->ioaddr + MSR) & MSR_SPEED10;
	mode = inport_w(nd->ioaddr + BMCR) & BMCR_DUPLEXMODE;
	printk("%s      0x%04x-0x%04x   %3d", nd->name, nd->ioaddr, nd->ioaddr + 0x7F, pci_dev->irq);
	printk("\tFast Ethernet Realtek");
	ver = inport_l(nd->ioaddr + TCR);
	hwid = ((ver >> (TCR_HWVERID_A - 2)) & 0x7F) | ((ver >> TCR_HWVERID_B) & 3);
	switch(hwid) {
		case 112:
			printk(" RTL8139A\n");
			break;
		case 116:
			printk(" RTL8139C\n");
			break;
		case 117:
			printk(" RTL8100B/8139D\n");
			break;
		case 118:
			printk(" RTL8139C+\n");
			break;
		case 119:
			printk(" RTL8101\n");
			break;
		case 120:
			printk(" RTL8130/8139B\n");
			break;
		case 122:
			printk(" RTL8100\n");
			break;
		default:
			printk(" RTL8139\n");
			break;
	}
	printk("\t\t\t\tmac=");
	for(n = 0; n < NETIF_MAX_HWADDR_LEN; n++) {
		nd->mac[n] = inport_b(nd->ioaddr + IDR + n);
		printk("%02x", nd->mac[n]);
		if(n < (NETIF_MAX_HWADDR_LEN - 1)) {
			printk(":");
		}
	}
	printk(", %sMbps, %s-duplex\n", speed ? "10" : "100", mode ? "full" : "half");
	pci_show_desc(pci_dev);

	/* enable I/O space and bus master */
	pci_dev->command |= (PCI_COMMAND_IO | PCI_COMMAND_MASTER);
	pci_dev->command &= ~PCI_COMMAND_INT_DISABLE;
	pci_write_short(pci_dev, PCI_COMMAND, pci_dev->command);

	irq_config_rtl8139.name = nd->name;
	if(!register_irq(pci_dev->irq, &irq_config_rtl8139)) {
		enable_irq(pci_dev->irq);
	}
	register_netdevice(nd);
	memset_b(&nic, 0, sizeof(struct rtl8139));
	nicp = &rtl8139_active;
	/* multiple rtl8139 NICs are not supported yet
	if(*nicp) {
		do {
			nicp = &(*nicp)->next;
		} while(*nicp);
	}
	*/
	nic.nd = nd;
	*nicp = &nic;
	return nd;
}

static void rtl8139_reset(struct netdevice *nd)
{
	int n;

	outport_b(nd->ioaddr + CMD, CMD_RESET);
	for(n = 10000; n > 0; n--) {
		if(!(inport_b(nd->ioaddr + CMD) & CMD_RESET)) {
			break;
		}
	}
	if(!n) {
		printk("WARNING: %s(): reset not completed.", __FUNCTION__);
	}
	nic.tx_index = nic.tx_sent = 0;
}

static void rtl8139_rx(struct netif *netif)
{
	struct netdevice *nd;
	struct pbuf *p;
	struct packet *pk;
	int offset, frag;
	unsigned short int *header, status, size;

	nd = nic.nd;

	while((inport_b(nd->ioaddr + CMD) & CMD_RXBUFEMPTY) == 0) {
		offset = nic.rx_index;
		header = (unsigned short int *)(nic.rx_buffer + offset);
		status = header[0];
		size = header[1];
		offset += (sizeof(unsigned short int)) * 2;

		if(status & (ST_RX_FAE | ST_RX_CRC | ST_RX_LONG | ST_RX_RUNT | ST_RX_ISE)) {
			printk("WARNING: %s(): %s packet error: 0x%x\n", nd->name, status);
		}
#if ETH_PAD_SIZE
		/* allow room for Ethernet padding */
		if(!(p = pbuf_alloc(PBUF_RAW, size + ETH_PAD_SIZE, PBUF_RAM))) {
#else
		if(!(p = pbuf_alloc(PBUF_RAW, size, PBUF_RAM))) {
#endif /* ETH_PAD_SIZE */
			printk("WARNING: %s(): unable to allocate memory for pbuf structure.", __FUNCTION__);
			break;
		}
#if ETH_PAD_SIZE
		pbuf_remove_header(p, ETH_PAD_SIZE);	/* drop the padding word */
#endif /* ETH_PAD_SIZE */
		if(offset + size > RX_BUFFER_SIZE) {
			frag = RX_BUFFER_SIZE - offset;
			memcpy_b(p->payload, &nic.rx_buffer[offset], frag);
			memcpy_b(p->payload + frag, &nic.rx_buffer, size - frag);
		} else {
			memcpy_b(p->payload, &nic.rx_buffer[offset], size);
		}
		p->len = size;

		if(!(pk = (struct packet *)kmalloc(sizeof(struct packet)))) {
			printk("WARNING: %s(): unable to allocate memory for packet structure.", __FUNCTION__);
			return;
		}
		memset_b(pk, 0, sizeof(struct packet));
		pk->lwip_netif = netif;
		pk->lwip_pbuf = p;
		append_packet_to_queue(pk, &nd->queue);

		nic.rx_index = ((nic.rx_index + size + 4 + 3) & ~3) % RX_BUFFER_SIZE;
		outport_w(nd->ioaddr + CAPR, nic.rx_index - 16);
		netdevice_bh.flags |= BH_ACTIVE;
	}
}

static void rtl8139_tx_end(void)
{
	struct netdevice *nd;
	unsigned int status;
	int entry;

	nd = nic.nd;
	while(nic.tx_sent < nic.tx_index || nic.tx_full) {
		entry = nic.tx_sent % NUM_TX_DESC;
		status = inport_l(nd->ioaddr + TSD0 + (entry * 4));
		if(!(status & (TSD_TOK | TSD_TUN | TSD_TABT))) {
			break;
		}
		if(status & (TSD_TOK | TSD_OWN)) {
			if(nic.tx_full) {
				nic.tx_full = 0;
			}
		}
		nic.tx_sent++;
	}
}

static err_t rtl8139_lwip_send(struct netif *netif, struct pbuf *p)
{
	struct netdevice *nd;
	char *data;
	int size, entry;

	if(nic.tx_full) {
		return ERR_IF;
	}
	if(can_lock_area(AREA_NETDEVICE)) {
#if ETH_PAD_SIZE
		pbuf_remove_header(p, ETH_PAD_SIZE);	/* drop the padding word */
#endif /* ETH_PAD_SIZE */
		size = LWIP_MIN(TX_BUFFER_SIZE, p->tot_len);
		entry = nic.tx_index % NUM_TX_DESC;
		if(!(data = pbuf_get_contiguous(p, nic.tx_buffer[entry], TX_BUFFER_SIZE, size, 0))) {
			printk("WARNING: %s(): pbuf_get_contiguous() returned NULL.\n", __FUNCTION__);
			return ERR_IF;
		}
		nd = nic.nd;
		if((unsigned int)data & 3) {
			memcpy_b(nic.tx_buffer[entry], data, size);
			outport_l(nd->ioaddr + TSAD0 + (entry * 4), (unsigned int)V2P(nic.tx_buffer[entry]));
		} else {
			outport_l(nd->ioaddr + TSAD0 + (entry * 4), (unsigned int)V2P(data));
		}
		outport_l(nd->ioaddr + TSD0 + (entry * 4), TSD_TXFIFO_THR | size);
		if(++nic.tx_index - nic.tx_sent == NUM_TX_DESC) {
			nic.tx_full = 1;
		}

		MIB2_STATS_NETIF_ADD(netif, ifoutoctets, size);
		MIB2_STATS_NETIF_INC(netif, ifoutucastpkts);	/* always unicast packets? */
#if ETH_PAD_SIZE
		pbuf_add_header(p, ETH_PAD_SIZE);	/* reclaim the padding word */
#endif /* ETH_PAD_SIZE */
		LINK_STATS_INC(link.xmit);
		unlock_area(AREA_NETDEVICE);
		return ERR_OK;
	} else {
		return ERR_IF;
	}
}

int rtl8139_open(struct netdevice *nd)
{
	struct netif *netif;
	unsigned int addr;

	outport_b(nd->ioaddr + CR9346, CR9346_EEM10);
	outport_b(nd->ioaddr + CONFIG1, 0x0);
	outport_b(nd->ioaddr + CR9346, 0x0);

	rtl8139_reset(nd);

	/* set the rx buffer */
	outport_l(nd->ioaddr + RBSTART, (unsigned int)V2P(nic.rx_buffer));

	/* enable transmitter and receiver */
	outport_b(nd->ioaddr + CMD, CMD_TXENABLE | CMD_RXENABLE);

	/* TX DMA burst size to 1024 bytes */
	outport_l(nd->ioaddr + TCR, DMA_BURST | 0x03000000);

	outport_b(nd->ioaddr + CR9346, CR9346_EEM10);
	outport_b(nd->ioaddr + CONFIG1, 0x20);
	outport_b(nd->ioaddr + CR9346, 0x0);

	outport_l(nd->ioaddr + MPC, 0);
	outport_l(nd->ioaddr + RCR, RCR_RXFIFO_THR | DMA_BURST | RCR_AB | RCR_AM | RCR_APM);

	outport_b(nd->ioaddr + CR9346, CR9346_EEM10);
	memcpy_l(&addr, &nd->mac[0], sizeof(unsigned int));
	outport_l(nd->ioaddr + IDR + 0, addr);
	memcpy_l(&addr, &nd->mac[4], sizeof(unsigned int));
	outport_l(nd->ioaddr + IDR + 4, addr);
	outport_b(nd->ioaddr + CR9346, 0x0);

	outport_l(nd->ioaddr + MAR0 + 0, 0xFFFFFFFF);
	outport_l(nd->ioaddr + MAR0 + 4, 0xFFFFFFFF);

	/* enable transmitter and receiver */
	outport_b(nd->ioaddr + CMD, CMD_TXENABLE | CMD_RXENABLE);

	/* enable all interrupts and clear all pending */
	outport_w(nd->ioaddr + IMR, IMR_ALLINT);
	outport_w(nd->ioaddr + ISR, 0xFFFF);

	netif = nd->lwip_netif;
	netif->flags |= NETIF_FLAG_LINK_UP;
	netif_set_link_up(netif);
	return 0;
}

int rtl8139_close(struct netdevice *nd)
{
	struct netif *netif;

	/* disable all interrupts */
	outport_w(nd->ioaddr + IMR, 0);

	/* disable transmitter and receiver */
	outport_b(nd->ioaddr + CMD, 0);

	netif = nd->lwip_netif;
	netif->flags &= ~NETIF_FLAG_LINK_UP;
	netif_set_link_down(netif);
	return 0;
}

void irq_rtl8139(int num, struct sigcontext *sc)
{
	struct rtl8139 *nicp;
	struct netdevice *nd;
	int status;

	nicp = rtl8139_active;
	/* multiple rtl8139 NICs are not supported yet
	while(nicp) {
		nicp = nicp->next;
	}
	*/
	nd = nicp->nd;
	status = inport_w(nd->ioaddr + ISR);
	outport_w(nd->ioaddr + ISR, status);	/* ack interrupt bits */
	if(status & (IMR_ROK | IMR_RER)) {
		rtl8139_rx(nd->lwip_netif);
	}
	if(status & (IMR_TOK | IMR_TER)) {
		rtl8139_tx_end();
	}
	if(status & IMR_LENCHG) {
		printk("%s(): %s: cable length change detected.\n", __FUNCTION__, nd->name);
	}
	if(status & (IMR_RXOVW | IMR_PUN | IMR_FOVW)) {
		printk("WARNING: %s(): %s: receive error: status = 0x%x\n", __FUNCTION__, nd->name, status);
	}
	if(status & (IMR_TIMEOUT | IMR_SERR)) {
		printk("WARNING: %s(): %s: error: status = 0x%x\n", __FUNCTION__, nd->name, status);
	}
}

err_t rtl8139_lwip_init(struct netif *netif)
{
	struct netdevice *nd;
	int n;

	nd = (struct netdevice *)netif->state;
	netif->name[0] = 'e';
	netif->name[1] = '0' + ether_count;
	netif->hwaddr_len = ETH_HWADDR_LEN;
	for(n = 0; n < NETIF_MAX_HWADDR_LEN; n++) {
		netif->hwaddr[n] = nd->mac[n];
	}
	netif->mtu = 1500;	/* TCP_MSS + 40 actually */
	netif->flags |= NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP;
#if LWIP_IPV4
	netif->output = etharp_output;
#endif /* LWIP_IPV4 */
	netif->linkoutput = rtl8139_lwip_send;

	netif_set_default(netif);
	netif_set_up(netif);
	return ERR_OK;
}

void rtl8139_init(struct pci_device *pci_dev)
{
	struct netdevice *nd;

	/* multiple rtl8139 NICs are not supported yet */
	if(nic.nd) {
		printk("WARNING: %s(): multiple rtl8139 NICs are not supported yet.\n", __FUNCTION__);
		return;
	}
	if(!(nd = rtl8139_enable(pci_dev))) {
		return;
	}
	netif_add(nd->lwip_netif, NULL, NULL, NULL, (void *)nd, rtl8139_lwip_init, tcpip_input);
	if_count++;
	ether_count++;
}
#endif /* CONFIG_PCI */
#endif /* CONFIG_NET */
