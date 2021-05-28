/*
 * fiwix/drivers/char/fbcon.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/fb.h>
#include <fiwix/fbcon.h>
#include <fiwix/font.h>
#include <fiwix/console.h>
#include <fiwix/tty.h>
#include <fiwix/timer.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

#define SPACE_CHAR	32

unsigned char *font_data;
struct video_parms video;
static unsigned char screen_is_off = 0;

static unsigned char cursor_shape[] = {
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0x00,   /* -------- */
        0xFF,   /* ######## */
        0xFF,   /* ######## */
};

/* RGB colors */
static int color_table[] = {
	0x000000,	/* black */
	0x0000AA,	/* blue */
	0x00AA00,	/* green */
	0x00AAAA,	/* cyan */
	0xAA0000,	/* red */
	0xAA00AA,	/* magenta */
	0xAA5000,	/* brown */
	0xAAAAAA,	/* gray */

	0x555555,	/* dark gray */
	0x5555FF,	/* light blue */
	0x55FF55,	/* light green */
	0x55FFFF,	/* light cyan */
	0xFF5555,	/* light red */
	0xFF55FF,	/* light magenta */
	0xFFFF55,	/* yellow */
	0xFFFFFF,	/* white */
};

static int get_fg_color(unsigned char color)
{
	int fg, bright;

	fg = color & 7;
	bright = (color & 0xF) & 8;
	return color_table[bright + fg];
}

static int get_bg_color(unsigned char color)
{
	int bg;

	bg = (color >> 4) & 7;
	return color_table[bg];
}

static void set_color(void *addr, int color)
{
	unsigned int *addr32;
	unsigned short int *addr16;
	unsigned char *addr8;
	short int r, g, b;

	switch(video.fb_bpp) {
		case 32:
			addr32 = (unsigned int *)addr;
			*addr32 = color;
			break;
		case 24:
			addr8 = (unsigned char *)addr;
			*(addr8++) = color & 0xFF;
			*(addr8++) = (color >> 8) & 0xFF;
			*(addr8++) = (color >> 16) & 0xFF;
			break;
		case 16:
			/* 0:5:6:5 */
			r = ((color >> 16) & 0xFF) << 8;
			g = ((color >> 8) & 0xFF) << 8;
			b = (color & 0xFF) << 8;
			addr16 = (unsigned short int *)addr;
			*addr16 = (r & 0xf800) | ((g & 0xfc00) >> 5) | ((b & 0xf800) >> 11);
		case 15:
			/* 1:5:5:5 */
			r = ((color >> 16) & 0xFF) << 8;
			g = ((color >> 8) & 0xFF) << 8;
			b = (color & 0xFF) << 8;
			addr16 = (unsigned short int *)addr;
			*addr16 = ((r & 0xf800) >> 1) | ((g & 0xf800) >> 6) | ((b & 0xf800) >> 11);
			break;
	}
}

static void draw_glyph(unsigned char *addr, int x, int y, unsigned char *ch, int color)
{
	int n, b, offset;

	if(screen_is_off) {
		return;
	}

	offset = (y * video.fb_linesize) + (x * video.fb_bpp);
	addr += offset;

	for(n = 0; n < video.fb_char_height; n++) {
		if(*(ch + n) == 0) {
			if(ch == &cursor_shape[0]) {
				addr += video.fb_pitch;
				continue;
			}
			b = video.fb_char_width - 1;
			do {
				set_color(addr, get_bg_color(color));
				addr += video.fb_pixelwidth;
				b--;
			} while(b >= 0);
		} else {
			b = video.fb_char_width - 1;
			do {
				if(*(ch + n) & (1 << b)) {
					set_color(addr, get_fg_color(color));
				} else {
					set_color(addr, get_bg_color(color));
				}
				addr += video.fb_pixelwidth;
				b--;
			} while(b >= 0);
		}
		addr += (video.fb_width - video.fb_char_width) * video.fb_pixelwidth;
	}
}

static void remove_cursor(struct vconsole *vc)
{
	int soffset;
	unsigned char *vidmem, *ch;
	short int *screen, sch;

	vidmem = vc->vidmem;
	screen = vc->screen;
	soffset = (vc->cursor_y * vc->columns) + vc->cursor_x;

	sch = screen[soffset];
	if(sch & 0xFF) {
		ch = &font_data[(sch & 0xFF) * video.fb_char_height];
	} else {
		ch = &font_data[SPACE_CHAR * video.fb_char_height];
	}
	draw_glyph(vidmem, vc->cursor_x, vc->cursor_y, ch, sch >> 8);
}

static void draw_cursor(struct vconsole *vc)
{
	unsigned char *vidmem;

	vidmem = vc->vidmem;
	draw_glyph(vidmem, vc->x, vc->y, &cursor_shape[0], DEF_MODE >> 8);
}

void fbcon_put_char(struct vconsole *vc, unsigned char ch)
{
	short int *screen;
	unsigned char *vidmem;

	screen = vc->screen;

	if(!vc->has_focus) {
		screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
		return;
	}

	vidmem = vc->vidmem;
	draw_glyph(vidmem, vc->x, vc->y, &font_data[ch * video.fb_char_height], vc->color_attr >> 8);
	screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
	vcbuf[(video.buf_y * vc->columns) + vc->x] = vc->color_attr | ch;
}

void fbcon_insert_char(struct vconsole *vc)
{
	int n, soffset;
	short int tmp, slast_ch;
	unsigned char *vidmem, *last_ch;
	short int *screen;

	vidmem = vc->vidmem;
	screen = vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	n = vc->x;
	last_ch = &font_data[SPACE_CHAR * video.fb_char_height];
	slast_ch = BLANK_MEM;

	while(n < vc->columns) {
		tmp = screen[soffset];
		if(vc->has_focus) {
			draw_glyph(vidmem, n, vc->y, last_ch, vc->color_attr >> 8);
			last_ch = &font_data[(tmp & 0xFF) * video.fb_char_height];
		}
		memset_w(screen + soffset, slast_ch, 1);
		slast_ch = tmp;
		soffset++;
		n++;
	}
}

void fbcon_delete_char(struct vconsole *vc)
{
	int n, soffset;
	short int sch;
	unsigned char *vidmem, *ch;
	short int *screen;

	vidmem = vc->vidmem;
	screen = vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	n = vc->x;

	while(n < vc->columns) {
		sch = screen[soffset + 1];
		if(vc->has_focus) {
			if(sch & 0xFF) {
				ch = &font_data[(sch & 0xFF) * video.fb_char_height];
			} else {
				ch = &font_data[SPACE_CHAR * video.fb_char_height];
			}
			draw_glyph(vidmem, n, vc->y, ch, vc->color_attr >> 8);
		}
		memset_w(screen + soffset, sch, 1);
		soffset++;
		n++;
	}
	memset_w(screen + soffset, BLANK_MEM, 1);
}

void fbcon_update_curpos(struct vconsole *vc)
{
	if(!vc->has_focus) {
		return;
	}

	/* remove old cursor */
	if(vc->x != vc->cursor_x || vc->y != vc->cursor_y) {
		remove_cursor(vc);
	}

	if(video.flags & VPF_CURSOR_ON) {
		draw_cursor(vc);
	}
	vc->cursor_x = vc->x;
	vc->cursor_y = vc->y;
}

void fbcon_show_cursor(struct vconsole *vc, int mode)
{
	switch(mode) {
		case COND:
			if(!(video.flags & VPF_CURSOR_ON)) {
				break;
			}
			/* fall through */
		case ON:
			video.flags |= VPF_CURSOR_ON;
			fbcon_update_curpos(vc);
			break;
		case OFF:
			video.flags &= ~VPF_CURSOR_ON;
			fbcon_update_curpos(vc);
			break;
	}
}

void fbcon_get_curpos(struct vconsole *vc)
{
	/* not used */
}	

void fbcon_write_screen(struct vconsole *vc, int from, int count, short int color)
{
	int n, n2, lines, columns, x, y;
	unsigned char *vidmem, *ch;
	short int *screen;

	screen = vc->screen;
	if(!vc->has_focus) {
		memset_w(screen + from, color, count);
		return;
	}

	vidmem = vc->vidmem;
	ch = &font_data[SPACE_CHAR * video.fb_char_height];
	x = from % vc->columns;
	y = from / vc->columns;
	lines = count / vc->columns;
	columns = x + count;
	if(!lines) {
		lines = 1;
	}
	if(!columns) {
		columns = vc->columns;
	}
	for(n = 0; n < lines; n++) {
		for(n2 = x; n2 < columns; n2++) {
			draw_glyph(vidmem, n2, y + n, ch, color >> 8);
		}
		x = 0;
		columns = vc->columns;
	}
	memset_w(screen + from, color, count);
}

void fbcon_blank_screen(struct vconsole *vc)
{
	unsigned char *vidmem;

	if(vc->blanked) {
		return;
	}

	vidmem = vc->vidmem;
	if(!(int)vidmem) {
		return;
	}

	memset_b(vidmem, 0, video.fb_size);
	vc->blanked = 1;
}

void fbcon_scroll_screen(struct vconsole *vc, int top, int mode)
{
	int soffset, poffset, count;
	int x, y;
	short int *screen, sch, pch;
	unsigned char *vidmem, *ch;

	vidmem = vc->vidmem;
	screen = vc->screen;

	if(!top) {
		top = vc->top;
	}
	switch(mode) {
		case SCROLL_UP:
			if(vc->has_focus) {
				for(y = top + 1; y < vc->lines; y++) {
					for(x = 0; x < vc->columns; x++) {
						soffset = (y * vc->columns) + x;
						poffset = ((y - 1) * vc->columns) + x;
						sch = screen[soffset];
						pch = screen[poffset];
						if(sch == pch) {
							continue;
						}
						if(sch & 0xFF) {
							ch = &font_data[(sch & 0xFF) * video.fb_char_height];
						} else {
							ch = &font_data[SPACE_CHAR * video.fb_char_height];
						}
						draw_glyph(vidmem, x, y - 1, ch, sch >> 8);
					}
				}
				if(!screen_is_off) {
					count = video.fb_pitch * video.fb_char_height;
					memset_l(vidmem + video.fb_vsize - count, 0, count / sizeof(unsigned int));
				}
			}
			count = vc->columns * (vc->lines - top - 1);
			soffset = top * vc->columns;
			top = (top + 1) * vc->columns;
			if(vc->cursor_y) {
				vc->cursor_y--;
			}
			memcpy_w(screen + soffset, screen + top, count);
			memset_w(screen + soffset + count, BLANK_MEM, top);
			break;
		case SCROLL_DOWN:
			for(y = vc->lines - 2; y >= top; y--) {
				for(x = 0; x < vc->columns; x++) {
					if(vc->has_focus) {
						soffset = (y * vc->columns) + x;
						poffset = ((y + 1) * vc->columns) + x;
						sch = screen[soffset];
						pch = screen[poffset];
						if(sch == pch) {
							continue;
						}
						if(sch & 0xFF) {
							ch = &font_data[(sch & 0xFF) * video.fb_char_height];
						} else {
							ch = &font_data[SPACE_CHAR * video.fb_char_height];
						}
						draw_glyph(vidmem, x, y + 1, ch, sch >> 8);
					}
				}
				memcpy_w(screen + (vc->columns * (y + 1)), screen + (vc->columns * y), vc->columns);
			}
			if(vc->has_focus && !screen_is_off) {
				count = video.fb_pitch * video.fb_char_height;
				memset_l(vidmem + (top * count), 0, count / sizeof(unsigned int));
			}
			memset_w(screen + (top * vc->columns), BLANK_MEM, vc->columns);
			break;
	}
	return;
}

void fbcon_restore_screen(struct vconsole *vc)
{
	int x, y;
	short int *screen, sch;
	unsigned char *vidmem, *ch, c;

	vidmem = vc->vidmem;
	screen = vc->screen;

	if(!screen_is_off && !video.buf_top) {
		memset_b(vidmem, 0, video.fb_size);
	}
	for(y = 0; y < video.lines; y++) {
		for(x = 0; x < vc->columns; x++) {
			sch = screen[(y * vc->columns) + x];
			c = sch & 0xFF;
			if(!c || (c == SPACE_CHAR && !(sch >> 8))) {
				continue;
			}
			ch = &font_data[c * video.fb_char_height];
			draw_glyph(vidmem, x, y, ch, sch >> 8);
		}
	}
	vc->blanked = 0;
}

void fbcon_screen_on(struct vconsole *vc)
{
	unsigned long int flags;
	struct callout_req creq;

	if(screen_is_off) {
		screen_is_off = 0;
		SAVE_FLAGS(flags); CLI();
		fbcon_restore_screen(vc);
		fbcon_update_curpos(vc);
		RESTORE_FLAGS(flags);
		vc->blanked = 0;
	}
	creq.fn = fbcon_screen_off;
	creq.arg = (unsigned int)vc;
	add_callout(&creq, BLANK_INTERVAL);
}

void fbcon_screen_off(unsigned int arg)
{
	struct vconsole *vc;
	unsigned long int flags;

	vc = (struct vconsole *)arg;
	screen_is_off = 1;
	SAVE_FLAGS(flags); CLI();
	fbcon_blank_screen(vc);
	RESTORE_FLAGS(flags);
}

void fbcon_buf_scroll(struct vconsole *vc, int mode)
{
	short int sch;
	int y, x, offset;
	unsigned char *vidmem, *ch;

	if(video.buf_y <= SCREEN_LINES) {
		return;
	}

	vidmem = vc->vidmem;

	if(mode == SCROLL_UP) {
		if(video.buf_top < 0) {
			return;
		}
		if(!video.buf_top) {
			video.buf_top = (video.buf_y - SCREEN_LINES + 1) * SCREEN_COLS;
		}
		video.buf_top -= (SCREEN_LINES / 2) * SCREEN_COLS;
		if(video.buf_top < 0) {
			video.buf_top = 0;
		}
		for(offset = 0, y = 0; y < video.lines; y++) {
			for(x = 0; x < vc->columns; x++, offset++) {
				sch = vcbuf[video.buf_top + offset];
				if(sch & 0xFF) {
					ch = &font_data[(sch & 0xFF) * video.fb_char_height];
				} else {
					ch = &font_data[SPACE_CHAR * video.fb_char_height];
				}
				draw_glyph(vidmem, x, y, ch, sch >> 8);
			}
		}
		if(!video.buf_top) {
			video.buf_top = -1;
		}
		fbcon_show_cursor(vc, OFF);
		return;
	}
	if(mode == SCROLL_DOWN) {
		if(!video.buf_top) {
			return;
		}
		if(video.buf_top == video.buf_y * SCREEN_COLS) {
			return;
		}
		if(video.buf_top < 0) {
			video.buf_top = 0;
		}
		video.buf_top += (SCREEN_LINES / 2) * SCREEN_COLS;
		if(video.buf_top >= (video.buf_y - SCREEN_LINES + 1) * SCREEN_COLS) {
			video.buf_top = (video.buf_y - SCREEN_LINES + 1) * SCREEN_COLS;
		}
		for(offset = 0, y = 0; y < video.lines; y++) {
			for(x = 0; x < vc->columns; x++, offset++) {
				sch = vcbuf[video.buf_top + offset];
				if(sch & 0xFF) {
					ch = &font_data[(sch & 0xFF) * video.fb_char_height];
				} else {
					ch = &font_data[SPACE_CHAR * video.fb_char_height];
				}
				draw_glyph(vidmem, x, y, ch, sch >> 8);
			}
		}
		if(video.buf_top >= (video.buf_y - SCREEN_LINES + 1) * SCREEN_COLS) {
			fbcon_show_cursor(vc, ON);
			fbcon_update_curpos(vc);
		}
		return;
	}
}

void fbcon_cursor_blink(unsigned int arg)
{
	struct vconsole *vc;
	struct callout_req creq;
	static int blink_on = 0;

	vc = (struct vconsole *)arg;
	if(!vc->has_focus) {
		return;
	}

	if(video.flags & VPF_CURSOR_ON && !screen_is_off) {
		if(blink_on) {
			draw_cursor(vc);
		} else {
			remove_cursor(vc);
		}
	}
	blink_on = !blink_on;
	creq.fn = fbcon_cursor_blink;
	creq.arg = arg;
	add_callout(&creq, 25);		/* 250ms */
}

void fbcon_init(void)
{
	struct fbcon_font_desc *font_desc;

	video.put_char = fbcon_put_char;
	video.insert_char = fbcon_insert_char;
	video.delete_char = fbcon_delete_char;
	video.update_curpos = fbcon_update_curpos;
	video.show_cursor = fbcon_show_cursor;
	video.get_curpos = fbcon_get_curpos;
	video.write_screen = fbcon_write_screen;
	video.blank_screen = fbcon_blank_screen;
	video.scroll_screen = fbcon_scroll_screen;
	video.restore_screen = fbcon_restore_screen;
	video.screen_on = fbcon_screen_on;
	video.buf_scroll = fbcon_buf_scroll;
	video.cursor_blink = fbcon_cursor_blink;

	if(!(font_desc = fbcon_find_font(video.fb_char_height))) {
		font_desc = fbcon_find_font(16);
	}
	font_data = font_desc->data;
}
