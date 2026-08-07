/*
 * Link sentinels for legacy subsystems excluded by section reachability.
 * The image gate rejects any symbol below that survives --gc-sections.
 */

	.option norvc

	.macro DEAD_STUB name
	.section .text.dead_stub.\name, "ax"
	.weak \name
\name:
	ret
	.endm

	DEAD_STUB ext_accept
	DEAD_STUB ext_bind
	DEAD_STUB ext_close
	DEAD_STUB ext_connect
	DEAD_STUB ext_init
	DEAD_STUB ext_ioctl
	DEAD_STUB ext_listen
	DEAD_STUB ext_open
	DEAD_STUB ext_read
	DEAD_STUB ext_recvfrom
	DEAD_STUB ext_sendto
	DEAD_STUB ext_write
	DEAD_STUB inport_b
	DEAD_STUB inport_l
	DEAD_STUB inport_sl
	DEAD_STUB inport_sw
	DEAD_STUB inport_w
	DEAD_STUB outport_b
	DEAD_STUB outport_l
	DEAD_STUB outport_sl
	DEAD_STUB outport_sw
	DEAD_STUB outport_w
