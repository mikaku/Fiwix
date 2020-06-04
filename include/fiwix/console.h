/*
 * fiwix/include/fiwix/console.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONSOLE_H
#define _FIWIX_CONSOLE_H

#include <fiwix/config.h>
#include <fiwix/termios.h>
#include <fiwix/vt.h>

#define NR_VCONSOLES		12	/* number of virtual consoles */

#define VCONSOLES_MAJOR		4	/* virtual consoles major number */
#define SYSCON_MAJOR		5	/* system console major number */

#define MONO_ADDR		0xB0000L
#define COLOR_ADDR		0xB8000L

#define MONO_6845_ADDR		0x3B4	/* i/o address (+1 for data register) */
#define COLOR_6845_ADDR		0x3D4	/* i/o address (+1 for data register) */

#define ATTR_CONTROLLER		0x3C0	/* attribute controller registrer */
#define ATTR_CONTROLLER_PAS	0x20	/* palette address source */
#define INPUT_STAT1		0x3DA	/* input status #1 register */
#define BLANK_INTERVAL	(600 * HZ)	/* 600 seconds (10 minutes) */

#define CRT_INDEX		0
#define CRT_DATA		1
#define CRT_CURSOR_STR		0xA
#define CRT_CURSOR_END		0xB
#define CRT_START_ADDR_HI	0xC
#define CRT_START_ADDR_LO	0xD
#define CRT_CURSOR_POS_HI	0xE
#define CRT_CURSOR_POS_LO	0xF

#define CURSOR_MASK		0x1F
#define CURSOR_DISABLE		0x20

#define COLOR_NORMAL		0
#define COLOR_BOLD		1
#define COLOR_BOLD_OFF		2
#define COLOR_UNDERLINE		4
#define COLOR_BLINK		5
#define COLOR_REVERSE		7

#define COLOR_BLACK		0x0000
#define COLOR_BLUE		0x0100
#define COLOR_GREEN		0x0200
#define COLOR_CYAN		0x0300
#define COLOR_RED		0x0400
#define COLOR_MAGENTA		0x0500
#define COLOR_BROWN		0x0600
#define COLOR_WHITE		0x0700
#define BG_BLACK		0x0000
#define BG_BLUE			0x1000
#define BG_GREEN		0x2000
#define BG_CYAN			0x3000
#define BG_RED			0x4000
#define BG_MAGENTA		0x5000
#define BG_BROWN		0x6000
#define BG_WHITE		0x7000

#define DEF_MODE		(COLOR_WHITE | BG_BLACK)
#define BLANK_MEM		(DEF_MODE | ' ')

#define SCROLL_UP		1
#define SCROLL_DOWN		2

#define VC_BUF_UP		1
#define VC_BUF_DOWN		2

#define BS			127	/* backspace */

extern short int current_cons;	/* current console (/dev/tty1 ... /dev/tty12) */

struct video_parms {
	short int *address;
	int port;
	char *type;
	int columns;
	int lines;
};

struct vconsole {
	int x;		/* current column */
	int y;		/* current line */
	int top, bottom, columns;
	short int check_x;
	unsigned char led_status;
	unsigned char scrlock, numlock, capslock;
	unsigned char esc, sbracket, semicolon, question;
	int parmv1, parmv2;
	unsigned short int color_attr;
	unsigned char bold, underline, blink, reverse;
	int insert_mode;
	short int *vidmem;
	short int has_focus;
	short int *scrbuf;
	int saved_x;
	int saved_y;
	struct vt_mode vt_mode;
	unsigned char vc_mode;
	unsigned char blanked;
	int switchto_tty;
	struct tty *tty;
};

void vconsole_reset(struct tty *);
void vconsole_write(struct tty *);
void vconsole_select(int);
void vconsole_select_final(int);
void vconsole_save(struct vconsole *);
void vconsole_restore(struct vconsole *);
void vconsole_buffer_scrl(int);
void blank_screen(struct vconsole *);
void unblank_screen(struct vconsole *);
void screen_on(void);
void screen_off(unsigned int);
void vconsole_start(struct tty *);
void vconsole_stop(struct tty *);
void vconsole_beep(void);
void vconsole_deltab(struct tty *);
void console_flush_log_buf(char *, unsigned int);
void get_video_parms(void);
void vconsole_init(void);

#endif /* _FIWIX_CONSOLE_H */
