/*========================================================================
        E D I S E R V E R

        File:		tr_ftruncate.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Dec  2 12:22:03 EET 1996

        Copyright (c) 1996 Telecom Finland/EDI Operations

	ftruncate should be removed, callers changed to native NT stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_ftruncate.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 02.12.96/JN	Created.
========================================================================*/

/*
 *  Temporary compatibility routines.
 */

#include <stdio.h>
#include <io.h>

int
ftruncate(int fd, long size)
{
	return (_chsize(fd, size));
}

