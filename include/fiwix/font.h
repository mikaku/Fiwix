/*
 * fiwix/include/fiwix/font.h
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FONT_H
#define _FIWIX_FONT_H

struct fbcon_font_desc {
	char *name;
	int width;
	int height;
	void *data;
	void *cursorshape;
};

extern struct fbcon_font_desc font_vga_8x8;
extern struct fbcon_font_desc font_vga_8x10;
extern struct fbcon_font_desc font_vga_8x12;
extern struct fbcon_font_desc font_vga_8x14;
extern struct fbcon_font_desc font_vga_8x16;

struct fbcon_font_desc *fbcon_find_font(int);

#endif /* _FIWIX_FONT_H */
