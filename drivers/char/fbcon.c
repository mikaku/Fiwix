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

	offset = (y * video.fb_width * video.fb_char_height) + (x * video.fb_char_width);
	addr += offset * video.fb_pixelwidth;

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
	short int *vidmem, *screen;

	if(!vc->has_focus) {
		vidmem = (short int *)vc->vidmem;
		vidmem[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
		return;
	}

	color = 0xAAAAAA;
	draw_glyph(vc->vidmem, vc->x, vc->y, &font_data[ch * video.fb_char_height], color);
	vc->screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
}

void fbcon_insert_char(struct vconsole *vc)
{
	int n, soffset, color;
	short int tmp, slast_ch;
	unsigned char *last_ch;
	short int *screen;

	screen = (short int *)vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	color = 0xAAAAAA;	// FIXME: should be the background color
	n = vc->x;
	last_ch = &font_data[SPACE_CHAR * video.fb_char_height];
	slast_ch = BLANK_MEM;

	while(n < vc->columns) {
		tmp = vc->screen[soffset];
		if(vc->has_focus) {
			draw_glyph(vc->vidmem, n, vc->y, last_ch, color);
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
	unsigned char *ch;
	short int *screen;

	screen = (short int *)vc->screen;
	soffset = (vc->y * vc->columns) + vc->x;
	color = 0xAAAAAA;	// FIXME: should be the background color
	n = vc->x;

	while(n < vc->columns) {
		sch = vc->screen[soffset + 1];
		if(vc->has_focus) {
			if(sch & 0xFF) {
				ch = &font_data[(sch & 0xFF) * video.fb_char_height];
			} else {
				ch = &font_data[SPACE_CHAR * video.fb_char_height];
			}
			draw_glyph(vc->vidmem, n, vc->y, ch, color);
		}
		memset_w(screen + soffset, sch, 1);
		soffset++;
		n++;
	}
	memset_w(screen + soffset, BLANK_MEM, 1);
}

void fbcon_update_curpos(struct vconsole *vc)
{
	int color;

	if(!vc->has_focus) {
		return;
	}

	/* remove old cursor */
	color = 0;	// FIXME: should be the background color
	if(vc->x != vc->cursor_x || vc->y != vc->cursor_y) {
		draw_glyph(vc->vidmem, vc->cursor_x, vc->cursor_y, &cursor_shape[0], color);
	}

	color = 0xAAAAAA;
	draw_glyph(vc->vidmem, vc->x, vc->y, &cursor_shape[0], color);
	vc->cursor_x = vc->x;
	vc->cursor_y = vc->y;
}

void fbcon_show_cursor(int mode)
{
	switch(mode) {
		case ON:
			video.flags |= VPF_CURSOR_ON;
			break;
		case OFF:
			video.flags &= ~VPF_CURSOR_ON;
			break;
	}
}

void fbcon_get_curpos(struct vconsole *vc)
{
}	

void fbcon_write_screen(struct vconsole *vc, int from, int count, int color)
{
	int n, n2, lines, columns, x, y;
	short int *vidmem;
	unsigned char *ch;

	if(!vc->has_focus) {
		vidmem = (short int *)vc->vidmem;
		memset_w(vidmem + from, color, count);
		return;
	}

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
			draw_glyph(vc->vidmem, n2, y + n, ch, color);
		}
		x = 0;
		columns = vc->columns;
	}
	memset_w(vc->screen + from, color, count);
}

void fbcon_blank_screen(struct vconsole *vc)
{
	int n, count;
	unsigned char *addr;

	if(vc->blanked) {
		return;
	}

	addr = vc->vidmem;
	for(n = 0, count = 0; n < video.lines; n++) {
		count = video.fb_pitch * video.fb_char_height;
		memset_b(addr, 0, count);
		addr += count;
	}
	vc->blanked = 1;
	fbcon_show_cursor(OFF);
}

void fbcon_scroll_screen(struct vconsole *vc, int top, int mode)
{
	int n, offset, soffset, count, scount;
	short int *screen;

	screen = (short int *)vc->screen;

	if(!top) {
		top = vc->top;
	}
	switch(mode) {
		case SCROLL_UP:
			count = video.fb_pitch * video.fb_char_height;
			scount = vc->columns * (vc->bottom - top - 1);
			soffset = top * vc->columns;
			top = (top + 1) * vc->columns;
			memcpy_b(vc->vidmem, vc->vidmem + count, video.fb_size - count);
			memset_b(vc->vidmem + video.fb_size - count, 0, count);
			if(vc->cursor_y) {
				vc->cursor_y--;
			}
			if(vc->has_focus) {
				memcpy_w(screen + soffset, screen + top, scount);
				memset_w(screen + soffset + scount, BLANK_MEM, top);
			}
			break;
		case SCROLL_DOWN:
			/*
			count = vc->columns;
			for(n = vc->bottom - 1; n >= top; n--) {
				memcpy_b(addr + (vc->columns * (n + 1)), addr + (vc->columns * n), count);
			}
			memset_b(addr + (top * vc->columns), BLANK_MEM, count);
			*/
			break;
	}
	return;
}

void fbcon_restore_screen(struct vconsole *vc)
{
	int x, y, color;
	unsigned char *ch, c;

	color = 0xAAAAAA;

	if(!screen_is_off) {
		fbcon_blank_screen(vc);
	}
	for(y = 0; y < video.lines; y++) {
		for(x = 0; x < vc->columns; x++) {
			c = vc->screen[(y * vc->columns) + x] & 0xFF;
			if(!c || c == SPACE_CHAR) {
				continue;
			}
			ch = &font_data[c * video.fb_char_height];
			draw_glyph(vc->vidmem, x, y, ch, color);
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
}

void fbcon_buf_refresh(struct vconsole *vc)
{
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
//	video.buf_scroll = fbcon_buf_scroll;
}
