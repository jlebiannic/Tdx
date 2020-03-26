/*========================================================================
        E D I S E R V E R

        File:		logsystem/sigmask.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Some signals are blocked during the time we have
	logsystem map inconsistent.

	logsys_blocksignals() and logsys_restoresignals()
	keep a counter so that nested calls work.
	Signalmask is restored when counter drops to zero.

========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: sigmask.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

#ifndef MACHINE_WNT

static sigset_t oldset;

#endif

static int nesting = 0;

void
old_legacy_logsys_blocksignals()
{
#ifndef MACHINE_WNT
	sigset_t newset;

	sigemptyset(&newset);
	sigaddset(&newset, SIGHUP);
	sigaddset(&newset, SIGINT);
	sigaddset(&newset, SIGALRM);
	sigaddset(&newset, SIGTERM);

	sigprocmask(SIG_BLOCK, &newset, nesting++ ? NULL : &oldset);
#endif
}

void
old_legacy_logsys_restoresignals()
{
#ifndef MACHINE_WNT
	if (--nesting > 0)
		return;
	else
	if (nesting < 0) {
		fprintf(stderr, "Unpaired call to logsys_restoresignals\n");
		nesting = 0;
		return;
	}

	sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
}

void
old_legacy_logsys_signalsforchild()
{
#ifndef MACHINE_WNT
	if (nesting <= 0)
		return;

	sigprocmask(SIG_SETMASK, &oldset, NULL);
#endif
}

