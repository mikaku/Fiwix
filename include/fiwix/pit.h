/*
 * fiwix/include/fiwix/pit.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_PIT_H
#define _FIWIX_PIT_H

/* Intel 8253/82c54 Programmable Interval Timer */

#define OSCIL		1193182	/* oscillator frequency */

#define MODEREG		0x43	/* mode/command register (w) */
#define CHANNEL0	0x40	/* channel 0 data port (rw) */
#define CHANNEL2	0x42	/* channel 2 data port (rw) */

#define BINARY_CTR	0x00	/* 16bit binary mode counter */
#define TERM_COUNT	0x00	/* mode 0 (Terminal Count) */
#define RATE_GEN	0x04	/* mode 2 (Rate Generator) */
#define SQUARE_WAVE	0x06	/* mode 3 (Square Wave) */
#define LSB_MSB		0x30	/* LSB then MSB */
#define SEL_CHAN0	0x00	/* select channel 0 */
#define SEL_CHAN2	0x80	/* select channel 2 */

/*
 * PS/2 System Control Port B
 * ---------------------------------------
 * bit 7 -> IRQ=1, 0=reset
 * bit 6 -> reserved
 * bit 5 -> reserved
 * bit 4 -> reserved
 * bit 3 -> channel check enable
 * bit 2 -> parity check enable
 * bit 1 -> speaker data enable
 * bit 0 -> timer 2 gate to speaker enable
 */
#define PS2_SYSCTRL_B	0x61	/* PS/2 system control port B (write) */

#define ENABLE_TMR2G	0x01	/* timer 2 gate to speaker enable */
#define ENABLE_SDATA	0x02	/* speaker data enable */

#define BEEP_FREQ	900	/* 900Hz */

void pit_beep_on(void);
void pit_beep_off(unsigned int);
int pit_getcounter0(void);
void pit_init(unsigned short int);

#endif /* _FIWIX_PIT_H */
