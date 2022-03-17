/*
 * fiwix/fs/iso9660/rrip.c
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/types.h>
#include <fiwix/errno.h>
#include <fiwix/fs.h>
#include <fiwix/stat.h>
#include <fiwix/filesystems.h>
#include <fiwix/buffer.h>
#include <fiwix/fs_iso9660.h>
#include <fiwix/mm.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

void check_rrip_inode(struct iso9660_directory_record *d, struct inode *i)
{
	unsigned int total_len;
	unsigned int len;
	unsigned int sig;
	int n, nm_len, rootflag;
	struct susp_rrip *rrip;
	unsigned int dev_h, dev_l;
	unsigned int ce_block, ce_offset, ce_size;
	struct buffer *buf;
	unsigned char *sue;
	int sl_len;
	struct rrip_sl_component *slc;

	ce_block = ce_offset = ce_size = 0;
	buf = NULL;
	total_len = isonum_711(d->length);
	len = isonum_711(d->name_len);
	if(!(len % 2)) {
		len++;
	}
	sue = (unsigned char *)d->name;
	nm_len = 0;

loop:
	if(ce_block && ce_size) { 
		/* FIXME: it only looks in one directory block */
		if(!(buf = bread(i->dev, ce_block, i->sb->s_blocksize))) {
			return;
		}
		sue = (unsigned char *)buf->data + ce_offset;
		total_len = ce_size;
		len = 0;
	}

	while(len < total_len) {
		rrip = (struct susp_rrip *)(sue + len);
		if(rrip->len == 0) {
			break;
		}
		sig = GET_SIG(rrip->signature[0], rrip->signature[1]);
		switch(sig) {
			case GET_SIG('S', 'P'):
				if(rrip->u.sp.magic[0] != SP_MAGIC1 || rrip->u.sp.magic[1] != SP_MAGIC2) {
					if(ce_block) {
						brelse(buf);
					}
					return;
				}
				break;
			case GET_SIG('C', 'E'):
				if(ce_block) {
					brelse(buf);
				}
				ce_block = isonum_733(rrip->u.ce.block);
				ce_offset = isonum_733(rrip->u.ce.offset);
				ce_size = isonum_733(rrip->u.ce.size);
				goto loop;
				break;
			case GET_SIG('E', 'R'):
				i->sb->u.iso9660.rrip = 1;
				printk("ISO 9660 Extensions: ");
				for(n = 0; n < rrip->u.er.len_id; n++) {
					printk("%c", rrip->u.er.data[n]);
				}
				printk("\n");
				break;
			case GET_SIG('P', 'X'):
				i->i_mode  = isonum_733(rrip->u.px.mode);
				i->i_nlink = isonum_733(rrip->u.px.nlink);
				i->i_uid   = isonum_733(rrip->u.px.uid);
				i->i_gid   = isonum_733(rrip->u.px.gid);
				break;
			case GET_SIG('P', 'N'):
				if(S_ISBLK(i->i_mode) || S_ISCHR(i->i_mode)) {
					dev_h = isonum_733(rrip->u.pn.dev_h);
					dev_l = isonum_733(rrip->u.pn.dev_l);
					i->rdev = MKDEV(dev_h, dev_l);
				}
				break;
			case GET_SIG('S', 'L'):
				sl_len = rootflag = 0;
				slc = (struct rrip_sl_component *)&rrip->u.sl.area;
				while(sl_len < (rrip->len - 5)) {
					if(sl_len && !rootflag) {
						nm_len++;
					}
					rootflag = 0;
					switch(slc->flags & 0xE) {
						case 0:
							nm_len += slc->len;
							break;
						case SL_CURRENT:
							nm_len += 1;
							break;
						case SL_PARENT:
							nm_len += 2;
							break;
						case SL_ROOT:
							nm_len += 1;
							rootflag = 1;
							break;
						default:
							printk("WARNING: %s(): unsupported RRIP SL flags %d.\n", __FUNCTION__, slc->flags & 0xE);
					}
					slc = (struct rrip_sl_component *)(((char *)slc) + slc->len + sizeof(struct rrip_sl_component));
					sl_len += slc->len + sizeof(struct rrip_sl_component);
				}
				i->i_size = nm_len;
				break;
			case GET_SIG('T', 'F'):
				n = 0;
				if(rrip->u.tf.flags & TF_CREATION) {
					i->i_ctime = isodate(rrip->u.tf.times[n++].time);
				}
				if(rrip->u.tf.flags & TF_MODIFY) {
					i->i_mtime = isodate(rrip->u.tf.times[n++].time);
				}
				if(rrip->u.tf.flags & TF_ACCESS) {
					i->i_atime = isodate(rrip->u.tf.times[n++].time);
				}
				if(rrip->u.tf.flags & TF_ATTRIBUTES) {
					i->i_ctime = isodate(rrip->u.tf.times[n++].time);
				}
				break;
		}
		len += rrip->len;
	}
	if(ce_block) {
		brelse(buf);
	}
}

int get_rrip_filename(struct iso9660_directory_record *d, struct inode *i, char *name)
{
	unsigned int total_len;
	unsigned int len;
	unsigned int sig;
	int nm_len;
	struct susp_rrip *rrip;
	unsigned int ce_block, ce_offset, ce_size;
	struct buffer *buf;
	unsigned char *sue;

	ce_block = ce_offset = ce_size = 0;
	buf = NULL;
	total_len = isonum_711(d->length);
	len = isonum_711(d->name_len);
	if(!(len % 2)) {
		len++;
	}
	sue = (unsigned char *)d->name;
	nm_len = 0;

loop:
	if(ce_block && ce_size) { 
		/* FIXME: it only looks in one directory block */
		if(!(buf = bread(i->dev, ce_block, i->sb->s_blocksize))) {
			return 0;
		}
		sue = (unsigned char *)buf->data + ce_offset;
		total_len = ce_size;
		len = 0;
	}

	while(len < total_len) {
		rrip = (struct susp_rrip *)(sue + len);
		if(rrip->len == 0) {
			break;
		}
		sig = GET_SIG(rrip->signature[0], rrip->signature[1]);
		switch(sig) {
			case GET_SIG('S', 'P'):
				if(rrip->u.sp.magic[0] != SP_MAGIC1 || rrip->u.sp.magic[1] != SP_MAGIC2) {
					if(ce_block) {
						brelse(buf);
					}
					return 0;
				}
				break;
			case GET_SIG('C', 'E'):
				if(ce_block) {
					brelse(buf);
				}
				ce_block = isonum_733(rrip->u.ce.block);
				ce_offset = isonum_733(rrip->u.ce.offset);
				ce_size = isonum_733(rrip->u.ce.size);
				goto loop;
			case GET_SIG('N', 'M'):
				if(rrip->u.nm.flags) { /* FIXME: & ~(NM_CONTINUE | NM_CURRENT | NM_PARENT)) { */
					printk("WARNING: %s(): unsupported NM flag settings (%d).\n", __FUNCTION__, rrip->u.nm.flags);
					if(ce_block) {
						brelse(buf);
					}
					return 0;
				}
				nm_len = rrip->len - 5;
				memcpy_b(name, rrip->u.nm.name, nm_len);
				name[nm_len] = 0;
				break;
		}
		len += rrip->len;
	}
	if(ce_block) {
		brelse(buf);
	}
	return nm_len;
}

int get_rrip_symlink(struct inode *i, char *name)
{
	unsigned int total_len;
	unsigned int len;
	unsigned int sig;
	int nm_len;
	struct susp_rrip *rrip;
	unsigned int ce_block, ce_offset, ce_size;
	struct buffer *buf;
	struct buffer *buf2;
	unsigned char *sue;
	struct iso9660_directory_record *d;
	__blk_t dblock;
	__off_t doffset;
	int sl_len, rootflag;
	struct rrip_sl_component *slc;

	dblock = (i->inode & ~ISO9660_INODE_MASK) >> ISO9660_INODE_BITS;
	doffset = i->inode & ISO9660_INODE_MASK;
	/* FIXME: it only looks in one directory block */
	if(!(buf = bread(i->dev, dblock, i->sb->s_blocksize))) {
		return -EIO;
	}
	d = (struct iso9660_directory_record *)(buf->data + doffset);

	ce_block = ce_offset = ce_size = 0;
	buf2 = NULL;
	total_len = isonum_711(d->length);
	len = isonum_711(d->name_len);
	if(!(len % 2)) {
		len++;
	}
	sue = (unsigned char *)d->name;
	nm_len = 0;

loop:
	if(ce_block && ce_size) { 
		/* FIXME: it only looks in one directory block */
		if(!(buf2 = bread(i->dev, ce_block, i->sb->s_blocksize))) {
			return 0;
		}
		sue = (unsigned char *)buf2->data + ce_offset;
		total_len = ce_size;
		len = 0;
	}

	while(len < total_len) {
		rrip = (struct susp_rrip *)(sue + len);
		if(rrip->len == 0) {
			break;
		}
		sig = GET_SIG(rrip->signature[0], rrip->signature[1]);
		switch(sig) {
			case GET_SIG('S', 'P'):
				if(rrip->u.sp.magic[0] != SP_MAGIC1 || rrip->u.sp.magic[1] != SP_MAGIC2) {
					if(ce_block) {
						brelse(buf2);
					}
					return 0;
				}
				break;
			case GET_SIG('C', 'E'):
				if(ce_block) {
					brelse(buf2);
				}
				ce_block = isonum_733(rrip->u.ce.block);
				ce_offset = isonum_733(rrip->u.ce.offset);
				ce_size = isonum_733(rrip->u.ce.size);
				goto loop;
			case GET_SIG('S', 'L'):
				sl_len = rootflag = 0;
				slc = (struct rrip_sl_component *)&rrip->u.sl.area;
				while(sl_len < (rrip->len - 5)) {
					if(sl_len && !rootflag) {
						strcat(name, "/");
						nm_len++;
					}
					rootflag = 0;
					switch(slc->flags & 0xE) {
						case 0:
							nm_len += slc->len;
							strncat(name, slc->name, slc->len);
							break;
						case SL_CURRENT:
							nm_len += 1;
							strcat(name, ".");
							break;
						case SL_PARENT:
							nm_len += 2;
							strcat(name, "..");
							break;
						case SL_ROOT:
							nm_len += 1;
							rootflag = 1;
							strcat(name, "/");
							break;
						default:
							printk("WARNING: %s(): unsupported RRIP SL flags %d.\n", __FUNCTION__, slc->flags & 0xE);
					}
					slc = (struct rrip_sl_component *)(((char *)slc) + slc->len + sizeof(struct rrip_sl_component));
					sl_len += slc->len + sizeof(struct rrip_sl_component);
				}
				name[nm_len] = 0;
				break;
		}
		len += rrip->len;
	}
	if(ce_block) {
		brelse(buf2);
	}
	brelse(buf);
	return nm_len;
}
