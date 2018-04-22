/* Copyright (C) 1991, 1992, 1994, 1996 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/*
 *	POSIX Standard: 4.4 System Identification	<fiwix/utsname.h>
 */

#ifndef _FIWIX_UTSNAME_H
#define _FIWIX_UTSNAME_H

#define _OLD_UTSNAME_LENGTH	9
#define _UTSNAME_LENGTH		65

#ifndef _UTSNAME_NODENAME_LENGTH
#define _UTSNAME_NODENAME_LENGTH _UTSNAME_LENGTH
#endif

/* Very OLD structure describing the system and machine.  */
struct oldold_utsname
{
    char sysname[_OLD_UTSNAME_LENGTH];
    char nodename[_OLD_UTSNAME_LENGTH];
    char release[_OLD_UTSNAME_LENGTH];
    char version[_OLD_UTSNAME_LENGTH];
    char machine[_OLD_UTSNAME_LENGTH];
};

/* OLD structure describing the system and machine.  */
struct old_utsname
{
    char sysname[_UTSNAME_LENGTH];
    char nodename[_UTSNAME_NODENAME_LENGTH];
    char release[_UTSNAME_LENGTH];
    char version[_UTSNAME_LENGTH];
    char machine[_UTSNAME_LENGTH];
};

/* NEW structure describing the system and machine.  */
struct new_utsname
{
    /* Name of the implementation of the operating system.  */
    char sysname[_UTSNAME_LENGTH];

    /* Name of this node on the network.  */
    char nodename[_UTSNAME_NODENAME_LENGTH];

    /* Current release level of this implementation.  */
    char release[_UTSNAME_LENGTH];
    /* Current version level of this release.  */
    char version[_UTSNAME_LENGTH];

    /* Name of the hardware type the system is running on.  */
    char machine[_UTSNAME_LENGTH];
    char domainname[_UTSNAME_LENGTH];
};

extern struct new_utsname sys_utsname;
extern char UTS_MACHINE[_UTSNAME_LENGTH];

#endif /* _FIWIX_UTSNAME_H */
