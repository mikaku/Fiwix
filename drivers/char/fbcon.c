/*
 * fiwix/drivers/char/fbcon.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/fb.h>
#include <fiwix/console.h>
#include <fiwix/tty.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>
#include "font-lat0-sun16.c"

struct video_parms video;

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
	unsigned char *addr;

	color = 0xAAAAAA;
	addr = (unsigned char *)video.address;  // FIXME: vc->vidmem;
	draw_glyph(addr, vc->x, vc->y, &font_data[ch * video.fb_char_height], color);
}

void fbcon_insert_char(struct vconsole *vc)
{
	int n, offset, color;
	short int tmp, last_char;
	unsigned char *addr, *ch;

	addr = (unsigned char *)video.address;	// FIXME: vc->vidmem;
	offset = (vc->y * video.fb_width * video.fb_char_height) + (vc->x * video.fb_char_width);
	addr += offset * video.fb_pixelwidth;
	ch = &font_data[32 * video.fb_char_height];

	color = 0;	// FIXME: should be the background color
	/*
	n = vc->x;
	last_char = BLANK_MEM;
	while(n++ < vc->columns) {
		memcpy_w(&tmp, vc->vidmem + offset, 1);
		memset_w(vc->vidmem + offset, last_char, 1);
		last_char = tmp;
		offset++;
	}
	*/
}

void fbcon_delete_char(struct vconsole *vc)
{
	int offset, count, color;
	unsigned char *addr, *ch;

	addr = (unsigned char *)video.address;	// FIXME: vc->vidmem;
	offset = (vc->y * video.fb_width * video.fb_char_height) + (vc->x * video.fb_char_width);
	addr += offset * video.fb_pixelwidth;
	ch = &font_data[32 * video.fb_char_height];

	color = 0;	// FIXME: should be the background color
	count = (vc->columns - vc->x) * video.fb_char_height * video.fb_bpp;
	memcpy_b(addr, addr + video.fb_bpp, count);
	draw_glyph(addr, vc->columns - 1, vc->y, ch, color);
}

void fbcon_update_curpos(struct vconsole *vc)
{
	int color;
	unsigned char *addr;

	if(!vc->has_focus) {
		return;
	}

	/* remove old cursor */
	color = 0;	// FIXME: should be the background color
	addr = (unsigned char *)video.address;  // FIXME: vc->vidmem;
	if(vc->x != vc->cursor_x || vc->y != vc->cursor_y) {
		draw_glyph(addr, vc->cursor_x, vc->cursor_y, &cursor_shape[0], color);
	}

	color = 0xAAAAAA;
	draw_glyph(addr, vc->x, vc->y, &cursor_shape[0], color);
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
	short int *vidmem;

	if(vc->has_focus) {
		from = from * video.fb_char_width * video.fb_char_height;
		from = from * video.fb_pixelwidth;
		count = count * video.fb_char_width * video.fb_char_height;
		count = count * video.fb_pixelwidth;
		memset_b(vc->vidmem + from, color, count);
	} else {
		vidmem = vc->vidmem;
		memset_w(vidmem + from, color, count);
	}
}

void fbcon_scroll_screen(struct vconsole *vc, int top, int mode)
{
	int n, offset, count;
	unsigned char *addr;

	addr = (unsigned char *)video.address;

	if(!top) {
		top = vc->top;
	}
	switch(mode) {
		case SCROLL_UP:
		/*
			count = vc->columns * (vc->bottom - top - 1);
			offset = top * vc->columns;
			top = (top + 1) * vc->columns;
			memcpy_b(addr + offset, addr + top, count);
			memset_b(addr + offset + count, BLANK_MEM, top);
		*/
			count = video.fb_pitch * video.fb_char_height;
			memcpy_b(addr, addr + count, video.fb_size - count);
			memset_b(addr + video.fb_size - count, 0, count);
			if(vc->cursor_y) {
				vc->cursor_y--;
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

void fbcon_screen_on(void)
{
}

void fbcon_screen_off(unsigned int arg)
{
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
	video.scroll_screen = fbcon_scroll_screen;
	video.screen_on = fbcon_screen_on;
	video.buf_scroll_up = fbcon_buf_scroll_up;
	video.buf_refresh = fbcon_buf_refresh;
}
