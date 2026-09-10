/*
 * fiwix/include/fiwix/ioctl.h
 */

#ifndef _FIWIX_IOCTL_H
#define _FIWIX_IOCTL_H

#define HDIO_GETGEO	0x0301	/* get device geometry */

#define BLKROSET	0x125D	/* set device read-only (0 = read-write) */
#define BLKROGET	0x125E	/* get read-only status (0 = read_write) */
#define BLKRRPART	0x125F	/* re-read partition table */
#define BLKGETSIZE	0x1260	/* return device size */
#define BLKFLSBUF	0x1261	/* flush buffer cache */
#define BLKSSZGET	0x1268	/* get block device sector size */
#define BLKBSZGET       0x1270	/* get device block size */
#define BLKBSZSET       0x1271	/* set device block size */

/* 0x54 is just a magic number to make these relatively unique ('T') */
#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A
#define FIONREAD	0x541B
#define TIOCINQ		FIONREAD
#define TIOCLINUX	0x541C
#define TIOCCONS	0x541D
#define TIOCGSERIAL	0x541E
#define TIOCSSERIAL	0x541F
#define TIOCPKT		0x5420
#define FIONBIO		0x5421
#define TIOCNOTTY	0x5422
#define TIOCSETD	0x5423
#define TIOCGETD	0x5424
#define TCSBRKP		0x5425	/* Needed for POSIX tcsendbreak() */
#define TIOCTTYGSTRUCT	0x5426  /* For debugging only */
#define TIOCSBRK	0x5427  /* BSD compatibility */
#define TIOCCBRK	0x5428  /* BSD compatibility */
#define TIOCGSID	0x5429  /* Return the session ID of FD */
#define TIOCGPTN	0x80045430 /* Get Pty Number (of pty-mux device) */
#define TIOCSPTLCK	0x40045431 /* Lock/unlock Pty */

#define FIONCLEX	0x5450  /* these numbers need to be adjusted. */
#define FIOCLEX		0x5451
#define FIOASYNC	0x5452
#define TIOCSERCONFIG	0x5453
#define TIOCSERGWILD	0x5454
#define TIOCSERSWILD	0x5455
#define TIOCGLCKTRMIOS	0x5456
#define TIOCSLCKTRMIOS	0x5457
#define TIOCSERGSTRUCT	0x5458	/* For debugging only */
#define TIOCSERGETLSR   0x5459	/* Get line status register */
#define TIOCSERGETMULTI 0x545A	/* Get multiport config  */
#define TIOCSERSETMULTI 0x545B	/* Set multiport config */

#define TIOCMIWAIT	0x545C	/* wait for a change on serial input line(s) */
#define TIOCGICOUNT	0x545D	/* read serial port inline interrupt counts */
#define TIOCGHAYESESP   0x545E  /* Get Hayes ESP configuration */
#define TIOCSHAYESESP   0x545F  /* Set Hayes ESP configuration */

/* Used for packet mode */
#define TIOCPKT_DATA		 0
#define TIOCPKT_FLUSHREAD	 1
#define TIOCPKT_FLUSHWRITE	 2
#define TIOCPKT_STOP		 4
#define TIOCPKT_START		 8
#define TIOCPKT_NOSTOP		16
#define TIOCPKT_DOSTOP		32

#define TIOCSER_TEMT    0x01	/* Transmitter physically empty */

#ifdef CONFIG_NET
#define SIOCADDRT		0x890B
#define SIOCDELRT		0x890C

#define SIOCGIFNAME		0x8910
#define SIOCSIFLINK		0x8911
#define SIOCGIFCONF		0x8912
#define SIOCGIFFLAGS		0x8913
#define SIOCSIFFLAGS		0x8914
#define SIOCGIFADDR		0x8915
#define SIOCSIFADDR		0x8916
#define SIOCGIFDSTADDR		0x8917
#define SIOCSIFDSTADDR		0x8918
#define SIOCGIFBRDADDR		0x8919
#define SIOCSIFBRDADDR		0x891A
#define SIOCGIFNETMASK		0x891B
#define SIOCSIFNETMASK		0x891C
#define SIOCGIFMETRIC		0x891D
#define SIOCSIFMETRIC		0x891E
#define SIOCGIFMEM		0x891F
#define SIOCSIFMEM		0x8920
#define SIOCGIFMTU		0x8921
#define SIOCSIFMTU		0x8922
#define SIOCSIFNAME		0x8923
#define SIOCSIFHWADDR		0x8924
#define SIOCGIFENCAP		0x8925
#define SIOCSIFENCAP		0x8926
#define SIOCGIFHWADDR		0x8927
#define SIOCGIFSLAVE		0x8929
#define SIOCSIFSLAVE		0x8930
#define SIOCADDMULTI		0x8931
#define SIOCDELMULTI		0x8932
#define SIOCGIFINDEX		0x8933
#define SIOGIFINDEX		SIOCGIFINDEX
#define SIOCSIFPFLAGS		0x8934
#define SIOCGIFPFLAGS		0x8935
#define SIOCDIFADDR		0x8936
#define SIOCSIFHWBROADCAST	0x8937
#define SIOCGIFCOUNT		0x8938

#define SIOCGIFMAP		0x8970
#define SIOCSIFMAP		0x8971
#endif /* CONFIG_NET */

#endif /* _FIWIX_IOCTL_H */
