/* 
Copyright (C) 1988 Free Software Foundation

This file is part of GNU CC.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.  No author or distributor
accepts responsibility to anyone for the consequences of using it
or for whether it serves any particular purpose or works at all,
unless he says so in writing.  Refer to the GNU CC General Public
License for full details.

Everyone is granted permission to copy, modify and redistribute
GNU CC, but only under the conditions described in the
GNU CC General Public License.   A copy of this license is
supposed to have been given to you along with GNU CC so you
can know your rights and responsibilities.  It should be in a
file named COPYING.  Among other things, the copyright notice
and this notice must be preserved on all copies.  
*/

#ifndef __stdarg_h
#define __stdarg_h

typedef char *va_list;

/* Amount of space required in an argument list for an arg of type TYPE.
   TYPE may alternatively be an expression whose type is used.  */

#define __va_rounded_size(TYPE)  \
  (((sizeof (TYPE) + sizeof (int) - 1) / sizeof (int)) * sizeof (int))

#define va_start(AP, LASTARG) \
 (AP = ((char *) &(LASTARG) + __va_rounded_size(LASTARG)))

extern void va_end (va_list);
#define va_end(AP) /* Nothing */

#define va_arg(AP, TYPE) (AP += __va_rounded_size (TYPE), \
  *((TYPE *) (AP - __va_rounded_size (TYPE))))

#endif /* __stdarg_h */
