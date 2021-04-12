/*
 * fiwix/drivers/char/fbcon.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/fb.h>
#include <fiwix/fbcon.h>
#include <fiwix/console.h>
#include <fiwix/tty.h>
#include <fiwix/timer.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include "font-lat0-sun16.c"

#define SPACE_CHAR	32

struct video_parms video;
static unsigned char screen_is_off = 0;
extern short int *fb_vcbuf;

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

static void set_color(void *addr, int color)
{
	unsigned int *addr32;
	unsigned char *addr8;

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
		case 15:
			/* FIXME: 15bpp=5:5:5, 16bpp=5:6:5 */
			addr8 = (unsigned char *)addr;
			*(addr8++) = color & 0xFF;
			*(addr8++) = (color >> 8) & 0xFF;
			break;
	}
}

static void draw_glyph(unsigned char *addr, int x, int y, unsigned char *ch, int color)
{
	int n, b, offset;

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
				set_color(addr, 0);
				addr += video.fb_pixelwidth;
				b--;
			} while(b >= 0);
		} else {
			b = video.fb_char_width - 1;
			do {
				if(*(ch + n) & (1 << b)) {
					set_color(addr, color);
				} else {
					set_color(addr, 0);
				}
				addr += video.fb_pixelwidth;
				b--;
			} while(b >= 0);
		}
		addr += (video.fb_width - video.fb_char_width) * video.fb_pixelwidth;
	}
}

void fbcon_put_char(struct vconsole *vc, unsigned char ch)
{
	int color;
	short int *screen;
	unsigned char *vidmem;

	screen = (short int *)vc->screen;

	if(!vc->has_focus) {
		screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
		return;
	}

	if(!screen_is_off) {
		color = 0xAAAAAA;
		vidmem = vc->vidmem;
		draw_glyph(vidmem, vc->x, vc->y, &font_data[ch * video.fb_char_height], color);
	}
	screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
	fb_vcbuf[(video.buf_y * vc->columns) + vc->x] = vc->color_attr | ch;
}

void fbcon_insert_char(struct vconsole *vc)
{
	int n, soffset, color;
	short int tmp, slast_ch;
	unsigned char *vidmem, *last_ch;
	short int *screen;

	vidmem = vc->vidmem;
	screen = (short int *)vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	color = 0xAAAAAA;	// FIXME: should be the background color
	n = vc->x;
	last_ch = &font_data[SPACE_CHAR * video.fb_char_height];
	slast_ch = BLANK_MEM;

	while(n < vc->columns) {
		tmp = screen[soffset];
		if(vc->has_focus) {
			draw_glyph(vidmem, n, vc->y, last_ch, color);
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
	int n, soffset, color;
	short int sch;
	unsigned char *vidmem, *ch;
	short int *screen;

	vidmem = vc->vidmem;
	screen = (short int *)vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	color = 0xAAAAAA;	// FIXME: should be the background color
	n = vc->x;

	while(n < vc->columns) {
		sch = screen[soffset + 1];
		if(vc->has_focus) {
			if(sch & 0xFF) {
				ch = &font_data[(sch & 0xFF) * video.fb_char_height];
			} else {
				ch = &font_data[SPACE_CHAR * video.fb_char_height];
			}
			draw_glyph(vidmem, n, vc->y, ch, color);
		}
		memset_w(screen + soffset, sch, 1);
		soffset++;
		n++;
	}
	memset_w(screen + soffset, BLANK_MEM, 1);
}

void fbcon_update_curpos(struct vconsole *vc)
{
	int soffset, color;
	short int sch;
	unsigned char *vidmem, *ch;
	short int *screen;

	if(!vc->has_focus) {
		return;
	}

	vidmem = vc->vidmem;
	screen = (short int *)vc->screen;
	soffset = (vc->cursor_y * vc->columns) + vc->cursor_x;
	color = 0xAAAAAA;

	/* remove old cursor */
	if(vc->x != vc->cursor_x || vc->y != vc->cursor_y) {
		sch = screen[soffset];
		if(sch & 0xFF) {
			ch = &font_data[(sch & 0xFF) * video.fb_char_height];
		} else {
			ch = &font_data[SPACE_CHAR * video.fb_char_height];
		}
		draw_glyph(vidmem, vc->cursor_x, vc->cursor_y, ch, color);
	}

	if(video.flags & VPF_CURSOR_ON) {
		draw_glyph(vidmem, vc->x, vc->y, &cursor_shape[0], color);
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
			break;
	}
}

void fbcon_get_curpos(struct vconsole *vc)
{
	/* not used */
}	

void fbcon_write_screen(struct vconsole *vc, int from, int count, int color)
{
	int n, n2, lines, columns, x, y;
	unsigned char *vidmem, *ch;
	short int *screen;

	screen = (short int *)vc->screen;
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
			draw_glyph(vidmem, n2, y + n, ch, color);
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
	int x, y, sch, pch, color;
	short int *screen;
	unsigned char *vidmem, *ch;
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	vidmem = vc->vidmem;
	screen = (short int *)vc->screen;
	color = 0xAAAAAA;	// FIXME: should be the background color

	if(!top) {
		top = vc->top;
	}
	switch(mode) {
		case SCROLL_UP:
			if(vc->has_focus) {
				for(y = top + 1; y < vc->bottom; y++) {
					for(x = 0; x < vc->columns; x++) {
						soffset = (y * vc->columns) + x;
						poffset = ((y - 1) * vc->columns) + x;
						sch = screen[soffset] & 0xFF;
						pch = screen[poffset] & 0xFF;
						if(sch == pch) {
							continue;
						}
						if(sch) {
							ch = &font_data[sch * video.fb_char_height];
						} else {
							ch = &font_data[SPACE_CHAR * video.fb_char_height];
						}
						draw_glyph(vidmem, x, y - 1, ch, color);
					}
				}
				count = video.fb_pitch * video.fb_char_height;
				memset_l(vidmem + video.fb_vsize - count, 0, count / sizeof(unsigned int));
			}
			count = vc->columns * (vc->bottom - top - 1);
			soffset = top * vc->columns;
			top = (top + 1) * vc->columns;
			if(vc->cursor_y) {
				vc->cursor_y--;
			}
			memcpy_w(screen + soffset, screen + top, count);
			memset_w(screen + soffset + count, BLANK_MEM, top);
			break;
		case SCROLL_DOWN:
			for(y = vc->bottom - 2; y >= top; y--) {
				for(x = 0; x < vc->columns; x++) {
					if(vc->has_focus) {
						soffset = (y * vc->columns) + x;
						poffset = ((y + 1) * vc->columns) + x;
						sch = screen[soffset] & 0xFF;
						pch = screen[poffset] & 0xFF;
						if(sch == pch) {
							continue;
						}
						if(sch) {
							ch = &font_data[sch * video.fb_char_height];
						} else {
							ch = &font_data[SPACE_CHAR * video.fb_char_height];
						}
						draw_glyph(vidmem, x, y + 1, ch, color);
					}
				}
				memcpy_w(screen + (vc->columns * (y + 1)), screen + (vc->columns * y), vc->columns);
			}
			if(vc->has_focus) {
				count = video.fb_pitch * video.fb_char_height;
				memset_l(vidmem + (top * count), 0, count / sizeof(unsigned int));
			}
			memset_w(screen + (top * vc->columns), BLANK_MEM, vc->columns);
			break;
	}
	RESTORE_FLAGS(flags);
	return;
}

void fbcon_restore_screen(struct vconsole *vc)
{
	int x, y, color;
	short int *screen;
	unsigned char *vidmem, *ch, c;

	vidmem = vc->vidmem;
	screen = (short int *)vc->screen;
	color = 0xAAAAAA;

	if(!screen_is_off) {
		memset_b(vidmem, 0, video.fb_size);
	}
	for(y = 0; y < video.lines; y++) {
		for(x = 0; x < vc->columns; x++) {
			c = screen[(y * vc->columns) + x] & 0xFF;
			if(!c || c == SPACE_CHAR) {
				continue;
			}
			ch = &font_data[c * video.fb_char_height];
			draw_glyph(vidmem, x, y, ch, color);
		}
	}
	vc->blanked = 0;
}

void fbcon_screen_on(struct vconsole *vc)
{
	unsigned long int flags;
	struct callout_req creq;

	if(screen_is_off) {
		SAVE_FLAGS(flags); CLI();
		fbcon_restore_screen(vc);
		fbcon_update_curpos(vc);
		RESTORE_FLAGS(flags);
		vc->blanked = 0;
		screen_is_off = 0;
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

void fbcon_buf_scroll_up(void)
{
	memcpy_w(fb_vcbuf, fb_vcbuf + video.columns, (VC_BUF_SIZE - video.columns) * 2);
}

void fbcon_buf_refresh(struct vconsole *vc)
{
	short int *screen;

	screen = (short int *)vc->screen;
	memset_w(fb_vcbuf, BLANK_MEM, VC_BUF_SIZE);
	memcpy_w(fb_vcbuf, screen, SCREEN_SIZE);
}

void fbcon_buf_scroll(struct vconsole *vc, int mode)
{
	short int sch;
	int y, x, offset, color;
	unsigned char *vidmem, *ch;

	if(video.buf_y <= SCREEN_LINES) {
		return;
	}

	vidmem = vc->vidmem;
	color = 0xAAAAAA;

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
				sch = fb_vcbuf[video.buf_top + offset] & 0xFF;
				if(sch) {
					ch = &font_data[sch * video.fb_char_height];
				} else {
					ch = &font_data[SPACE_CHAR * video.fb_char_height];
				}
				draw_glyph(vidmem, x, y, ch, color);
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
				sch = fb_vcbuf[video.buf_top + offset] & 0xFF;
				if(sch) {
					ch = &font_data[sch * video.fb_char_height];
				} else {
					ch = &font_data[SPACE_CHAR * video.fb_char_height];
				}
				draw_glyph(vidmem, x, y, ch, color);
			}
		}
		if(video.buf_top >= (video.buf_y - SCREEN_LINES + 1) * SCREEN_COLS) {
			fbcon_show_cursor(vc, ON);
			fbcon_update_curpos(vc);
		}
		return;
	}
}

void fbcon_init(void)
{
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
	video.buf_scroll_up = fbcon_buf_scroll_up;
	video.buf_refresh = fbcon_buf_refresh;
	video.buf_scroll = fbcon_buf_scroll;
}
