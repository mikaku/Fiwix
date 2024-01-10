Fiwix system console
====================
It is possible to specify multiple devices for console output. You can define
up to NR_SYSCONSOLES 'console=' kernel command line options to select which
device(s) to use for console output.

Refer to 'include/fiwix/kparms.h' to know which devices are supported as
system consoles.

The device '/dev/tty0' is the default when no device is specified. It is
directly linked to the current virtual console in use, so by default, all
kernel messages will go to the current virtual console.

Since the VGA graphics is always present in the PC i386 architecture, the early
log messages will appear only in the first virtual console defined, regardless
if a serial console was also defined.

