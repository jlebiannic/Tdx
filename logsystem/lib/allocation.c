/*========================================================================
        E D I S E R V E R

        File:		logsystem/allocation.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines to create/destroy logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: allocation.c 55415 2020-03-04 18:11:43Z jlebiannic $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 01.12.95/JN	The data-part of a record was not zeroed on destroy.
  3.02 17.10.96/JN	Confusing compiler-warning removed.
  3.03 18.05.17/TCE(CG) EI-325 delete an access from a previously freed pointer
========================================================================*/

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>

#include "logsystem.sqlite.h"
#include "dastub.h"

LogEntry *sqlite_logentry_new(LogSystem *log)
{
	LogIndex idx;
	LogEntry *entry;

	sqlite_logsys_trace(log, "creating a new entry");

	/* Pick up a new entry in the database and ask her the new index */
	/* Jira TX-3199 DAO: stub dao/sqlite */
	idx = dao_logentry_new(log);

	if (idx == 0) 
		return NULL;

	/* clean allocation (record zeroed) */
	entry = sqlite_logentry_alloc(log, idx);

	/* we've got an index so we have a new entry */
	sqlite_logsys_trace(log, "new index %d ok", idx);
	/* update timestamps */ 
	sqlite_logentry_settimebyname(entry,"CREATED",sqlite_log_curtime());
	sqlite_logentry_settimebyname(entry,"MODIFIED",sqlite_log_curtime());

	return (entry);
}

int sqlite_logentry_destroy(LogEntry *entry)
{
	LogIndex idx = entry->idx;

    /* what we are about to do */
	sqlite_logsys_trace(entry->logsys, "destroying entry %d", idx);
    /* real remove in the database */
    logentry_sqlite_destroy(entry);
    /* no matter the result, free the memory entry */
	sqlite_logentry_free(entry);
    
    return 0;
}
