#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		port.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:17:57 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Some OS-specifics.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: port.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 04.04.96/JN	readv for those systems which do not have it.
  3.02 09.07.96/JN	SCO5 now has readv & ftruncate.
  3.03 24.02.99/HT  SCO supports ftruncate now
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <sys/uio.h>
#endif

#ifdef MACHINE_WNT
#define MACHINE_NEEDS_IOV
#include "port.old_legacy.h"
#else
#include <unistd.h>
#endif

#ifdef MACHINE_SCO
#ifndef _SCO_DS

 /* 24.02.99/HT obsoleted following definitions */
#if 0 

#define MACHINE_NEEDS_IOV

ftruncate(fd, size)
	int fd;
	int size;
{
	return (chsize(fd, size));
}
#endif /* end of obsoliting */
#endif /* SCO5 */
#endif /* MACHINE_SCO */

#ifdef MACHINE_NEEDS_IOV

writev(fd, iovec, iovcnt)
	int fd;
	struct iovec *iovec;
	int iovcnt;
{
	int total, n, len;
	char *base;

	for (total = 0; iovcnt-- > 0; ++iovec) {
		base = (char *) iovec->iov_base;
		len  = iovec->iov_len;

		for ( ; len > 0; total += n) {
			n = write(fd, base, len);
			if (n == 0)
				return (total);
			if (n < 0)
				return (-1);
			base += n;
			len  -= n;
		}
	}
	return (total);
}

readv(fd, iovec, iovcnt)
	int fd;
	struct iovec *iovec;
	int iovcnt;
{
	int total, n, len;
	char *base;

	for (total = 0; iovcnt-- > 0; ++iovec) {
		base = (char *) iovec->iov_base;
		len  = iovec->iov_len;

		for ( ; len > 0; total += n) {
			n = read(fd, base, len);
			if (n == 0)
				return (total);
			if (n < 0)
				return (-1);
			base += n;
			len  -= n;
		}
	}
	return (total);
}

#endif /* MACHINE_NEEDS_IOV */

