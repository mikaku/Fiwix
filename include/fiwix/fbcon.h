/*
 * fiwix/include/fiwix/fbcon.h
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FBCON_H
#define _FIWIX_FBCON_H

void fbcon_put_char(struct vconsole *, unsigned char);
void fbcon_delete_char(struct vconsole *);
void fbcon_update_curpos(struct vconsole *);
void fbcon_show_cursor(int);
void fbcon_get_curpos(struct vconsole *);
void fbcon_screen_on(void);
void fbcon_screen_off(unsigned int);
void fbcon_init(void);

#endif /* _FIWIX_FBCON_H */
