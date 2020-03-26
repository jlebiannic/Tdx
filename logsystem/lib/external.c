/*========================================================================
        E D I S E R V E R

        File:		logsystem/external.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Run external programs.
========================================================================*/

#ifdef MACHINE_WNT

/* replace runprog */

#include "external.c.w.NT"

#else /* ! WNT */

#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: external.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>

#include "logsystem.sqlite.h"

int sqlite_logsys_runprog(LogSystem *log, char *progname, LogIndex vindex)
{
	int pid, i, status = 0;
	char tmp[64], *arg = NULL;
	char *fullprog = sqlite_logsys_filepath(log, progname);
	char *basename = strrchr(progname, '/') + 1; /* loggin only */

	if (access(fullprog, 0) != 0)
    {
		sqlite_logsys_warning(log, "No %s", fullprog);
		return (-2);
	}
	if (vindex != 0)
    {
		arg = tmp;
		sprintf(arg, "%d", vindex);
	}
	sqlite_logsys_trace(log, "executing %s %s", fullprog, arg ? arg : "");

	fflush(stdout);

	if ((pid = fork()) == -1)
    {
		sqlite_logsys_warning(log, "Cannot fork, %s not executed", basename);
		return (-3);
	}
	if (pid == 0)
    {
		/* Make sure the child does not inherit any signal blocks. */
		execl(fullprog, fullprog, arg, NULL);
		_exit(1);
	}
	i = waitpid(pid, &status, 0);
	if (i != pid)
    {
		status = -1;
		sqlite_logsys_trace(log, "%s did not return!", basename);
	}
    else
    {
		sqlite_logsys_trace(log, "%s returns %x", basename, status);
	}
	return (status);
}

#endif /* ! WNT */

