/*
 * fiwix/drivers/video/video.c
 *
 * Copyright 2021-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/asm.h>
#include <fiwix/config.h>
#include <fiwix/vgacon.h>
#include <fiwix/fb.h>
#include <fiwix/fbcon.h>
#include <fiwix/console.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

struct video_parms video;

void video_init(void)
{
	memset_b(vcbuf, 0, (video.columns * video.lines * SCREENS_LOG * 2 * sizeof(short int)));

	if(video.flags & VPF_VGA) {
		vgacon_init();
	}
	if(video.flags & VPF_VESAFB) {
		fb_init();
		fbcon_init();
	}
}
