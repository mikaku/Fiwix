/*
 * fiwix/drivers/char/keyboard.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/kernel.h>
#include <fiwix/limits.h>
#include <fiwix/ps2.h>
#include <fiwix/keyboard.h>
#include <fiwix/reboot.h>
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
static unsigned char altsysrq = 0;
static int sysrq_op = 0;
static unsigned char kb_identify[2] = {0, 0};
static unsigned char is_ps2 = 0;
static unsigned char orig_scan_set = 0;
volatile unsigned char ack = 0;

static char do_switch_console = -1;
static unsigned char do_buf_scroll = 0;
static unsigned char do_setleds = 0;
static unsigned char do_tty_stop = 0;
static unsigned char do_tty_start = 0;
static unsigned char do_sysrq = 0;

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

static void keyboard_identify(void)
{
	char config;

	/* disable */
	ps2_write(PS2_DATA, PS2_KB_DISABLE);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on disable command!\n", __FUNCTION__);
	} else {
		is_ps2++;
	}

	/* identify */
	ps2_write(PS2_DATA, PS2_DEV_IDENTIFY);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on identify command!\n", __FUNCTION__);
	} else {
		is_ps2++;
	}
	kb_identify[0] = ps2_read(PS2_DATA);
	kb_identify[1] = ps2_read(PS2_DATA);

	/* get scan code */
	ps2_write(PS2_COMMAND, PS2_CMD_RECV_CONFIG);
	config = ps2_read(PS2_DATA);	/* save state */
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config & ~0x40);	/* unset translation */
	ps2_write(PS2_DATA, PS2_KB_GETSETSCAN);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on get scan code command!\n", __FUNCTION__);
	}
	ps2_write(PS2_DATA, 0);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on get scan code command!\n", __FUNCTION__);
	}
	orig_scan_set = ps2_read(PS2_DATA);
	if(orig_scan_set != 2) {
		ps2_write(PS2_DATA, PS2_KB_GETSETSCAN);
		ps2_write(PS2_DATA, 2);
		if(ps2_wait_ack()) {
			printk("WARNING: %s(): ACK not received on set scan code command!\n", __FUNCTION__);
		}
	}
	ps2_write(PS2_COMMAND, PS2_CMD_SEND_CONFIG);
	ps2_write(PS2_DATA, config);	/* restore state */

	/* enable */
	ps2_write(PS2_DATA, PS2_DEV_ENABLE);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on enable command!\n", __FUNCTION__);
	}
	ps2_clear_buffer();
}

static void putc(struct tty *tty, unsigned char ch)
{
	if(tty->count) {
		if(charq_putchar(&tty->read_q, ch) < 0) {
			if(tty->termios.c_iflag & IMAXBEL) {
				vconsole_beep();
			}
		}
	}
}

static void puts(struct tty *tty, char *seq)
{
	char ch;

	if(tty->count) {
		while((ch = *(seq++))) {
			putc(tty, ch);
		}
	}
}

void set_leds(unsigned char led_status)
{
	ps2_write(PS2_DATA, PS2_KB_SETLED);
	ps2_wait_ack();

	ps2_write(PS2_DATA, led_status);
	ps2_wait_ack();
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

	scode = inport_b(PS2_DATA);

	/* keyboard controller said 'acknowledge!' */
	if(scode == DEV_ACK) {
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
					altsysrq = 0;
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


	type = key & 0xFF00;
	c = key & 0xFF;

	if(altsysrq) {
		/* treat 0-9 and a-z keys as normal */
		type &= ~META_KEYS;
	}

	switch(type) {
		case FN_KEYS:
			if(c > sizeof(fn_seq) / sizeof(char *)) {
				printk("WARNING: %s(): unrecognized function key.\n", __FUNCTION__);
				break;
			}
			puts(tty, fn_seq[c]);
			break;

		case SPEC_KEYS:
			switch(key) {
				case CR:
					putc(tty, C('M'));
					break;
				case SYSRQ:
					altsysrq = 1;
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
			if(altsysrq) {
				switch(c) {
					case 'l':
						sysrq_op = SYSRQ_STACK;
						do_sysrq = 1;
						break;
					case 'm':
						sysrq_op = SYSRQ_MEMORY;
						do_sysrq = 1;
						break;
					case 't':
						sysrq_op = SYSRQ_TASKS;
						do_sysrq = 1;
						break;
					default:
						sysrq_op = SYSRQ_UNDEF;
						do_sysrq = 1;
						break;
				}
				break;
			}
			if(deadkey && c == ' ') {
				c = diacr_chars[deadkey - 1];
			}
			putc(tty, c);
			break;
	}

	deadkey = 0;
}

void irq_keyboard_bh(struct sigcontext *sc)
{
	struct tty *tty;
	struct vconsole *vc;
	char value;

	tty = get_tty(MKDEV(VCONSOLES_MAJOR, current_cons));
	vc = (struct vconsole *)tty->driver_data;

	video.screen_on(vc);

	if(do_switch_console >= 0) {
		value = do_switch_console;
		do_switch_console = -1;
		vconsole_select(value);
	}

	if(do_buf_scroll) {
		value = do_buf_scroll;
		do_buf_scroll = 0;
		video.buf_scroll(vc, value);
	}

	if(do_setleds) {
		do_setleds = 0;
		set_leds(vc->led_status);
	}

	if(do_tty_start) {
		do_tty_start = do_tty_stop = 0;
		tty->start(tty);
	}

	if(do_tty_stop) {
		do_tty_start = do_tty_stop = 0;
		tty->stop(tty);
	}

	if(do_sysrq) {
		do_sysrq = 0;
		sysrq(sysrq_op);
	}

	for(tty = tty_table; tty; tty = tty->next) {
		if(MAJOR(tty->dev) == VCONSOLES_MAJOR && MINOR(tty->dev) < NR_VCONSOLES) {
			if(!tty->read_q.count) {
				continue;
			}
			if(tty->kbd.mode == K_RAW || tty->kbd.mode == K_MEDIUMRAW) {
				wakeup(&tty->read_q);
				continue;
			}
			if(!can_lock_area(AREA_TTY_READ)) {
				keyboard_bh.flags |= BH_ACTIVE;
				continue;
			}
			tty->input(tty);
			unlock_area(AREA_TTY_READ);
		}
	}
}

void keyboard_init(void)
{
	struct tty *tty;
	struct vconsole *vc;
	int errno;

	tty = get_tty(MKDEV(VCONSOLES_MAJOR, current_cons));
	vc = (struct vconsole *)tty->driver_data;
	video.screen_on(vc);
	video.cursor_blink((unsigned int)vc);

	add_bh(&keyboard_bh);
	if(!register_irq(KEYBOARD_IRQ, &irq_config_keyboard)) {
		enable_irq(KEYBOARD_IRQ);
	}

	/* reset device */
	ps2_write(PS2_DATA, PS2_DEV_RESET);
	if(ps2_wait_ack()) {
		printk("WARNING: %s(): ACK not received on reset command!\n", __FUNCTION__);
	}
	if((errno = ps2_read(PS2_DATA)) != DEV_RESET_OK) {
		/* some keyboards return an ID byte before 0xAA */
		if((errno = ps2_read(PS2_DATA)) != DEV_RESET_OK) {
			printk("WARNING: %s(): keyboard returned 0x%x on reset (1).\n", __FUNCTION__, errno);
		}
	}

	ps2_clear_buffer();
	keyboard_identify();
	printk("keyboard  0x%04x-0x%04x     %d", 0x60, 0x64, KEYBOARD_IRQ);
	printk("\ttype=%s %s", kb_identify[0] == 0xAB ? "MF2" : "unknown", is_ps2 ? "PS/2" : "");
	printk(" %s", (kb_identify[1] == 0x41 || kb_identify[1] == 0xC1) ? "translated" : "");
	printk(" scan set 2");
	if(orig_scan_set != 2) {
		printk(" (was %d)", orig_scan_set);
	}
	printk("\n");

	ps2_write(PS2_DATA, PS2_DEV_RATE);
	ps2_wait_ack();
	ps2_write(PS2_DATA, DELAY_250 | RATE_30);
	ps2_wait_ack();
}
