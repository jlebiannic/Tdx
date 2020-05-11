/*========================================================================
        E D I S E R V E R

        File:		logsystem/defcleanup.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Default cleaning-routine.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: defcleanup.c 55487 2020-05-06 08:56:27Z jlebiannic $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <fcntl.h>

#include "logsystem.dao.h"

/* Returns 0 if ok. */
int dao_logsys_defcleanup(LogSystem *log)
{
	LogFilter *lf = NULL;

	dao_logsys_trace(log, "default cleanup");

	/* filter order by MODIFIED, ASCending, LIMIT cleanup_count */
	dao_logfilter_setkey(&lf, "MODIFIED");
	dao_logfilter_setorder(&lf, 1);
	dao_logfilter_setmaxcount(&lf, log->label->cleanup_count);
	dao_logsys_compilefilter(log, lf);

	/* remove all entries and extensions that match the filter */
	dao_logsys_removebyfilter(log, lf, NULL);

	dao_logfilter_free(lf);

	/* as soon as one entry removed it's okay ! */
	return (dao_get_removed_entries_count(log) == 0);
}

int dao_get_removed_entries_count(LogSystem *log)
{
	if ((! log)||(! log->label->maxcount))
		return (0);
	else
		return (log->label->maxcount - logsys_dao_entry_count(log, NULL));
}
