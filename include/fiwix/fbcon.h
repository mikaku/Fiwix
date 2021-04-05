/*
 * fiwix/include/fiwix/fbcon.h
 *
 * Copyright 2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_FBCON_H
#define _FIWIX_FBCON_H

void fbcon_put_char(struct vconsole *, unsigned char);

void fbcon_put_char(struct vconsole *, unsigned char);
void fbcon_insert_char(struct vconsole *);
void fbcon_delete_char(struct vconsole *);
void fbcon_update_curpos(struct vconsole *);
void fbcon_show_cursor(struct vconsole *, int);
void fbcon_get_curpos(struct vconsole *);
void fbcon_write_screen(struct vconsole *, int, int, int);
void fbcon_blank_screen(struct vconsole *);
void fbcon_scroll_screen(struct vconsole *, int, int);
void fbcon_restore_screen(struct vconsole *);
void fbcon_screen_on(struct vconsole *);
void fbcon_screen_off(unsigned int);
void fbcon_init(void);

#endif /* _FIWIX_FBCON_H */
