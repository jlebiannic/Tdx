/*========================================================================
        E D I S E R V E R

        File:		logsystem/allocation.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines to allocate and remove logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: allocation.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 01.12.95/JN	The data-part of a record was not zeroed on destroy.
  3.02 17.10.96/JN	Confusing compiler-warning removed.
========================================================================*/

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

LogEntry *
old_legacy_logentry_new(log)
	LogSystem *log;
{
	LogIndex idx;
	LogEntry *entry;
	int need_thresholdprog = 0;

retry:
	old_legacy_logsys_trace(log, "creating a new entry");

	/*
	 * If we get the map, it must be released with
	 * either writemap or releasemap.
	 */
	if (old_legacy_logsys_readmap(log))
		return (NULL);

	if (log->map->firstfree) {
		/*
		 * Pick up the first free entry.
		 */
		idx = log->map->firstfree;
		entry = old_legacy_logentry_readraw(log, idx);
		if (entry == NULL) {
			/*
			 * HELP!
			 * Cannot read the entry which was marked as free.
			 * Clear freelist, it has obviously gone bad.
			 */
old_legacy_logsys_warning(log, "Error reading free entry %d, freelist cleared", idx);
			log->map->firstfree = 0;
			log->map->lastfree = 0;

		} else if (ACTIVE_ENTRY(entry->record)) {
			/*
			 * HELP!
			 * Active entry on freelist.
			 * Clear freelist, it has obviously gone bad.
			 */
old_legacy_logsys_warning(log, "Active entry %d on freelist, freelist cleared", idx);
			log->map->firstfree = 0;
			log->map->lastfree = 0;

		} else {
			log->map->firstfree = entry->record_nextfree;
			if (log->map->firstfree == 0)
				log->map->lastfree = 0;

			goto gotit;
		}
	}
	old_legacy_logsys_trace(log, "no free entries to use");

	if (!log->label->maxcount || log->map->numused < log->label->maxcount) {
		/*
		 * Expand by one record.
		 */
		idx = ++log->map->lastindex;

		entry = old_legacy_logentry_alloc(log, idx);

		goto gotit;
	}
	old_legacy_logsys_trace(log, "maxcount %d reached", log->label->maxcount);

	/*
	 * No free records nor can we expand.
	 * Cleanup needed.
	 * While the cleanup succeeds we try again.
	 */
	old_legacy_logsys_releasemap(log);

	if (old_legacy_logsys_docleanup(log) == 0)
		goto retry;

	return (NULL);

gotit:
	old_legacy_logsys_trace(log, "got index %d", idx);

	log->map->numused++;

	entry->record_idx = LS_VINDEX(idx, entry->record_generation);
	entry->record_ctime =
	entry->record_mtime = old_legacy_log_curtime();
	entry->record_nextfree = 0;

	if (old_legacy_logentry_write(entry)) {
		/*
		 * Cannot write the entry...
		 */
		old_legacy_logsys_releasemap(log);
		old_legacy_logentry_free(entry);
		return (NULL);
	}
	/*
	 * Are we about to fill up ?
	 */
	if (log->label->cleanup_threshold > 0
	 && log->label->cleanup_threshold < log->map->numused)
		need_thresholdprog = 1;

	if (old_legacy_logsys_writemap(log)) {
		/*
		 * Uh oh. The entry was written but
		 * map update failed.
		 */
		old_legacy_logentry_free(entry);
		return (NULL);
	}

	/*
	 * Update hints
	 */
	old_legacy_logentry_created(entry);

	old_legacy_logsys_trace(log, "new %d ok", entry->record_idx);

	if (need_thresholdprog)
		old_legacy_logsys_thresholdprog(log);

	old_legacy_logentry_addprog(entry);

	return (entry);
}

int
old_legacy_logentry_destroy(entry)
	LogEntry *entry;
{
	LogSystem *log = entry->logsys;
	LogIndex idx = entry->idx;

	LogEntry *lastfree = NULL;

	old_legacy_logsys_trace(log, "destroying entry %d", idx);

	old_legacy_logentry_removeprog(entry);

	if (old_legacy_logsys_readmap(log)) {
		old_legacy_logentry_free(entry);
		return (-1);
	}
	if (log->map->firstfree == 0) {
		/*
		 * Freelist was empty.
		 */
		log->map->firstfree = idx;
		log->map->lastfree = idx;

		old_legacy_logsys_trace(log, "freelist was empty");
	} else {
		/*
		 * Read in last free record and link us after that.
		 */
		lastfree = old_legacy_logentry_readraw(log, log->map->lastfree);
		if (lastfree) {
			lastfree->record_nextfree = idx;
			log->map->lastfree = idx;

			old_legacy_logsys_trace(log, "freelist tail %d", lastfree->idx);
		} else {
old_legacy_logsys_warning(log, "Error reading last free entry %d, freelist cleared",
				log->map->lastfree);
			log->map->firstfree = 0;
			log->map->lastfree = 0;
		}
	}
	entry->record_ctime = 0;
	entry->record_mtime = old_legacy_log_curtime();
	entry->record_nextfree = 0;
	entry->record_generation++;

	/*
	 *  Zero rest of record.
	 *
	int len = entry->record->header.rest - entry->record_buffer;
	 */

	memset(entry->record->header.rest, 0,
		log->label->recordsize
			- offsetof(union disklogrecord, header.rest[0]));

	log->map->numused--;

	if (old_legacy_logentry_writeraw(entry))
		goto bad;
	if (lastfree && old_legacy_logentry_writeraw(lastfree))
		goto bad;

	/*
	 * Purge any hints there might be.
	 */
	old_legacy_logentry_purgehints(entry);

	/*
	 * We already deleted the entry.
	 * If this fails, what can we do ?
	 */
	old_legacy_logsys_writemap(log);

	old_legacy_logentry_free(entry);
	if (lastfree)
		old_legacy_logentry_free(lastfree);

	old_legacy_logsys_trace(log, "entry %d deleted", idx);

	return (0);
bad:
	old_legacy_logsys_releasemap(log);

	old_legacy_logentry_free(entry);
	if (lastfree)
		old_legacy_logentry_free(lastfree);

	old_legacy_logsys_trace(log, "failed to delete %d", idx);

	return (-2);
}

