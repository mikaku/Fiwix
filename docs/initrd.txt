Initrd image size restriction
=============================
The Fiwix kernel supports very large initrd images with two different maximum
sizes depending on the type of the virtual memory split. If the configuration
option CONFIG_VM_SPLIT22 is not defined (the default case), the virtual memory
is divided as 3GB for the user and 1GB for the kernel, in this case the maximum
size of an initrd image will be near 1GB. When CONFIG_VM_SPLIT22 is defined,
which means 2GB for the user and 2GB for the kernel, then the maximum size can
be near 2GB.

In all, the memory layout of the Fiwix kernel imposes a small and, hopefully,
harmless restriction on an specific size of the initrd image.

Since the kernel Page Directory and Page Tables are placed right after the end
of the initrd image in memory, (see 'docs/kmem_layout.txt'), the PD and PTs will
probably conflict with a user program loaded at the address 0x08048000 if the
initrd image has a size of around 128MB.

In this case, is recommendable define a bigger initrd image to avoid PD and PTs
be placed in the same memory address as user programs.

