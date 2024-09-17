/*
 * fiwix/include/fiwix/ps2.h
 *
 * Copyright 2024, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PS2_H
#define _FIWIX_PS2_H

#define PS2_DATA	0x60	/* I/O data port */
#define PS2_STATUS	0x64	/* status register port */
#define PS2_COMMAND	0x64	/* command/control port */

/* status register */
#define PS2_STAT_OUTBUSY	0x01	/* output buffer full, don't read yet */
#define PS2_STAT_INBUSY		0x02	/* input buffer full, don't write yet */
#define PS2_STAT_TXTMOUT	0x20	/* transmit time-out error */
#define PS2_STAT_RXTMOUT	0x40	/* receive time-out error */
#define PS2_STAT_PARERR		0x80	/* parity error */
#define PS2_STAT_COMMERR	(PS2_STAT_TXTMOUT | PS2_STAT_RXTMOUT)

/* controller commands */
#define PS2_CMD_RECV_CONFIG	0x20	/* read controller's config byte */
#define PS2_CMD_SEND_CONFIG	0x60	/* write controller's config byte */
#define PS2_CMD_DISABLE_CH2	0xA7	/* disable second channel (if any) */
#define PS2_CMD_ENABLE_CH2	0xA8	/* enable second channel (if any) */
#define PS2_CMD_TEST_CH2	0xA9	/* test interface second channel */
#define PS2_CMD_SELF_TEST	0xAA	/* self-test command */
#define PS2_CMD_TEST_CH1	0xAB	/* test interface first channel */
#define PS2_CMD_DISABLE_CH1	0xAD	/* disable first channel */
#define PS2_CMD_ENABLE_CH1	0xAE	/* enable first channel */
#define PS2_CMD_GET_IFACE	0xCA	/* get the current interface */
#define PS2_CMD_HOTRESET	0xFE	/* Hot Reset */

/* device commands */
#define PS2_DEV_GETINFO		0xE9	/* get current status information */
#define PS2_KB_SETLED		0xED	/* set/reset status indicators (LEDs) */
#define PS2_KB_ECHO		0xEE	/* echo (for diagnostics only) */
#define PS2_KB_GETSETSCAN	0xF0	/* keyboard get/set scan code */
#define PS2_DEV_IDENTIFY	0xF2	/* device identify (for PS/2 only) */
#define PS2_DEV_RATE		0xF3	/* set typematic rate/delay */
#define PS2_DEV_ENABLE		0xF4	/* keyboard enable scanning */
#define PS2_KB_DISABLE		0xF5	/* keyboard disable scanning */
#define PS2_DEV_RESET		0xFF	/* device reset */

#define DEV_RESET_OK		0xAA	/* self-test passed */
#define DEV_ACK			0xFA	/* acknowledge */

extern volatile unsigned char ack;

int ps2_wait_ack(void);
void ps2_write(const unsigned char, const unsigned char);
unsigned char ps2_read(const unsigned char);
void ps2_clear_buffer(void);
void ps2_init(void);

#endif /* _FIWIX_PS2_H */
