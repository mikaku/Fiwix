/*
 * fiwix/drivers/char/keyboard.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/limits.h>
#include <fiwix/keyboard.h>
#include <fiwix/console.h>
#include <fiwix/vgacon.h>
#include <fiwix/pic.h>
#include <fiwix/irq.h>
#include <fiwix/signal.h>
#include <fiwix/process.h>
#include <fiwix/sleep.h>
#include <fiwix/kd.h>
#include <fiwix/sysrq.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define KB_DATA		0x60	/* I/O data port */
#define KBC_COMMAND	0x64	/* command/control port */
#define KBC_STATUS 	0x64	/* status register port */

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

#define KB_CMD_RESET	0xFF	/* keyboard reset */
#define KB_CMD_ENABLE	0xF4	/* keyboard enable scanning */
#define KB_CMD_DISABLE	0xF5	/* keyboard disable scanning */
#define KB_CMD_IDENTIFY	0xF2	/* keyboard identify (for PS/2 only) */
#define KB_CMD_ECHO	0xEE	/* echo (for diagnostics only) */

#define KBC_CMD_RECV_CONFIG	0x20	/* read controller's config byte */
#define KBC_CMD_SEND_CONFIG	0x60	/* write controller's config byte */
#define KBC_CMD_SELF_TEST	0xAA	/* self-test command */
#define KBC_CMD_PS2_1_TEST	0xAB	/* first PS/2 interface test command */
#define KBC_CMD_PS2_2_TEST	0xA9	/* second PS/2 interface test command */
#define KBC_CMD_DISABLE_PS2_1	0xAD	/* disable first PS/2 port */
#define KBC_CMD_ENABLE_PS2_1	0xAE	/* enable first PS/2 port */
#define KBC_CMD_DISABLE_PS2_2	0xA7	/* disable second PS/2 port (if any) */
#define KBC_CMD_ENABLE_PS2_2	0xA8	/* enable second PS/2 port (if any) */
#define KBC_CMD_HOTRESET	0xFE	/* Hot Reset */

/* flags of the status register */
#define KB_STR_OUTBUSY	0x01	/* output buffer full, don't read yet */
#define KB_STR_INBUSY	0x02	/* input buffer full, don't write yet */
#define KB_STR_TXTMOUT	0x20	/* transmit time-out error */
#define KB_STR_RXTMOUT	0x40	/* receive time-out error */
#define KB_STR_PARERR	0X80	/* parity error */
#define KB_STR_COMMERR	(KB_STR_TXTMOUT | KB_STR_RXTMOUT)

#define KB_RESET_OK	0xAA	/* self-test passed */
#define KB_ACK		0xFA	/* acknowledge */
#define KB_SETLED	0xED	/* set/reset status indicators (LEDs) */
#define KB_RATE		0xF3	/* set typematic rate/delay */
#define DELAY_250	0x00	/* typematic delay at 250ms (default) */
#define DELAY_500	0x40	/* typematic delay at 500ms */
#define DELAY_750	0x80	/* typematic delay at 750ms */
#define DELAY_1000	0xC0	/* typematic delay at 1000ms */
#define RATE_30		0x00	/* typematic rate at 30.0 reports/sec (default) */

#define EXTKEY		0xE0	/* extended key (AltGr, Ctrl-Print, etc.) */

__key_t *keymap_line;

static unsigned char e0_keys[128] = {
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x00-0x07 */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x08-0x0F */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x10-0x17 */
	0, 0, 0, 0, E0ENTER, RCTRL, 0, 0,		/* 0x18-0x1F */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x20-0x27 */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x28-0x2F */
	0, 0, 0, 0, 0, E0SLASH, 0, 0,			/* 0x30-0x37 */
	ALTGR, 0, 0, 0, 0, 0, 0, 0,			/* 0x38-0x3F */
	0, 0, 0, 0, 0, 0, 0, E0HOME,			/* 0x40-0x47 */
	E0UP, E0PGUP, 0, E0LEFT, 0, E0RIGHT, 0, E0END,	/* 0x48-0x4F */
	E0DOWN, E0PGDN, E0INS, E0DEL, 0, 0, 0, 0,	/* 0x50-0x57 */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x58-0x5f */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x60-0x67 */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x68-0x6F */
	0, 0, 0, 0, 0, 0, 0, 0,				/* 0x70-0x77 */
	0, 0, 0, 0, 0, 0, 0, 0				/* 0x78-0x7F */
};

static unsigned char leds = 0;
static unsigned char shift = 0;
static unsigned char altgr = 0;
static unsigned char ctrl = 0;
static unsigned char alt = 0;
static unsigned char extkey = 0;
static unsigned char deadkey = 0;
static unsigned char sysrq = 0;
static int sysrq_op = 0;
static volatile unsigned char ack = 0;

static char do_switch_console = -1;
static unsigned char do_buf_scroll = 0;
static unsigned char do_setleds = 0;
static unsigned char do_tty_stop = 0;
static unsigned char do_tty_start = 0;

unsigned char kb_identify[2] = {0, 0};
char ps2_active_ports = 0;
char ps2_supp_ports = 0;
char ctrl_alt_del = 1;
char any_key_to_reboot = 0;

static struct bh keyboard_bh = { 0, &irq_keyboard_bh, NULL };
static struct interrupt irq_config_keyboard = { 0, "keyboard", &irq_keyboard, NULL };

struct diacritic *diacr;
static char *diacr_chars = "`'^ \"";
struct diacritic grave_table[NR_DIACR] = {
	{ 'A', '\300' },
	{ 'E', '\310' },
	{ 'I', '\314' },
	{ 'O', '\322' },
	{ 'U', '\331' },
	{ 'a', '\340' },
	{ 'e', '\350' },
	{ 'i', '\354' },
	{ 'o', '\362' },
	{ 'u', '\371' },
};
struct diacritic acute_table[NR_DIACR] = {
	{ 'A', '\301' },
	{ 'E', '\311' },
	{ 'I', '\315' },
	{ 'O', '\323' },
	{ 'U', '\332' },
	{ 'a', '\341' },
	{ 'e', '\351' },
	{ 'i', '\355' },
	{ 'o', '\363' },
	{ 'u', '\372' },
};
struct diacritic circm_table[NR_DIACR] = {
	{ 'A', '\302' },
	{ 'E', '\312' },
	{ 'I', '\316' },
	{ 'O', '\324' },
	{ 'U', '\333' },
	{ 'a', '\342' },
	{ 'e', '\352' },
	{ 'i', '\356' },
	{ 'o', '\364' },
	{ 'u', '\373' },
};
struct diacritic diere_table[NR_DIACR] = {
	{ 'A', '\304' },
	{ 'E', '\313' },
	{ 'I', '\317' },
	{ 'O', '\326' },
	{ 'U', '\334' },
	{ 'a', '\344' },
	{ 'e', '\353' },
	{ 'i', '\357' },
	{ 'o', '\366' },
	{ 'u', '\374' },
};

static char *pad_chars = "0123456789+-*/\015,.";

static char *pad_seq[] = {
	"\033[2~",	/* INS */
	"\033[4~",	/* END */
	"\033[B" ,	/* DOWN */
	"\033[6~",	/* PGDN */
	"\033[D" ,	/* LEFT */
	"\033[G" ,	/* MID */
	"\033[C" ,	/* RIGHT */
	"\033[1~",	/* HOME */
	"\033[A" ,	/* UP */
	"\033[5~",	/* PGUP */
	"+",		/* PLUS */
	"-",		/* MINUS */
	"*",		/* ASTERISK */
	"/",		/* SLASH */
	"'\n'",		/* ENTER */
	",",		/* COMMA */
	"\033[3~",	/* DEL */
};

static char *fn_seq[] = {
	"\033[[A",	/* F1 */
	"\033[[B",	/* F2 */
	"\033[[C",	/* F3 */
	"\033[[D",	/* F4 */
	"\033[[E",	/* F5 */
	"\033[17~",	/* F6 */
	"\033[18~",	/* F7 */
	"\033[19~",	/* F8 */
	"\033[20~",	/* F9 */
	"\033[21~",	/* F10 */
	"\033[23~",	/* F11, SF1 */
	"\033[24~",	/* F12, SF2 */
	"\033[25~",	/* SF3 */
	"\033[26~",	/* SF4 */
	"\033[28~",	/* SF5 */
	"\033[29~",	/* SF6 */
	"\033[31~",	/* SF7 */
	"\033[32~",	/* SF8 */
	"\033[33~",	/* SF9 */
	"\033[34~",	/* SF10 */
};

static void keyboard_delay(void)
{
	int n;

	for(n = 0; n < 1000; n++) {
		NOP();
	}
}

/* wait controller input buffer to be clear */
static int is_ready_to_write(void)
{
	int n;

	for(n = 0; n < 500000; n++) {
		if(!(inport_b(KBC_STATUS) & KB_STR_INBUSY)) {
			return 1;
		}
	}
	return 0;
}

static void keyboard_write(const unsigned char port, const unsigned char byte)
{
	ack = 0;

	if(is_ready_to_write()) {
		outport_b(port, byte);
	}
}

/* wait controller output buffer to be full or for controller acknowledge */
static int is_ready_to_read(void)
{
	int n, value;

	for(n = 0; n < 500000; n++) {
		if(ack) {
			return 1;
		}
		if((value = inport_b(KBC_STATUS)) & KB_STR_OUTBUSY) {
			if(value & (KB_STR_COMMERR | KB_STR_PARERR)) {
				continue;
			}
			return 1;
		}
	}
	return 0;
}

static int keyboard_wait_ack(void)
{
	int n;

	if(is_ready_to_read()) {
		for(n = 0; n < 1000; n++) {
			if(inport_b(KB_DATA) == KB_ACK) {
				return 0;
			}
			keyboard_delay();
		}
	}
	return 1;
}

static void keyboard_identify(void)
{
	/* disable */
	keyboard_write(KB_DATA, KB_CMD_DISABLE);
	if(keyboard_wait_ack()) {
		printk("WARNING: %s(): ACK not received on disable command!\n", __FUNCTION__);
	}

	/* identify */
	keyboard_write(KB_DATA, KB_CMD_IDENTIFY);
	if(keyboard_wait_ack()) {
		printk("WARNING: %s(): ACK not received on identify command!\n", __FUNCTION__);
	}
	if(is_ready_to_read()) {
		kb_identify[0] = inport_b(KB_DATA);
	}
	if(is_ready_to_read()) {
		kb_identify[1] = inport_b(KB_DATA);
	}

	/* enable */
	keyboard_write(KB_DATA, KB_CMD_ENABLE);
	if(keyboard_wait_ack()) {
		printk("WARNING: %s(): ACK not received on enable command!\n", __FUNCTION__);
	}

	/* flush buffers */
	inport_b(KB_DATA);
}

static void keyboard_reset(void)
{
	int errno;
	unsigned char config;

	/* disable device(s) */
	keyboard_write(KBC_COMMAND, KBC_CMD_DISABLE_PS2_1);
	keyboard_write(KBC_COMMAND, KBC_CMD_DISABLE_PS2_2);

	/* flush buffers */
	inport_b(KB_DATA);

	/* get controller configuration */
	config = 0;
	keyboard_write(KBC_COMMAND, KBC_CMD_RECV_CONFIG);
	if(is_ready_to_read()) {
		config = inport_b(KB_DATA);
	}
	ps2_active_ports = config & 0x01 ? 1 : 0;
	ps2_active_ports += config & 0x02 ? 1 : 0;
	ps2_supp_ports = 1 + (config & 0x20 ? 1 : 0);

	/* set controller configuration (disabling IRQs) */
	/*
	keyboard_write(KBC_COMMAND, KBC_CMD_SEND_CONFIG);
	keyboard_write(KB_DATA, config & ~(0x01 | 0x02 | 0x40));
	*/

	/* PS/2 controller self-test */
	keyboard_write(KBC_COMMAND, KBC_CMD_SELF_TEST);
	if(is_ready_to_read()) {
		if((errno = inport_b(KB_DATA)) != 0x55) {
			printk("WARNING: %s(): keyboard returned 0x%x in self-test.\n", __FUNCTION__, errno);
		}
	}

	/*
	 * This sets again the controller configuration since the previous
	 * step may also reset the PS/2 controller to its power-on defaults.
	 */
	keyboard_write(KBC_COMMAND, KBC_CMD_SEND_CONFIG);
	keyboard_write(KB_DATA, config);

	/* first PS/2 interface test */
	keyboard_write(KBC_COMMAND, KBC_CMD_PS2_1_TEST);
	if(is_ready_to_read()) {
		if((errno = inport_b(KB_DATA)) != 0) {
			printk("WARNING: %s(): keyboard returned 0x%x in first PS/2 interface test.\n", __FUNCTION__, errno);
		}
	}

	if(ps2_supp_ports > 1) {
		/* second PS/2 interface test */
		keyboard_write(KBC_COMMAND, KBC_CMD_PS2_2_TEST);
		if(is_ready_to_read()) {
			if((errno = inport_b(KB_DATA)) != 0) {
				printk("WARNING: %s(): keyboard returned 0x%x in second PS/2 interface test.\n", __FUNCTION__, errno);
			}
		}
	}

	/* enable device(s) */
	keyboard_write(KBC_COMMAND, KBC_CMD_ENABLE_PS2_1);
	keyboard_write(KBC_COMMAND, KBC_CMD_ENABLE_PS2_2);

	/* reset device(s) */
	keyboard_write(KB_DATA, KB_CMD_RESET);
	if(keyboard_wait_ack()) {
		printk("WARNING: %s(): ACK not received on reset command!\n", __FUNCTION__);
	}
	if(is_ready_to_read()) {
		if((errno = inport_b(KB_DATA)) != KB_RESET_OK) {
			printk("WARNING: %s(): keyboard returned 0x%x in reset.\n", __FUNCTION__, errno);
		}
	}

	return;
}

static void putc(struct tty *tty, unsigned char ch)
{
	if(tty_queue_putchar(tty, &tty->read_q, ch) < 0) {
		if(tty->termios.c_iflag & IMAXBEL) {
			vconsole_beep();
		}
	}
}

static void puts(struct tty *tty, char *seq)
{
	char ch;

	while((ch = *(seq++))) {
		putc(tty, ch);
	}
}

void reboot(void)
{
	CLI();
	keyboard_write(PS2_SYSCTRL_A, 0x01);		/* Fast Hot Reset */
	keyboard_write(KBC_COMMAND, KBC_CMD_HOTRESET);	/* Hot Reset */
	HLT();
}

void set_leds(unsigned char led_status)
{
	keyboard_write(KB_DATA, KB_SETLED);
	keyboard_wait_ack();

	keyboard_write(KB_DATA, led_status);
	keyboard_wait_ack();
}

void irq_keyboard(int num, struct sigcontext *sc)
{
	__key_t key, type;
	unsigned char scode, mod;
	struct tty *tty;
	struct vconsole *vc;
	unsigned char c;
	int n;

	tty = get_tty(MKDEV(VCONSOLES_MAJOR, current_cons));
	vc = (struct vconsole *)tty->driver_data;

	scode = inport_b(KB_DATA);

	/* keyboard controller said 'acknowledge!' */
	if(scode == KB_ACK) {
		ack = 1;
		return;
	}

	keyboard_bh.flags |= BH_ACTIVE;

	/* if in pure raw mode just queue the scan code and return */
	if(tty->kbd.mode == K_RAW) {
		putc(tty, scode);
		return;
	}

	if(scode == EXTKEY) {
		extkey = 1;
		return;
	}
	
	if(extkey) {
		key = e0_keys[scode & 0x7F];
	} else {
		key = scode & 0x7F;
	}

	if(tty->kbd.mode == K_MEDIUMRAW) {
		putc(tty, key | (scode & 0x80));
		extkey = 0;
		return;
	}

	key = keymap[NR_MODIFIERS * (scode & 0x7F)];

	/* bit 7 enabled means a key has been released */
	if(scode & NR_SCODES) {
		switch(key) {
			case CTRL:
			case LCTRL:
			case RCTRL:
				ctrl = 0;
				break;
			case ALT:
				if(!extkey) {
					alt = 0;
					sysrq = 0;
				} else {
					altgr = 0;
				}
				break;
			case SHIFT:
			case LSHIFT:
			case RSHIFT:
				if(!extkey) {
					shift = 0;
				}
				break;
			case CAPS:
			case NUMS:
			case SCRL:
				leds = 0;
				break;
		}
		extkey = 0;
		return;
	}

	switch(key) {
		case CAPS:
			if(!leds) {
				vc->led_status ^= CAPSBIT;
				vc->capslock = !vc->capslock;
				do_setleds = 1;
			}
			leds = 1;
			return;
		case NUMS:
			if(!leds) {
				vc->led_status ^= NUMSBIT;
				vc->numlock = !vc->numlock;
				do_setleds = 1;
			}
			leds = 1;
			return;
		case SCRL:
			if(!leds) {
				if(vc->scrlock) {
					do_tty_start = 1;
				} else {
					do_tty_stop = 1;
				}
			}
			leds = 1;
			return;
		case CTRL:
		case LCTRL:
		case RCTRL:
			ctrl = 1;
			return;
		case ALT:
			if(!extkey) {
				alt = 1;
			} else {
				altgr = 1;
			}
			return;
		case SHIFT:
		case LSHIFT:
		case RSHIFT:
			shift = 1;
			extkey = 0;
			return;
	}

	if(ctrl && alt && key == DEL) {
		if(ctrl_alt_del) {
			reboot();
		} else {
			send_sig(&proc_table[INIT], SIGINT);
		}
		return;
	}

	keymap_line = &keymap[(scode & 0x7F) * NR_MODIFIERS];
	mod = 0;

	if(vc->capslock && (keymap_line[MOD_BASE] & LETTER_KEYS)) {
		mod = !vc->capslock ? shift : vc->capslock - shift;
	} else {
		if(shift && !extkey) {
			mod = 1;
		}
	}
	if(altgr) {
		mod = 2;
	}
	if(ctrl) {
		mod = 4;
	}
	if(alt) {
		mod = 8;
	}

	key = keymap_line[mod];

	if(key >= AF1 && key <= AF12) {
		do_switch_console = key - CONS_KEYS;
		return;
	}

	if(shift && (key == PGUP)) {
		do_buf_scroll = SCROLL_UP;
		return;
	}

	if(shift && (key == PGDN)) {
		do_buf_scroll = SCROLL_DOWN;
		return;
	}

	if(extkey && (scode == SLASH_NPAD)) {
		key = SLASH;
	}

	if(any_key_to_reboot) {
		reboot();
	}

	if(tty->count) {
		type = key & 0xFF00;
		c = key & 0xFF;

		if(sysrq) {
			/* treat 0-9 and a-z keys as normal */
			type &= ~META_KEYS;
		}

		switch(type) {
			case FN_KEYS:
				puts(tty, fn_seq[c]);
				break;

			case SPEC_KEYS:
				switch(key) {
					case CR:
						putc(tty, C('M'));
						break;
					case SYSRQ:
						sysrq = 1;
						break;
				}
				break;

			case PAD_KEYS:
				if(!vc->numlock) {
					puts(tty, pad_seq[c]);
				} else {
					putc(tty, pad_chars[c]);
				}
				break;

			case DEAD_KEYS:
				if(!deadkey) {
					switch(c) {
						case GRAVE ^ DEAD_KEYS:
							deadkey = 1;
							diacr = grave_table;
							break;
						case ACUTE ^ DEAD_KEYS:
							deadkey = 2;
							diacr = acute_table;
							break;
						case CIRCM ^ DEAD_KEYS:
							deadkey = 3;
							diacr = circm_table;
							break;
						case DIERE ^ DEAD_KEYS:
							deadkey = 5;
							diacr = diere_table;
							break;
					}
					return;
				}
				c = diacr_chars[c];
				deadkey = 0;
				putc(tty, c);

				break;

			case META_KEYS:
				putc(tty, '\033');
				putc(tty, c);
				break;

			case LETTER_KEYS:
				if(deadkey) {
					for(n = 0; n < NR_DIACR; n++) {
						if(diacr[n].letter == c) {
							c = diacr[n].code;
						}
					}
				}
				putc(tty, c);
				break;

			default:
				if(sysrq) {
					switch(c) {
						case 'l':
							sysrq_op = SYSRQ_STACK;
							break;
						case 't':
							sysrq_op = SYSRQ_TASKS;
							break;
						default:
							sysrq_op = SYSRQ_UNDEF;
							break;
					}
					if(sysrq_op) {
						do_sysrq(sysrq_op);
						sysrq_op = 0;
					}

					return;
				}
				if(deadkey && c == ' ') {
					c = diacr_chars[deadkey - 1];
				}
				putc(tty, c);
				break;
		}
	}
	deadkey = 0;
	return;
}

void irq_keyboard_bh(void)
{
	int n;
	struct tty *tty;
	struct vconsole *vc;

	tty = get_tty(MKDEV(VCONSOLES_MAJOR, current_cons));
	vc = (struct vconsole *)tty->driver_data;

	video.screen_on(vc);

	if(do_switch_console >= 0) {
		vconsole_select(do_switch_console);
		do_switch_console = -1;
	}

	if(do_buf_scroll) {
		video.buf_scroll(vc, do_buf_scroll);
		do_buf_scroll = 0;
	}

	if(do_setleds) {
		set_leds(vc->led_status);
		do_setleds = 0;
	}

	if(do_tty_start) {
		tty->start(tty);
		do_tty_start = do_tty_stop = 0;
	}

	if(do_tty_stop) {
		tty->stop(tty);
		do_tty_start = do_tty_stop = 0;
	}

	tty = &tty_table[0];
	for(n = 0; n < NR_VCONSOLES; n++, tty++) {
		if(!tty->read_q.count) {
			continue;
		}
		if(tty->kbd.mode == K_RAW || tty->kbd.mode == K_MEDIUMRAW) {
			wakeup(&tty_read);
			continue;
		}
		if(lock_area(AREA_TTY_READ)) {
			keyboard_bh.flags |= BH_ACTIVE;
			continue;
		}
		tty->input(tty);
		unlock_area(AREA_TTY_READ);
	}
}

void keyboard_init(void)
{
	struct tty *tty;
	struct vconsole *vc;

	tty = get_tty(MKDEV(VCONSOLES_MAJOR, current_cons));
	vc = (struct vconsole *)tty->driver_data;
	video.screen_on(vc);
	video.cursor_blink((unsigned int)vc);

	add_bh(&keyboard_bh);

	if(!register_irq(KEYBOARD_IRQ, &irq_config_keyboard)) {
		enable_irq(KEYBOARD_IRQ);
	}

	keyboard_reset();

	/* flush buffers */
	inport_b(KB_DATA);

	keyboard_identify();

	printk("keyboard  0x%04x-0x%04x     %d\ttype=%s PS/2 devices=%d/%d\n", 0x60, 0x64, KEYBOARD_IRQ, kb_identify[0] == 0xAB ? "MF2" : "unknown", ps2_active_ports, ps2_supp_ports);

	keyboard_write(KB_DATA, KB_RATE);
	keyboard_wait_ack();
	keyboard_write(KB_DATA, DELAY_250 | RATE_30);
	keyboard_wait_ack();
}
