/*
 * fiwix/include/fiwix/kparms.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_KPARMS_H
#define _FIWIX_KPARMS_H

#define CMDL_ARG_LEN	100	/* max. length of cmdline argument */
#define CMDL_NUM_VALUES	30	/* max. values of cmdline parameter */

struct kernel_params {
	int ps2_noreset;
	char bgaresolution[15 + 1];
	char initrd[DEVNAME_MAX + 1];
	int memsize;
	int extmemsize;
	int ramdisksize;
	int ro;
	int rootdev;
	char rootfstype[10 + 1];
	char rootdevname[DEVNAME_MAX + 1];
	int syscondev;
};

extern struct kernel_params kparms;

struct kernel_params_value {
	char *name;
	char *value[CMDL_NUM_VALUES];
	unsigned int sysval[CMDL_NUM_VALUES];
};

#endif /* _FIWIX_KPARMS_H */
