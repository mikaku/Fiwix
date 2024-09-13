/*
 * fiwix/drivers/char/ps2.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/ps2.h>
#include <fiwix/keyboard.h>
#include <fiwix/psaux.h>
#include <fiwix/stdio.h>

/*
 * PS/2 System Control Port A
 * --------------------------------
 * bit 7 -> fixed disk activity led
 * bit 6 -> fixed disk activity led
 * bit 5 -> reserved
 * bit 4 -> watchdog timer status
 * bit 3 -> security lock latch
 * bit 2 -> reserved
 * bit 1 -> alternate gate A20
 * bit 0 -> alternate hot reset
 */
#define PS2_SYSCTRL_A	0x92	/* PS/2 system control port A (write) */

#define PS2_TIMEOUT	500000

extern volatile unsigned char ack;

/* wait controller input buffer to be clear */
static int is_ready_to_write(void)
{
	int n;

	for(n = 0; n < PS2_TIMEOUT; n++) {
		if(!(inport_b(PS2_STATUS) & PS2_STAT_INBUSY)) {
			return 1;
		}
	}
	return 0;
}

/* wait controller output buffer to be full or for controller acknowledge */
int is_ready_to_read(void)
{
	int n, value;

	for(n = 0; n < PS2_TIMEOUT; n++) {
		if(ack) {
			return 1;
		}
		if((value = inport_b(PS2_STATUS)) & PS2_STAT_OUTBUSY) {
			if(value & (PS2_STAT_COMMERR | PS2_STAT_PARERR)) {
				continue;
			}
			return 1;
		}
	}
	return 0;
}

void ps2_write(const unsigned char port, const unsigned char byte)
{
	ack = 0;

	if(is_ready_to_write()) {
		outport_b(port, byte);
	}
}

void reboot(void)
{
	CLI();
	ps2_write(PS2_SYSCTRL_A, 0x01);			/* Fast Hot Reset */
	ps2_write(PS2_COMMAND, PS2_CMD_HOTRESET);	/* Hot Reset */
	HLT();
}

void ps2_init(void)
{
	int errno;
	unsigned char config, config2;
	char type, supp_ports, iface;

	type = supp_ports = iface = 0;

	/* disable device(s) */
	ps2_write(PS2_COMMAND, PS2_CMD_DISABLE_CH1);
	ps2_write(PS2_COMMAND, PS2_CMD_DISABLE_CH2);

	/* flush buffer */
	inport_b(PS2_DATA);

	/* get controller configuration */
	config = 0;
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	if(is_ready_to_read()) {
		config = inport_b(PS2_DATA);
	}

	/* set controller configuration (disabling IRQs) */
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config & ~(0x01 | 0x02 | 0x40));

	/* PS/2 controller self-test */
	ps2_write(PS2_COMMAND, PS2_CMD_SELF_TEST);
	if(is_ready_to_read()) {
		if((errno = inport_b(PS2_DATA)) != 0x55) {
			printk("WARNING: %s(): PS/2 controller returned 0x%x in self-test (expected 0x55).\n", __FUNCTION__, errno);
			return;
		} else {
			supp_ports = 1;
		}
	} else {
		printk("WARNING: %s(): PS/2 controller not ready.\n", __FUNCTION__);
		return;
	}

	/* double-check if we have a second channel */
	if(config & 0x20) {
		ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH2);
		ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
		if(is_ready_to_read()) {
			if(!(inport_b(PS2_DATA) & 0x20)) {
				supp_ports = 1 + (config & 0x20 ? 1 : 0);
			}
		}
		ps2_write(PS2_COMMAND, PS2_CMD_DISABLE_CH2);
	}

	/* test interface first channel */
	ps2_write(PS2_COMMAND, PS2_CMD_TEST_CH1);
	if(is_ready_to_read()) {
		if((errno = inport_b(PS2_DATA)) != 0) {
			printk("WARNING: %s(): test in first PS/2 interface returned 0x%x.\n", __FUNCTION__, errno);
		}
	}

	if(supp_ports > 1) {
		/* test interface second channel */
		ps2_write(PS2_COMMAND, PS2_CMD_TEST_CH2);
		if(is_ready_to_read()) {
			if((errno = inport_b(PS2_DATA)) != 0) {
				printk("WARNING: %s(): test in second PS/2 interface retrurned 0x%x.\n", __FUNCTION__, errno);
			}
		}
	}

	/* check if it is a type 1 or type 2 controller */
	config2 = 0;
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	if(is_ready_to_read()) {
		config = inport_b(PS2_DATA);	/* save state */
	}
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config | 0x40);	/* set translation */
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	if(is_ready_to_read()) {
		config2 = inport_b(PS2_DATA);
	}
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config);	/* restore state */
	type = (config2 & 0x40) ? 1 : 0;

	/* enable interrupts if channels are present */
	if(supp_ports) {
		config = 0x01;
		if(supp_ports > 1) {
			config |= 0x02;
		}
		config |= 0x40;	/* allow translation */
	} else {
		config = 0x10 + 0x20;	/* disable clocks */
	}
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config);

	/* get current interface */
	ps2_write(PS2_COMMAND, PS2_CMD_GET_IFACE);
	if(is_ready_to_read()) {
		iface = inport_b(PS2_DATA) & 0x01;
	}

	printk("ps/2      0x%04x,0x%04x     \t%s type=%d, channels=%d\n", PS2_DATA, PS2_COMMAND, iface == 1 ? "(MCA) PS/2" : "(ISA) AT", type, supp_ports);

	/* enable device(s) */
	if(supp_ports) {
		ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH1);
		keyboard_init();
		if(supp_ports > 1) {
			ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH2);
			psaux_init();
		}
	}
}
