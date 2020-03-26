/*========================================================================
        E D I S E R V E R

        File:		makelist.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: clusterlist.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.01 03.04 96/JN	Clustering reads.
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

LogEntry ** old_legacy_logsys_makelist_indexed(LogSystem *log, LogFilter *lf)
{
	LogEntry *entry, **entries;
	LogIndex idx;
	int maxcount;		/* all active entries in disk */
	int count = 0;		/* number checked so far */
	int n = 0;		/* number accepted */
	time_t now;
	int i, clustered;
	int tocluster = 32;     /* try this many records in one readv() */
	LogEntry *cluster[32];

	time(&now);
	old_legacy_logsys_refreshmap(log);

	maxcount = log->map->numused + 256;

	entries = (LogEntry **) old_legacy_log_malloc((maxcount + 1) * sizeof(*entries));

	/*
	 *  Start from block-boundary,
	 *  although the first record is never used.
	 *  Keep reads aligned.
	 */
	idx = 0;
	while (idx <= log->map->lastindex) {

		if (n >= maxcount || count >= log->map->numused)
			break;

		if (!old_legacy_logindex_passesfilter(LS_VINDEX(idx, 0), lf)) {
			/*
			 *  The filter contains index-limits,
			 *  dont read unnecessarily.
			 *  XXX should examine the filter more.
			 */
			idx++;
			tocluster = 1;
			continue;
		}
		clustered = old_legacy_logentry_readraw_clustered(log, idx,
				tocluster, cluster);
		if (clustered <= 0)
			break;

		/*
		 *  Skip the very first record, never used.
		 */
		i = 0;
		if (idx == 0)
			old_legacy_logentry_free(cluster[i++]);

		for ( ; i < clustered; ++i) {
			entry = cluster[i];

			if (!ACTIVE_ENTRY(entry->record)) {
				old_legacy_logentry_free(entry);
				continue;
			}
			if (entry->record_ctime < now)
				++count;

			if (!old_legacy_logentry_passesfilter(entry, lf))
				old_legacy_logentry_free(entry);
			else
				entries[n++] = entry;
		}
		idx += clustered;
	}

	/* store entry count before sorting to have the whole number of matching entries instead of the maxcount which is applied just after sorting */
	log->indexes_count = n;

	if (n == 0) {
		free(entries);
		return (NULL);
	}

	/*
	 * Do the sort (wont take long if there is no sorting in the filter).
	 */
	entries[n] = NULL;
	n = old_legacy_logsys_sortentries(entries, n, lf);
	entries[n] = NULL;

	if (n < maxcount) {
		/*
		 * Less than assumed amount was returned, shrink.
		 * libmalloc shouldn't do any copying, just bookkeep.
		 */
		entries = old_legacy_log_realloc(entries, (n + 1) * sizeof(*entries));
	}
	return (entries);
}

