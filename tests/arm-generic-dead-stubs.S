/*
 * Copyright 2026, Fiwix ARM contributors.
 * Distributed under the terms of the Fiwix License.
 *
 * Weak sentinels for unreachable legacy subsystems. The image gate rejects
 * any stub that survives section garbage collection.
 */

    .syntax unified
    .arch armv7-a
    .arm

    .macro dead_stub name
    .section .text.dead_stub.\name, "ax"
    .weak \name
\name:
    bx      lr
    .endm

    dead_stub ext_accept
    dead_stub ext_bind
    dead_stub ext_close
    dead_stub ext_connect
    dead_stub ext_init
    dead_stub ext_ioctl
    dead_stub ext_listen
    dead_stub ext_open
    dead_stub ext_read
    dead_stub ext_recvfrom
    dead_stub ext_sendto
    dead_stub ext_write
    dead_stub inport_b
    dead_stub inport_l
    dead_stub inport_sl
    dead_stub inport_sw
    dead_stub inport_w
    dead_stub outport_b
    dead_stub outport_l
    dead_stub outport_sl
    dead_stub outport_sw
    dead_stub outport_w

    .section .note.GNU-stack,"",%progbits
