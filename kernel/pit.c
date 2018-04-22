/*
 * fiwix/kernel/pit.c
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/pit.h>

void pit_beep_on(void)
{
	outport_b(MODEREG, SEL_CHAN2 | LSB_MSB | SQUARE_WAVE | BINARY_CTR);
	outport_b(CHANNEL2, (OSCIL / BEEP_FREQ) & 0xFF);	/* LSB */
	outport_b(CHANNEL2, (OSCIL / BEEP_FREQ) >> 8);		/* MSB */
	outport_b(PS2_SYSCTRL_B, inport_b(PS2_SYSCTRL_B) | ENABLE_SDATA | ENABLE_TMR2G);
}

void pit_beep_off(unsigned int unused)
{
	outport_b(PS2_SYSCTRL_B, inport_b(PS2_SYSCTRL_B) & ~(ENABLE_SDATA | ENABLE_TMR2G));
}

void pit_init(unsigned short int hertz)
{
	outport_b(MODEREG, SEL_CHAN0 | LSB_MSB | RATE_GEN | BINARY_CTR);
	outport_b(CHANNEL0, (OSCIL / hertz) & 0xFF);	/* LSB */
	outport_b(CHANNEL0, (OSCIL / hertz) >> 8);	/* MSB */
}
