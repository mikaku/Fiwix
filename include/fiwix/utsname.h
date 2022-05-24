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
 * fiwix/include/fiwix/utsname.h
 *
 * Copyright 2018-2022, Jordi Sanfeliu. All rights reserved.
 * Distributed under the terms of the Fiwix License.
 */

#ifndef _FIWIX_UTSNAME_H
#define _FIWIX_UTSNAME_H

#define _OLD_UTSNAME_LENGTH	8
#define _UTSNAME_LENGTH		64

#ifndef _UTSNAME_NODENAME_LENGTH
#define _UTSNAME_NODENAME_LENGTH _UTSNAME_LENGTH
#endif

/* very old structure describing the system and machine */
struct oldold_utsname
{
    char sysname[_OLD_UTSNAME_LENGTH + 1];
    char nodename[_OLD_UTSNAME_LENGTH + 1];
    char release[_OLD_UTSNAME_LENGTH + 1];
    char version[_OLD_UTSNAME_LENGTH + 1];
    char machine[_OLD_UTSNAME_LENGTH + 1];
};

/* old structure describing the system and machine */
struct old_utsname
{
    char sysname[_UTSNAME_LENGTH + 1];
    char nodename[_UTSNAME_NODENAME_LENGTH + 1];
    char release[_UTSNAME_LENGTH + 1];
    char version[_UTSNAME_LENGTH + 1];
    char machine[_UTSNAME_LENGTH + 1];
};

/* new structure describing the system and machine */
struct new_utsname
{
    /* name of this implementation of the operating system */
    char sysname[_UTSNAME_LENGTH + 1];

    /* name of this node on the network */
    char nodename[_UTSNAME_NODENAME_LENGTH + 1];

    /* current release level of this implementation */
    char release[_UTSNAME_LENGTH + 1];
    /* current version level of this release */
    char version[_UTSNAME_LENGTH + 1];

    /* name of the hardware type on which the system is running */
    char machine[_UTSNAME_LENGTH + 1];
    char domainname[_UTSNAME_LENGTH + 1];
};

extern struct new_utsname sys_utsname;
extern char UTS_MACHINE[_UTSNAME_LENGTH + 1];

#endif /* _FIWIX_UTSNAME_H */
