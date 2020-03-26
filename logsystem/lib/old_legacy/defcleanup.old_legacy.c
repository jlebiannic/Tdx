/*========================================================================
        E D I S E R V E R

        File:		logsystem/defcleanup.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Default cleaning-routine.
========================================================================*/
#include "conf/config.h"
/*LIBRARY(liblogsystem_version)
*/
MODULE("@(#)TradeXpress $Id: defcleanup.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <fcntl.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

static old_legacy_pos_bymodification();

/*
 * Returns the number of cleaned up entries.
 */

old_legacy_logsys_defcleanup(log)
	LogSystem *log;
{
	LogIndex idx;
	int nwanted = log->label->cleanup_count;
	int ncleaned = 0;
	int nselected = 0;
	LogEntry *entry, **list;
	int i, j;

	old_legacy_logsys_trace(log, "default cleanup");

	list = old_legacy_log_malloc(nwanted * sizeof(*list));

	/*
	 * Find the entries which get cleaned.
	 * pos_byxxx returns the position where to put this entry.
	 */
	for (idx = 1; entry = old_legacy_logentry_readraw(log, idx); ++idx) {

		i = old_legacy_pos_bymodification(entry, list, nselected);

		if (i >= nwanted) {
			old_legacy_logentry_free(entry);
			continue;
		}
		/*
		 * Make room for this one.
		 */
		j = nselected;
		if (j < nwanted)
			++nselected;
		else
			old_legacy_logentry_free(list[--j]);
		while (--j >= i)
			list[j + 1] = list[j];

		list[i] = entry;
	}

	for (i = 0; i < nselected; ++i) {
		if (old_legacy_logentry_removefiles(list[i], NULL) != 0) {
			old_legacy_logentry_free(list[i]);
			continue;
		}
		if (old_legacy_logentry_destroy(list[i]) != 0)
			continue;

		++ncleaned;
	}
	free(list);

	return (ncleaned);
}

static
old_legacy_pos_bymodification(entry, list, size)
	LogEntry *entry, **list;
	int size;
{
	int i;

	for (i = 0; i < size; ++i)
		if (entry->record_mtime < list[i]->record_mtime)
			break;
	return (i);
}

#if 0
	{
	extern int logsys_entries_allocated;
	int i; for (i = 0; i < nwanted; ++i)
	if (list[i]== 0) fprintf(stderr, "nil ");
	else fprintf(stderr, "%d/%d ", list[i]->idx, list[i]->record_mtime);
	fprintf(stderr, "\n");
	fprintf(stderr, "%d alloc\n", logsys_entries_allocated);
	}
#endif
