/*
 * fiwix/drivers/char/vt.c
 *
 * Copyright 2018-2021, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/kernel.h>
#include <fiwix/console.h>
#include <fiwix/keyboard.h>
#include <fiwix/tty.h>
#include <fiwix/vt.h>
#include <fiwix/kd.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int kbdmode = 0;

int vt_ioctl(struct tty *tty, int cmd, unsigned int arg)
{
	struct vconsole *vc;
	int n, errno;

	/* only virtual consoles support the following ioctl commands */
	if(MAJOR(tty->dev) != VCONSOLES_MAJOR) {
		return -ENXIO;
	}

	vc = (struct vconsole *)tty->driver_data;

	switch(cmd) {
		case KDGETLED:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned char)))) {
				return errno;
			}
			memset_b((void *)arg, vc->led_status, sizeof(char));
			break;

		case KDSETLED:
			if(arg > 7) {
				return -EINVAL;
			}
			vc->led_status = arg;
			set_leds(vc->led_status);
			break;

		/* FIXME: implement KDGKBLED and KDSKBLED
		 * it will need to convert 'scrlock, numlock, capslock' into led_flags.
		 */

		case KDGKBTYPE:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned char)))) {
				return errno;
			}
			memset_b((void *)arg, KB_101, sizeof(char));
			break;

		case KDSETMODE:
			if(arg != KD_TEXT && arg != KD_GRAPHICS) {
				return -EINVAL;
			}
			if(vc->vc_mode != arg) {
				vc->vc_mode = arg;
				if(arg == KD_GRAPHICS) {
					video.blank_screen(vc);
				} else {
					unblank_screen(vc);
				}
			}
			break;

		case KDGETMODE:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned char)))) {
				return errno;
			}
			memset_b((void *)arg, vc->vc_mode, sizeof(char));
			break;

		case KDGKBMODE:
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned char)))) {
				return errno;
			}
			memset_b((void *)arg, tty->kbd.mode, sizeof(unsigned char));
			break;

		case KDSKBMODE:
			if(arg != K_RAW && arg != K_XLATE && arg != K_MEDIUMRAW) {
				arg = K_XLATE;
			}
			tty->kbd.mode = arg;
			tty_queue_flush(&tty->read_q);
			break;

		case KDSKBENT:
		{
			struct kbentry *k = (struct kbentry *)arg;
			if((errno = check_user_area(VERIFY_WRITE, (void *)k, sizeof(struct kbentry)))) {
				return errno;
			}
			if(k->kb_table < NR_MODIFIERS) {
				if(k->kb_index < NR_SCODES) {
					keymap[(k->kb_index * NR_MODIFIERS) + k->kb_table] = k->kb_value;
				} else {
					return -EINVAL;
				}
			} else {
				printk("%s(): kb_table value '%d' not supported.\n", __FUNCTION__, k->kb_table);
				return -EINVAL;
			}
		}
			break;

		case VT_OPENQRY:
		{
			int *val = (int *)arg;
			if((errno = check_user_area(VERIFY_WRITE, (void *)arg, sizeof(unsigned int)))) {
				return errno;
			}
			for(n = 1; n < NR_VCONSOLES + 1; n++) {
				tty = get_tty(MKDEV(VCONSOLES_MAJOR, n));
				if(!tty->count) {
					break;
				}
			}
			*val = (n < NR_VCONSOLES + 1 ? n : -1);
		}
			break;

		case VT_GETMODE:
		{
			struct vt_mode *vt_mode = (struct vt_mode *)arg;
			if((errno = check_user_area(VERIFY_WRITE, (void *)vt_mode, sizeof(struct vt_mode)))) {
				return errno;
			}
			memcpy_b(vt_mode, &vc->vt_mode, sizeof(struct vt_mode));
		}
			break;

		case VT_SETMODE:
		{
			struct vt_mode *vt_mode = (struct vt_mode *)arg;
			if((errno = check_user_area(VERIFY_READ, (void *)vt_mode, sizeof(struct vt_mode)))) {
				return errno;
			}
			if(vt_mode->mode != VT_AUTO && vt_mode->mode != VT_PROCESS) {
				return -EINVAL;
			}
			memcpy_b(&vc->vt_mode, vt_mode, sizeof(struct vt_mode));
			vc->vt_mode.frsig = 0;	/* ignored */
			tty->pid = current->pid;
			vc->switchto_tty = 0;
		}
			break;

		case VT_GETSTATE:
		{
			struct vt_stat *vt_stat = (struct vt_stat *)arg;
			if((errno = check_user_area(VERIFY_WRITE, (void *)vt_stat, sizeof(struct vt_stat)))) {
				return errno;
			}
			vt_stat->v_active = current_cons;
			vt_stat->v_state = 1;	/* /dev/tty0 is always opened */
			for(n = 1; n < NR_VCONSOLES + 1; n++) {
				tty = get_tty(MKDEV(VCONSOLES_MAJOR, n));
				if(tty->count) {
					vt_stat->v_state |= (1 << n);
				}
			}
		}
			break;

		case VT_RELDISP:
			if(vc->vt_mode.mode != VT_PROCESS) {
				return -EINVAL;
			}
			if(vc->switchto_tty < 0) {
				if(arg != VT_ACKACQ) {
					return -EINVAL;
				}
			} else {
				if(arg) {
					int switchto_tty;
					switchto_tty = vc->switchto_tty;
					vc->switchto_tty = -1;
					vconsole_select_final(switchto_tty);
				} else {
					vc->switchto_tty = -1;
				}
			}
			break;

		case VT_ACTIVATE:
			if(current_cons == MINOR(tty->dev) || IS_SUPERUSER) {
				if(!arg || arg > NR_VCONSOLES) {
					return -ENXIO;
				}
				vconsole_select(--arg);
			} else {
				return -EPERM;
			}
			break;

		case VT_WAITACTIVE:
			if(current_cons == MINOR(tty->dev)) {
				break;
			}
			if(!arg || arg > NR_VCONSOLES) {
				return -ENXIO;
			}
			printk("ACTIVATING another tty!! (cmd = 0x%x)\n", cmd);
			break;

		default:
			return -EINVAL;
	}
	return 0;
}
