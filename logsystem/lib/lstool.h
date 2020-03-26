/*========================================================================
        E D I S E R V E R

        File:		lstool.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Globals etc. from lstool.
========================================================================*/
/*========================================================================
  @(#)TradeXpress $Id: lstool.h 47371 2013-10-21 13:58:37Z cdemory $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#ifndef _LOGSYSTEM_LSTOOL_H
#define _LOGSYSTEM_LSTOOL_H

#ifndef _BUILD_C
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#endif

extern int Access_mode;
extern int Quiet;
extern int Really;

extern char *Sysname;

extern int opterr, optind, optopt;
extern char *optarg;

#endif
