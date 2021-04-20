/*
 * fiwix/drivers/char/vgacon.c
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/config.h>
#include <fiwix/const.h>
#include <fiwix/vgacon.h>
#include <fiwix/console.h>
#include <fiwix/timer.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

/* ISO/IEC 8859-1:1998 (aka latin1, IBM819, CP819), same as in Linux */
static const char *iso8859 =
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	" !\"#$%&'()*+,-./0123456789:;<=>?"
	"@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_"
	"`abcdefghijklmnopqrstuvwxyz{|}~\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0"
	"\377\255\233\234\376\235\174\025\376\376\246\256\252\055\376\376"
	"\370\361\375\376\376\346\024\371\376\376\247\257\254\253\376\250"
	"\376\376\376\376\216\217\222\200\376\220\376\376\376\376\376\376"
	"\376\245\376\376\376\376\231\376\350\376\376\376\232\376\376\341"
	"\205\240\203\376\204\206\221\207\212\202\210\211\215\241\214\213"
	"\376\244\225\242\223\376\224\366\355\227\243\226\201\376\376\230"
;

struct video_parms video;
static unsigned char screen_is_off = 0;

void vgacon_put_char(struct vconsole *vc, unsigned char ch)
{
	short int *vidmem, *screen;

	ch = iso8859[ch];
	screen = (short int *)vc->screen;

	if(!vc->has_focus) {
		screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
		return;
	}

	vidmem = (short int *)vc->vidmem;
	vidmem[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
	screen[(vc->y * vc->columns) + vc->x] = vc->color_attr | ch;
	vcbuf[(video.buf_y * vc->columns) + vc->x] = vc->color_attr | ch;
}

void vgacon_insert_char(struct vconsole *vc)
{
	int n, offset;
	short int tmp, last_char, *vidmem, *screen;

	vidmem = (short int *)vc->vidmem;
	screen = (short int *)vc->screen;
	offset = (vc->y * vc->columns) + vc->x;
	n = vc->x;
	last_char = BLANK_MEM;

	while(n++ < vc->columns) {
		if(vc->has_focus) {
			memcpy_w(&tmp, vidmem + offset, 1);
			memset_w(vidmem + offset, last_char, 1);
		}
		memcpy_w(&tmp, screen + offset, 1);
		memset_w(screen + offset, last_char, 1);
		last_char = tmp;
		offset++;
	}
}

void vgacon_delete_char(struct vconsole *vc)
{
	int offset, count;
	short int *vidmem, *screen;

	vidmem = (short int *)vc->vidmem;
	screen = (short int *)vc->screen;
	offset = (vc->y * vc->columns) + vc->x;
	count = vc->columns - vc->x;

	if(vc->has_focus) {
		memcpy_w(vidmem + offset, vidmem + offset + 1, count);
		memset_w(vidmem + offset + count, BLANK_MEM, 1);
	}
	memcpy_w(screen + offset, screen + offset + 1, count);
	memset_w(screen + offset + count, BLANK_MEM, 1);
}

void vgacon_update_curpos(struct vconsole *vc)
{
	unsigned short int curpos;

	if(vc->has_focus) {
		curpos = (vc->y * vc->columns) + vc->x;
		outport_b(video.port + CRT_INDEX, CRT_CURSOR_POS_HI);
		outport_b(video.port + CRT_DATA, (curpos >> 8) & 0xFF);
		outport_b(video.port + CRT_INDEX, CRT_CURSOR_POS_LO);
		outport_b(video.port + CRT_DATA, (curpos & 0xFF));
	}
}

void vgacon_show_cursor(struct vconsole *vc, int mode)
{
	int status;

	switch(mode) {
		case COND:
			if(!(video.flags & VPF_CURSOR_ON)) {
				break;
			}
			/* fall through */
		case ON:
			outport_b(video.port + CRT_INDEX, CRT_CURSOR_STR);
			status = inport_b(video.port + CRT_DATA);
			outport_b(video.port + CRT_DATA, status & CURSOR_MASK);
			video.flags |= VPF_CURSOR_ON;
			break;
		case OFF:
			outport_b(video.port + CRT_INDEX, CRT_CURSOR_STR);
			status = inport_b(video.port + CRT_DATA);
			outport_b(video.port + CRT_DATA, status | CURSOR_DISABLE);
			video.flags &= ~VPF_CURSOR_ON;
			break;
	}
}

void vgacon_get_curpos(struct vconsole *vc)
{
	unsigned short int curpos;

	outport_b(video.port + CRT_INDEX, CRT_CURSOR_POS_HI);
	curpos = inport_b(video.port + CRT_DATA) << 8;
	outport_b(video.port + CRT_INDEX, CRT_CURSOR_POS_LO);
	curpos |= inport_b(video.port + CRT_DATA);

	vc->x = curpos % vc->columns;
	vc->y = curpos / vc->columns;
}	

void vgacon_write_screen(struct vconsole *vc, int from, int count, int color)
{
	short int *vidmem, *screen;

	screen = (short int *)vc->screen;
	if(!vc->has_focus) {
		memset_w(screen + from, color, count);
		return;
	}

	vidmem = (short int *)vc->vidmem;
	memset_w(vidmem + from, color, count);
	memset_w(screen + from, color, count);
}

void vgacon_blank_screen(struct vconsole *vc)
{
	short int *vidmem;

	if(vc->blanked) {
		return;
	}

	if(vc->has_focus) {
		vidmem = (short int *)vc->vidmem;
		memset_w(vidmem, BLANK_MEM, SCREEN_SIZE);
	}
	vc->blanked = 1;
	vgacon_show_cursor(vc, OFF);
}

void vgacon_scroll_screen(struct vconsole *vc, int top, int mode)
{
	int n, offset, count;
	short int *vidmem, *screen;
	unsigned long int flags;

	SAVE_FLAGS(flags); CLI();
	vidmem = (short int *)vc->vidmem;
	screen = (short int *)vc->screen;

	if(!top) {
		top = vc->top;
	}
	switch(mode) {
		case SCROLL_UP:
			count = vc->columns * (vc->bottom - top - 1);
			offset = top * vc->columns;
			top = (top + 1) * vc->columns;
			if(vc->has_focus) {
				memcpy_w(vidmem + offset, screen + top, count);
				memset_w(vidmem + offset + count, BLANK_MEM, top);
			}
			memcpy_w(screen + offset, screen + top, count);
			memset_w(screen + offset + count, BLANK_MEM, top);
			break;
		case SCROLL_DOWN:
			count = vc->columns;
			for(n = vc->bottom - 1; n >= top; n--) {
				memcpy_w(screen + (vc->columns * (n + 1)), screen + (vc->columns * n), count);
				if(vc->has_focus) {
					memcpy_w(vidmem + (vc->columns * (n + 1)), screen + (vc->columns * n), count);
				}
			}
			memset_w(screen + (top * vc->columns), BLANK_MEM, count);
			if(vc->has_focus) {
				memset_w(vidmem + (top * vc->columns), BLANK_MEM, count);
			}
			break;
	}
	RESTORE_FLAGS(flags);
	return;
}

void vgacon_restore_screen(struct vconsole *vc)
{
	short int *vidmem;

	if(vc->has_focus) {
		vidmem = (short int *)vc->vidmem;
		memcpy_w(vidmem, vc->screen, SCREEN_SIZE);
	}
}

void vgacon_screen_on(struct vconsole *vc)
{
	unsigned long int flags;
	struct callout_req creq;

	if(screen_is_off) {
		SAVE_FLAGS(flags); CLI();
		inport_b(INPUT_STAT1);
		inport_b(0x3BA);
		outport_b(ATTR_CONTROLLER, ATTR_CONTROLLER_PAS);
		RESTORE_FLAGS(flags);
	}
	creq.fn = vgacon_screen_off;
	creq.arg = 0;
	add_callout(&creq, BLANK_INTERVAL);
}

void vgacon_screen_off(unsigned int arg)
{
	unsigned long int flags;

	screen_is_off = 1;
	SAVE_FLAGS(flags); CLI();
	inport_b(INPUT_STAT1);
	inport_b(0x3BA);
	outport_b(ATTR_CONTROLLER, 0);
	RESTORE_FLAGS(flags);
}

void vgacon_buf_scroll(struct vconsole *vc, int mode)
{
	short int *vidmem;

	if(video.buf_y <= SCREEN_LINES) {
		return;
	}

	vidmem = (short int *)vc->vidmem;
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
		memcpy_w(vidmem, vcbuf + video.buf_top, SCREEN_SIZE);
		if(!video.buf_top) {
			video.buf_top = -1;
		}
		vgacon_show_cursor(vc, OFF);
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
			vgacon_restore_screen(vc);
			video.buf_top = 0;
			vgacon_show_cursor(vc, ON);
			vgacon_update_curpos(vc);
			return;
		}
		memcpy_w(vidmem, vcbuf + video.buf_top, SCREEN_SIZE);
		return;
	}
}

void vgacon_init(void)
{
	short int *bios_data;

	/* get the VGA type from the BIOS equipment information */
	bios_data = (short int *)(KERNEL_BASE_ADDR + 0x410);
	if((*bios_data & 0x30) == 0x30) {
		/* monochrome = 0x30 */
		video.address = (void *)MONO_ADDR;
		video.port = MONO_6845_ADDR;
		strcpy((char *)video.type, "VGA monochrome 80x25");
	} else {
		/* color = 0x00 || 0x20 */
		video.address = (void *)COLOR_ADDR;
		video.port = COLOR_6845_ADDR;
		strcpy((char *)video.type, "VGA color 80x25");
	}
	video.memsize = 384 * 1024;
	video.flags = VPF_VGA;
	video.columns = 80;
	video.lines = 25;
	video.put_char = vgacon_put_char;
	video.insert_char = vgacon_insert_char;
	video.delete_char = vgacon_delete_char;
	video.update_curpos = vgacon_update_curpos;
	video.show_cursor = vgacon_show_cursor;
	video.get_curpos = vgacon_get_curpos;
	video.write_screen = vgacon_write_screen;
	video.blank_screen = vgacon_blank_screen;
	video.scroll_screen = vgacon_scroll_screen;
	video.restore_screen = vgacon_restore_screen;
	video.screen_on = vgacon_screen_on;
	video.buf_scroll = vgacon_buf_scroll;

	memcpy_w(vcbuf, video.address, SCREEN_SIZE * 2);
}
