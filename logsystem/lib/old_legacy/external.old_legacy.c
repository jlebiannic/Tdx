/*========================================================================
        E D I S E R V E R

        File:		logsystem/external.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Run external programs.
========================================================================*/

#ifdef MACHINE_WNT

/* replace all */

#include "external.old_legacy.c.w.NT"

#else /* ! WNT */

#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: external.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <sys/wait.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

int
old_legacy_logentry_addprog(entry)
	LogEntry *entry;
{
	if (!entry->logsys->label->run_addprog)
		return (0);

	return (old_legacy_logsys_runprog(entry->logsys, LSFILE_ADDPROG,
		entry->record_idx));
}

int
old_legacy_logentry_removeprog(entry)
	LogEntry *entry;
{
	if (!entry->logsys->label->run_removeprog)
		return (0);

	return (old_legacy_logsys_runprog(entry->logsys, LSFILE_REMOVEPROG,
		entry->record_idx));
}

int
old_legacy_logsys_thresholdprog(log)
	LogSystem *log;
{
	if (!log->label->run_thresholdprog)
		return (0);

	return (old_legacy_logsys_runprog(log, LSFILE_THRESHOLDPROG, -1));
}

int
old_legacy_logsys_cleanupprog(log)
	LogSystem *log;
{
	if (!log->label->run_cleanupprog)
		return (0);

	return (old_legacy_logsys_runprog(log, LSFILE_CLEANUPPROG, -1));
}

int
old_legacy_logsys_runprog(log, progname, vindex)
	LogSystem *log;
	char *progname;
	LogIndex vindex;
{
	int pid, i, status = 0;
	char tmp[64], *arg = NULL;
	char *fullprog = old_legacy_logsys_filepath(log, progname);
	char *basename = strrchr(progname, '/') + 1; /* loggin only */

	if (access(fullprog, 0) != 0) {
		old_legacy_logsys_warning(log, "No %s", fullprog);
		return (-2);
	}
	if (vindex != -1) {
		arg = tmp;
		sprintf(arg, "%d", vindex);
	}
	old_legacy_logsys_trace(log, "executing %s %s", fullprog, arg ? arg : "");

	fflush(stdout);

	if ((pid = fork()) == -1) {
		old_legacy_logsys_warning(log, "Cannot fork, %s not executed", basename);
		return (-3);
	}
	if (pid == 0) {
		/*
		 * Make sure the child does not inherit any signal blocks.
		 */
		old_legacy_logsys_signalsforchild();
		execl(fullprog, fullprog, arg, NULL);
		_exit(1);
	}
#ifdef MACHINE_MIPS
	/*
	 * This is a library...
	 * We could accidentally wait a child that was for someone
	 * else. He/she will get pissed.
	 *
	 * no waitpid. sigh.
	 */
	while ((i = wait(&status)) != pid && i != -1)
		old_legacy_logsys_warning(log, "Unexpected child %d ignored", i)
#else
	i = waitpid(pid, &status, 0);
#endif
	if (i != pid) {
		status = -1;
		old_legacy_logsys_trace(log, "%s did not return!", basename);
	} else {
		old_legacy_logsys_trace(log, "%s returns %x", basename, status);
	}
	return (status);
}

#endif /* ! WNT */

