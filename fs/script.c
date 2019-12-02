/*
 * fiwix/fs/script.c
 *
 * Copyright 2019, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#include <fiwix/limits.h>
#include <fiwix/process.h>
#include <fiwix/errno.h>
#include <fiwix/stdio.h>
#include <fiwix/string.h>

int script_load(char *interpreter, char *args, char *data)
{
	char *p;
	int n, noargs;

	/* has shebang? */
	if(data[0] != '#' && data[1] != '!') {
		return -ENOEXEC;
	}

	/* discard possible blanks before the interpreter name */
	for(n = 2; n < NAME_MAX; n++) {
		if(data[n] != ' ' && data[n] != '\t') {
			break;
		}
	}

	/* get the interpreter name */
	p = interpreter;
	noargs = 0;
	while(n < NAME_MAX) {
		if(data[n] == '\n' || data[n] == NULL) {
			noargs = 1;
			break;
		}
		if(data[n] == ' ' || data[n] == '\t') {
			break;
		}
		*p = data[n];
		n++;
		p++;
	}

	if(!interpreter) {
		return -ENOEXEC;
	}


	/* get the interpreter arguments */
	if(!noargs) {
		p = args;
		/* discard possible blanks before the arguments */
		while(n < NAME_MAX) {
			if(data[n] != ' ' && data[n] != '\t') {
				break;
			}
			n++;
		}
		while(n < NAME_MAX) {
			if(data[n] == '\n' || data[n] == NULL) {
				break;
			}
			*p = data[n];
			n++;
			p++;
		}
	}

	return 0;
}
