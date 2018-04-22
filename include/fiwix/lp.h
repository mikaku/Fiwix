/*
 * fiwix/include/fiwix/lp.h
 *
 * Copyright 2018, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_LP_H
#define _FIWIX_LP_H

#include <fiwix/fs.h>

#define LP_MAJOR	6	/* major number for /dev/lp[n] */
#define LP_MINORS	1

/*#define LP0_ADDR	0x3BC */
#define LP0_ADDR	0x378
/*#define LP2_ADDR	0x278 */

#define LP_STAT_ERR	0x08	/* printer error */
#define LP_STAT_SEL	0x10	/* select in */
#define LP_STAT_PE	0x20	/* paper empty or no paper */
#define LP_STAT_ACK	0x40	/* ack */
#define LP_STAT_BUS	0x80	/* printer busy */

#define LP_CTRL_STR	0x01	/* strobe */
#define LP_CTRL_AUT	0x02	/* auto line feed */
#define LP_CTRL_INI	0x04	/* initialize printer (reset) */
#define LP_CTRL_SEL	0x08	/* select printer */
#define LP_CTRL_IRQ	0x10	/* enable IRQ */
#define LP_CTRL_BID	0x20	/* bidireccional (on PS/2 ports) */

#define LP_RDY_RETR	100	/* retries before timeout */

struct lp {
	int data;	/* data port address */
	int stat;	/* status port address */
	int ctrl;	/* control port address */
	char flags;	/* flags */
};

int lp_open(struct inode *, struct fd *);
int lp_close(struct inode *, struct fd *);
int lp_write(struct inode *, struct fd *, const char *, __size_t);

void lp_init(void);

#endif /* _FIWIX_LP_H */
