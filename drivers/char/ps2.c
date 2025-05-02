/*
 * fiwix/drivers/char/ps2.c
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/kparms.h>
#include <fiwix/config.h>
#include <fiwix/ps2.h>
#include <fiwix/keyboard.h>
#include <fiwix/psaux.h>
#include <fiwix/stdio.h>

/*
 * PS/2 System Control Port A bits
 * -------------------------------
 * #7 RW:fixed disk activity led
 * #6 RW:fixed disk activity led
 * #5 RW:reserved
 * #4 RW:watchdog timer status
 * #3 RW:security lock latch
 * #2 RW:reserved
 * #1 RW:alternate gate A20
 * #0 RW:alternate hot reset
 */
#define PS2_SYSCTRL_A	0x92	/* PS/2 system control port A (R/W) */

extern volatile unsigned char ack;

/* wait controller output buffer to be full or for controller acknowledge */
static int is_ready_to_read(void)
{
	int n;

	for(n = 0; n < PS2_READ_TIMEOUT; n++) {
		if(ack) {
			return 1;
		}
		if(inport_b(PS2_STATUS) & PS2_STAT_OUTBUSY) {
			return 1;
		}
	}
	printk("WARNING: PS/2 controller not ready to read.\n");
	return 0;
}

/* wait controller input buffer to be clear */
static int is_ready_to_write(void)
{
	int n;

	for(n = 0; n < PS2_WRITE_TIMEOUT; n++) {
		if(!(inport_b(PS2_STATUS) & PS2_STAT_INBUSY)) {
			return 1;
		}
	}
	return 0;
}

static void ps2_delay(void)
{
	int n;

	for(n = 0; n < 1000; n++) {
		NOP();
	}
}

int ps2_wait_ack(void)
{
	int n;

	if(is_ready_to_read()) {
		for(n = 0; n < 1000; n++) {
			if(inport_b(PS2_DATA) == DEV_ACK) {
				return 0;
			}
			ps2_delay();
		}
	}
	return 1;
}

void ps2_write(const unsigned char port, const unsigned char byte)
{
	ack = 0;

	if(is_ready_to_write()) {
		outport_b(port, byte);
	}
}

unsigned char ps2_read(const unsigned char port)
{
	if(is_ready_to_read()) {
		return inport_b(port);
	}
	return 0;
}

void ps2_clear_buffer(void)
{
	int n;

	for(n = 0; n < 1000; n++) {
		ps2_delay();
		if(inport_b(PS2_STATUS) & PS2_STAT_OUTBUSY) {
			ps2_read(PS2_DATA);
			continue;
		}
		break;
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
	config = ps2_read(PS2_DATA);

	if(!kparms.ps2_noreset) {
		/* set controller configuration (disabling IRQs) */
		ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
		ps2_write(PS2_DATA, config & ~(0x01 | 0x02));

		/* PS/2 controller self-test */
		ps2_write(PS2_COMMAND, PS2_CMD_SELF_TEST);
		if((errno = ps2_read(PS2_DATA)) != 0x55) {
			printk("WARNING: %s(): PS/2 controller not returned 0x55 after self-test (was 0x%x). Try with the parameter 'ps2_noreset=1'.\n", __FUNCTION__, errno);
			return;
		} else {
			supp_ports = 1;
		}

		/*
		 * This sets again the controller configuration since the previous
		 * step may also reset the PS/2 controller to its power-on defaults.
		 */
		ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
		ps2_write(PS2_DATA, config);
	}

	/* double-check if we have a second channel */
	if(config & 0x20) {
		ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH2);
		ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
		if(!(ps2_read(PS2_DATA) & 0x20)) {
			supp_ports = 1 + (config & 0x20 ? 1 : 0);
		}
		ps2_write(PS2_COMMAND, PS2_CMD_DISABLE_CH2);
	}

	/* test interface first channel */
	ps2_write(PS2_COMMAND, PS2_CMD_TEST_CH1);
	if((errno = ps2_read(PS2_DATA)) != 0) {
		printk("WARNING: %s(): test in first PS/2 interface returned 0x%x.\n", __FUNCTION__, errno);
	}

	if(supp_ports > 1) {
		/* test interface second channel */
		ps2_write(PS2_COMMAND, PS2_CMD_TEST_CH2);
		if((errno = ps2_read(PS2_DATA)) != 0) {
			printk("WARNING: %s(): test in second PS/2 interface returned 0x%x.\n", __FUNCTION__, errno);
		}
	}

	/* check if it is a type 1 or type 2 controller */
	config2 = 0;
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	config = ps2_read(PS2_DATA);	/* save state */
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config | 0x40);	/* set translation */
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	config2 = ps2_read(PS2_DATA);
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config);	/* restore state */
	type = (config2 & 0x40) ? 1 : 0;

	/* enable interrupts if channels are present */
	config |= 0x01;
	if(supp_ports > 1) {
		config |= 0x02;
	}
	config |= 0x40;	/* allow translation */
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config);

	/* get current interface */
	ps2_clear_buffer();
	ps2_write(PS2_COMMAND, PS2_CMD_GET_IFACE);
	iface = ps2_read(PS2_DATA) & 0x01;

	printk("ps/2      0x%04x,0x%04x     -\t%s type=%d, channels=%d\n", PS2_DATA, PS2_COMMAND, iface == 1 ? "(MCA) PS/2" : "(ISA) AT", type, supp_ports);

	/* enable device(s) */
	ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH1);
	keyboard_init();
#ifdef CONFIG_PSAUX
	if(supp_ports > 1) {
		ps2_write(PS2_COMMAND, PS2_CMD_ENABLE_CH2);
		psaux_init();
	}
#endif /* CONFIG_PSAUX */
}
