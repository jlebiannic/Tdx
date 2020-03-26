/*========================================================================
        E D I S E R V E R

        File:		private.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

==========================================================================
  @(#)TradeXpress $Id: private.h 47371 2013-10-21 13:58:37Z cdemory $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <errno.h>
#include <time.h>

#ifdef MACHINE_WNT
#include <io.h>
#include <ctype.h>
#endif

#ifndef MACHINE_MIPS
#include <stdlib.h>
#endif

#ifndef MACHINE_HPUX
extern int errno;
#endif

#define PROTO(x) x

extern void		bail_out	PROTO((char*,...));
#define sqlite_bail_out bail_out

#define UNUSED_PARAM(x) (void)x
