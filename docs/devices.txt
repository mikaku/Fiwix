Fiwix allocated devices
=======================

Character

Major | Minor | Device name   | Description
------+-------+---------------+------------------------------------------------
1				Memory devices
	1	/dev/mem	physical memory access
	2	/dev/kmem	kernel virtual memory access
	3	/dev/null	null device
	4	/dev/port	I/O port access
	5	/dev/zero	null byte source
	7	/dev/full	returns ENOSPC on write
	8	/dev/random	random number generator
	9	/dev/urandom	random number generator (same as above)

4				TTY devices
	0	/dev/tty0	current virtual console
	1	/dev/tty1	first virtual console
	...	...
	12	/dev/tty12	12th virtual console
	64	/dev/ttyS0	first UART serial port
	65	/dev/ttyS1	second UART serial port
	66	/dev/ttyS2	third UART serial port
	67	/dev/ttyS3	fourth UART serial port

5				Alternate TTY devices
	0	/dev/tty	current tty device
	1	/dev/console	system console
	2	/dev/ptmx	PTY master multiplex

6				Parallel printer devices
	0	/dev/lp0	first parallel printer port

10				Non-serial mice
	1	/dev/psaux	PS/2-style mouse port

29				Universal framebuffer
	0	/dev/fb0	first framebuffer

136				UNIX98 PTY slaves
	0	/dev/pts/0	first UNIX98 pseudo-TTY
	1	/dev/pts/1	second UNIX98 pseudo-TTY
	...




Block

Major | Minor | Device name   | Description
------+-------+---------------+------------------------------------------------
1				RAMdisk drives
	0	/dev/ram0	first RAMdisk drive
	1	/dev/ram1	second RAMdisk drive
	...	...
	9	/dev/ram9	10th RAMdisk drive

2				Floppy disks (controller 0)
	0	/dev/fd0	floppy disk drive 0
	1	/dev/fd1	floppy disk drive 1
	4	/dev/fd0h360	floppy disk drive 0 (5.25" 360KB)
	5	/dev/fd1h360	floppy disk drive 1 (5.25" 360KB)
	8	/dev/fd0h1200	floppy disk drive 0 (5.25" 1.2MB)
	9	/dev/fd1h1200	floppy disk drive 1 (5.25" 1.2MB)
	12	/dev/fd0h720	floppy disk drive 0 (3.5" 720KB)
	13	/dev/fd1h720	floppy disk drive 1 (3.5" 720KB)
	16	/dev/fd0h1440	floppy disk drive 0 (3.5" 1.44MB)
	17	/dev/fd1h1440	floppy disk drive 1 (3.5" 1.44MB)

3				Primary ATA interface (hard disk or CD-ROM)
	0	/dev/hda	whole hard disk/CD-ROM master
	1	/dev/hda1	first partition
	2	/dev/hda2	second partition
	3	/dev/hda3	third partition
	4	/dev/hda4	fourth partition
	64	/dev/hdb	whole hard disk/CD-ROM slave
	65	/dev/hdb1	first partition
	66	/dev/hdb2	second partition
	67	/dev/hdb3	third partition
	68	/dev/hdb4	fourth partition

22				Secondary ATA interface (hard disk or CD-ROM)
	0	/dev/hdc	whole hard disk/CD-ROM master
	1	/dev/hdc1	first partition
	2	/dev/hdc2	second partition
	3	/dev/hdc3	third partition
	4	/dev/hdc4	fourth partition
	64	/dev/hdd	whole hard disk/CD-ROM slave
	65	/dev/hdd1	first partition
	66	/dev/hdd2	second partition
	67	/dev/hdd3	third partition
	68	/dev/hdd4	fourth partition

