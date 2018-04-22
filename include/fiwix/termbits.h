/*
 * fiwix/include/fiwix/termbits.h
 *
 */

#ifndef _FIWIX_TERMBITS_H
#define _FIWIX_TERMBITS_H

/* These definitions match those used by the 4.4 BSD kernel.
   If the operating system has termios system calls or ioctls that
   correctly implement the POSIX.1 behavior, there should be a
   system-dependent version of this file that defines `struct termios',
   `tcflag_t', `cc_t', `speed_t' and the `TC*' constants appropriately.  */

/* Type of terminal control flag masks.  */
typedef unsigned long int tcflag_t;

/* Type of control characters.  */
typedef unsigned char cc_t;

/* Type of baud rate specifiers.  */
typedef long int speed_t;

#define NCCS 19

/* Terminal control structure.  */
struct termios {
	tcflag_t c_iflag;	/* Input mode flags */
	tcflag_t c_oflag;	/* Output mode flags */
	tcflag_t c_cflag;	/* Control mode flags */
	tcflag_t c_lflag;	/* Local mode flags */
	cc_t c_line;		/* Line discipline */
	cc_t c_cc[NCCS];	/* Control characters */
};

/* c_iflag bits */
#define IGNBRK	0000001		/* Ignore break condition */
#define BRKINT	0000002		/* Signal interrupt on break */
#define IGNPAR	0000004		/* Ignore characters with parity errors */
#define PARMRK	0000010		/* Mark parity and framing errors */
#define INPCK	0000020		/* Enable input parity check */
#define ISTRIP	0000040		/* Strip 8th bit off characters */
#define INLCR	0000100		/* Map NL to CR on input */
#define IGNCR	0000200		/* Ignore CR */
#define ICRNL	0000400		/* Map CR to NL on input */
#define IUCLC	0001000		/* Convert to lowercase */
#define IXON	0002000		/* Enable start/stop output control */
#define IXANY	0004000		/* Any character will restart after stop */
#define IXOFF	0010000		/* Enable start/stop input control */
#define IMAXBEL	0020000		/* Ring bell when input queue is full */

/* c_oflag bits */
#define OPOST	0000001		/* Perform output processing */
#define OLCUC	0000002
#define ONLCR	0000004		/* Map NL to CR-NL on output */
#define OCRNL	0000010
#define ONOCR	0000020
#define ONLRET	0000040
#define OFILL	0000100
#define OFDEL	0000200
#define NLDLY	0000400
#define   NL0	0000000
#define   NL1	0000400
#define CRDLY	0003000
#define   CR0	0000000
#define   CR1	0001000
#define   CR2	0002000
#define   CR3	0003000
#define TABDLY	0014000
#define   TAB0	0000000
#define   TAB1	0004000
#define   TAB2	0010000
#define   TAB3	0014000
#define   XTABS	0014000
#define BSDLY	0020000
#define   BS0	0000000
#define   BS1	0020000
#define VTDLY	0040000
#define   VT0	0000000
#define   VT1	0040000
#define FFDLY	0100000
#define   FF0	0000000
#define   FF1	0100000

/* c_cflag bit meaning */
#define CBAUD	0010017
#define  B0	0000000		/* hang up */
#define  B50	0000001		/* 50 baud */
#define  B75	0000002		/* 75 baud */
#define  B110	0000003		/* 110 baud */
#define  B134	0000004		/* 134 baud */
#define  B150	0000005		/* 150 baud */
#define  B200	0000006		/* 200 baud */
#define  B300	0000007		/* 300 baud */
#define  B600	0000010		/* 600 baud */
#define  B1200	0000011		/* 1200 baud */
#define  B1800	0000012		/* 1800 baud */
#define  B2400	0000013		/* 2400 baud */
#define  B4800	0000014		/* 4800 baud */
#define  B9600	0000015		/* 9600 baud */
#define  B19200	0000016		/* 19200 baud */
#define  B38400	0000017		/* 38400 baud */
#define EXTA B19200
#define EXTB B38400
#define CSIZE	0000060		/* Number of bits per byte (mask) */
#define   CS5	0000000		/* 5 bits per byte */
#define   CS6	0000020		/* 6 bits per byte */
#define   CS7	0000040		/* 7 bits per byte */
#define   CS8	0000060		/* 8 bits per byte */
#define CSTOPB	0000100		/* Two stop bits instead of one */
#define CREAD	0000200		/* Enable receiver */
#define PARENB	0000400		/* Parity enable */
#define PARODD	0001000		/* Odd parity instead of even */
#define HUPCL	0002000		/* Hang up on last close */
#define CLOCAL	0004000		/* Ignore modem status lines */
#define CBAUDEX 0010000
#define    B57600 0010001
#define   B115200 0010002
#define   B230400 0010003
#define   B460800 0010004
#define   B500000 0010005
#define   B576000 0010006
#define   B921600 0010007
#define  B1000000 0010010
#define  B1152000 0010011
#define  B1500000 0010012
#define  B2000000 0010013
#define  B2500000 0010014
#define  B3000000 0010015
#define  B3500000 0010016
#define  B4000000 0010017
#define CIBAUD	  002003600000	/* input baud rate (not used) */
#define CMSPAR	  010000000000		/* mark or space (stick) parity */
#define CRTSCTS	  020000000000		/* flow control */

/* c_lflag bits */
#define ISIG	0000001		/* Enable signals */
#define ICANON	0000002		/* Do erase and kill processing */
#define XCASE	0000004
#define ECHO	0000010		/* Enable echo */
#define ECHOE	0000020		/* Visual erase for ERASE */
#define ECHOK	0000040		/* Echo NL after KILL */
#define ECHONL	0000100		/* Echo NL even if echo is OFF */
#define NOFLSH	0000200		/* Disable flush after interrupt */
#define TOSTOP	0000400		/* Send SIGTTOU for background output */
#define ECHOCTL	0001000		/* Echo control characters as ^X */
#define ECHOPRT	0002000		/* Hardcopy visual erase */
#define ECHOKE	0004000		/* Visual erase for KILL */
#define FLUSHO	0010000		/* Output being flushed (state) */
#define PENDIN	0040000		/* Retype pending input (state) */
#define IEXTEN	0100000		/* Enable DISCARD and LNEXT */

/* c_cc characters */
#define VINTR 0			/* Interrupt character [ISIG] */
#define VQUIT 1			/* Quit character [ISIG] */
#define VERASE 2		/* Erase character [ICANON] */
#define VKILL 3			/* Kill-line character [ICANON] */
#define VEOF 4			/* End-of-file character [ICANON] */
#define VTIME 5			/* Time-out value (1/10 secs) [!ICANON] */
#define VMIN 6			/* Minimum # of bytes read at once [!ICANON] */
#define VSWTC 7
#define VSTART 8		/* Start (X-ON) character [IXON, IXOFF] */
#define VSTOP 9			/* Stop (X-OFF) character [IXON, IXOFF] */
#define VSUSP 10		/* Suspend character [ISIG] */
#define VEOL 11			/* End-of-line character [ICANON] */
#define VREPRINT 12		/* Reprint-line character [ICANON] */
#define VDISCARD 13		/* Discard character [IEXTEN] */
#define VWERASE 14		/* Word-erase character [ICANON] */
#define VLNEXT 15		/* Literal-next character [IEXTEN] */
#define VEOL2 16		/* Second EOL character [ICANON] */

/* Values for the ACTION argument to `tcflow'.  */
#define TCOOFF		0	/* Suspend output */
#define TCOON		1	/* Restart suspended output */
#define TCIOFF		2	/* Send a STOP character */
#define TCION		3	/* Send a START character */

/* Values for the QUEUE_SELECTOR argument to `tcflush'.  */
#define TCIFLUSH	0	/* Discard data received but not yet read */
#define TCOFLUSH	1	/* Discard data written but not yet sent */
#define TCIOFLUSH	2	/* Discard all pending data */

/* Values for the OPTIONAL_ACTIONS argument to `tcsetattr'.  */
#define TCSANOW		0	/* Change immediately */
#define TCSADRAIN	1	/* Change when pending output is written */
#define TCSAFLUSH	2	/* Flush pending input before changing */

#endif /* _FIWIX_TERMBITS_H */
