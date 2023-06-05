/*
 * fiwix/drivers/video/fonts.c
 *
 * Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/string.h>
#include <fiwix/font.h>
#include "font-lat9-8x8.c"
#include "font-lat9-8x14.c"
#include "font-lat9-8x16.c"

#define NR_FONTS (sizeof(fbcon_fonts) / sizeof(*fbcon_fonts))

static struct fbcon_font_desc *fbcon_fonts[] = {
	&font_lat9_8x8,
	&font_lat9_8x14,
	&font_lat9_8x16
};

struct fbcon_font_desc *fbcon_find_font(int height)
{
	int n;

	for(n = 0; n < NR_FONTS; n++) {
		if(fbcon_fonts[n]->height == height) {
 			return fbcon_fonts[n];
		}
	}

	return NULL;
}
