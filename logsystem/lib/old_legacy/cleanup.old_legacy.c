/*========================================================================
        E D I S E R V E R

        File:		logsystem/cleanup.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

========================================================================*/
#include "conf/config.h"
/*LIBRARY(liblogsystem_version)
*/
MODULE("@(#)TradeXpress $Id: cleanup.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <fcntl.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

/*
 * This function should return zero
 * only if it succeeds to make some room,
 * or thinks it did.
 *
 * When this is entered, nothing should be locked,
 * except maybe .label, if hot fixes get implemented.
 */
int
old_legacy_logsys_docleanup(log)
	LogSystem *log;
{
	if (log->label->cleanup_count == 0) {
		old_legacy_logsys_warning(log, "No cleanup allowed");
		return (-1);
	}
	old_legacy_logsys_trace(log, "cleanup start");

	if (old_legacy_logsys_readmap(log)) {
		old_legacy_logsys_warning(log, "cleanup cannot get map");
		return (-1);
	}
	if (old_legacy_logsys_cleanup_lock_nonblock(log)) {
		/*
		 * Someone has locked the system against cleanup.
		 * Release the map we have,
		 * and wait until the cleanup is finished.
		 * Then try again, return zero lying that all went ok
		 * so our caller retries.
		 */
		old_legacy_logsys_trace(log, "waiting cleanup to finish");

		old_legacy_logsys_releasemap(log);
		old_legacy_logsys_cleanup_lock(log);
		old_legacy_logsys_cleanup_unlock(log);
		return (0);
	}
	/*
	 * Now we hold both the map and cleanup locks.
	 * Unlock map and let the user program run.
	 * We keep holding the cleanup lock.
	 */
	old_legacy_logsys_releasemap(log);

	old_legacy_logsys_blocksignals();
	old_legacy_logsys_cleanupprog(log);
	old_legacy_logsys_restoresignals();

	/*
	 * Did the userprog make any room ?
	 * If not, use the default cleanup routine.
	 */
	if (old_legacy_logsys_refreshmap(log)) {
		old_legacy_logsys_warning(log, "cleanup cannot get map II");
		return (-1);
	}
	if (log->map->numused < log->label->maxcount) {
		old_legacy_logsys_trace(log, "cleanup prog did some room");
		return (0);
	}
	if (old_legacy_logsys_defcleanup(log) > 0) {
		old_legacy_logsys_trace(log, "default cleanup did some room");
		return (0);
	}
	old_legacy_logsys_warning(log, "cleanup failed");

	return (-1);
}

