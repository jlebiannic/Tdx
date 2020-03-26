/*========================================================================
        E D I S E R V E R

        File:		port.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Portability aids.
========================================================================*/
/*========================================================================
  @(#)TradeXpress $Id: port.old_legacy.h 47371 2013-10-21 13:58:37Z cdemory $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#if 1
#define DIRENT struct dirent
#else
#define DIRENT struct direct
#endif

#ifdef MACHINE_HPUX
#define HAVE_MADVISE 0
#else
#define HAVE_MADVISE 1
#endif

#ifndef MACHINE_WNT
#define O_BINARY 0
#endif

#ifdef MACHINE_WNT

struct iovec {
	void *iov_base;
	int   iov_len;
};

#endif


