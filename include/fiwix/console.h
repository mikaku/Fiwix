/*
 * fiwix/include/fiwix/console.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_CONSOLE_H
#define _FIWIX_CONSOLE_H

#include <fiwix/vt.h>

#define NR_VCONSOLES		12	/* number of virtual consoles */

#define VCONSOLES_MAJOR		4	/* virtual consoles major number */
#define SYSCON_MAJOR		5	/* system console major number */

/* Graphic Rendition Combination Modes */
#define SGR_DEFAULT		0	/* back to the default rendition */
#define SGR_BOLD		1	/* set bold */
#define SGR_BLINK		5	/* set slowly blinking */
#define SGR_REVERSE		7	/* set reverse video */
#define SGR_BOLD_OFF		21	/* unset bold */
#define SGR_BLINK_OFF		25	/* unset blinking */
#define SGR_REVERSE_OFF		27	/* unset reverse video */

#define SGR_BLACK_FG		30	/* set black foreground */
#define SGR_RED_FG		31	/* set red foreground */
#define SGR_GREEN_FG		32	/* set green foreground */
#define SGR_BROWN_FG		33	/* set brown foreground */
#define SGR_BLUE_FG		34	/* set blue foreground */
#define SGR_MAGENTA_FG		35	/* set magenta foreground */
#define SGR_CYAN_FG		36	/* set cyan foreground */
#define SGR_WHITE_FG		37	/* set white foreground */

#define SGR_DEFAULT_FG_U_ON	38	/* set def. fg color (underline on) */
#define SGR_DEFAULT_FG_U_OFF	39	/* set def. fg color (underline off) */

#define SGR_BLACK_BG		40	/* set black background */
#define SGR_RED_BG		41	/* set red background */
#define SGR_GREEN_BG		42	/* set green background */
#define SGR_BROWN_BG		43	/* set brown background */
#define SGR_BLUE_BG		44	/* set blue background */
#define SGR_MAGENTA_BG		45	/* set magenta background */
#define SGR_CYAN_BG		46	/* set cyan background */
#define SGR_WHITE_BG		47	/* set white background */

#define SGR_DEFAULT_BG		49	/* set default background color */

#define NPARMS			16

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

#define BS			127	/* backspace */

#define VPF_VGA			0x01	/* VGA text mode */
#define VPF_VESAFB		0x02	/* x86 frame buffer */
#define VPF_CURSOR_ON		0x04	/* draw cursor */

#define ON			1
#define OFF			0

extern short int current_cons;	/* current console (/dev/tty1 ... /dev/tty12) */

short int *vc_screen[NR_VCONSOLES + 1];

struct vconsole {
	int x;		/* current column */
	int y;		/* current line */
	int top, bottom, columns;
	short int check_x;
	unsigned char led_status;
	unsigned char scrlock, numlock, capslock;
	unsigned char esc, sbracket, semicolon, question;
	int parmv1, parmv2;
	int nparms, parms[NPARMS];
	unsigned short int color_attr;
	unsigned char bold, underline, blink, reverse;
	int insert_mode;
	short int *vidmem;
	short int has_focus;
	short int *screen;
	int saved_x, cursor_x;
	int saved_y, cursor_y;
	struct vt_mode vt_mode;
	unsigned char vc_mode;
	unsigned char blanked;
	int switchto_tty;
	struct tty *tty;
};

struct video_parms {
	int flags;
	unsigned int *address;
	int port;
	char *type;
	int columns;
	int lines;
	int buf_y;
	int buf_top;
	int fb_memsize;
	int fb_version;
	int fb_width;
	int fb_height;
	int fb_char_width;
	int fb_char_height;
	int fb_bpp;
	int fb_pixelwidth;
	int fb_pitch;
	int fb_size;

	/* formerly video driver operations */
	void (*put_char)(struct vconsole *, unsigned char);
	void (*insert_char)(struct vconsole *);
	void (*delete_char)(struct vconsole *);
	void (*update_curpos)(struct vconsole *);
	void (*show_cursor)(int);
	void (*screen_on)(void);
	void (*get_curpos)(struct vconsole *);
	void (*scroll_screen)(struct vconsole *, int, int);
	void (*buf_scroll_up)(void);
	void (*buf_refresh)(struct vconsole *);
	void (*buf_scroll)(struct vconsole *, int);
};
extern struct video_parms video;

void vconsole_reset(struct tty *);
void vconsole_write(struct tty *);
void vconsole_select(int);
void vconsole_select_final(int);
void vconsole_save(struct vconsole *);
void vconsole_restore(struct vconsole *);
void vconsole_buffer_scrl(int);
void blank_screen(struct vconsole *);
void unblank_screen(struct vconsole *);
void vconsole_start(struct tty *);
void vconsole_stop(struct tty *);
void vconsole_beep(void);
void vconsole_deltab(struct tty *);
void console_flush_log_buf(char *, unsigned int);
void console_init(void);

#endif /* _FIWIX_CONSOLE_H */
