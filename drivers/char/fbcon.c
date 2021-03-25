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
	short int *vidmem;

	if(!vc->has_focus) {
		vidmem = (short int *)vc->vidmem;
		vidmem[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
		return;
	}

	color = 0xAAAAAA;
	draw_glyph(vc->vidmem, vc->x, vc->y, &font_data[ch * video.fb_char_height], color);
}

void fbcon_insert_char(struct vconsole *vc)
{
	int n, offset, color;
	short int tmp, last_char;
	unsigned char *addr, *ch;

	addr = vc->vidmem;
	offset = (vc->y * video.fb_width * video.fb_char_height) + (vc->x * video.fb_char_width);
	addr += offset * video.fb_pixelwidth;
	ch = &font_data[SPACE_CHAR * video.fb_char_height];

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

	addr = vc->vidmem;
	offset = (vc->y * video.fb_width * video.fb_char_height) + (vc->x * video.fb_char_width);
	addr += offset * video.fb_pixelwidth;
	ch = &font_data[SPACE_CHAR * video.fb_char_height];

	color = 0;	// FIXME: should be the background color
	count = (vc->columns - vc->x) * video.fb_char_height * video.fb_bpp;
	memcpy_b(addr, addr + video.fb_bpp, count);
	draw_glyph(addr, vc->columns - 1, vc->y, ch, color);
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
}

void fbcon_scroll_screen(struct vconsole *vc, int top, int mode)
{
	int n, offset, count;

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
			memcpy_b(vc->vidmem, vc->vidmem + count, video.fb_size - count);
			memset_b(vc->vidmem + video.fb_size - count, 0, count);
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
	unsigned long int flags;
	struct callout_req creq;

	if(screen_is_off) {
		SAVE_FLAGS(flags); CLI();
		//restore_screen();
		printk("[*ON*]");
		RESTORE_FLAGS(flags);
	}
	creq.fn = fbcon_screen_off;
	creq.arg = 0;
	add_callout(&creq, BLANK_INTERVAL);
}

void fbcon_screen_off(unsigned int arg)
{
	unsigned long int flags;

	screen_is_off = 1;
	SAVE_FLAGS(flags); CLI();
	//blank_screen();
	printk("[*OFF*]");
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
	video.scroll_screen = fbcon_scroll_screen;
	video.screen_on = fbcon_screen_on;
	video.buf_scroll_up = fbcon_buf_scroll_up;
	video.buf_refresh = fbcon_buf_refresh;
}
