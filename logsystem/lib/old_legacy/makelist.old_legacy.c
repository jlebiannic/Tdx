/*========================================================================
        E D I S E R V E R

        File:		makelist.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: makelist.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
       03.04.96/JN	Carved off the other list-maker to clusterlist.c
  4.00 15.12.05/CD    use lastindex instead of numused  
                      to prevent cases when numused is not uptodate 
  5.00 20.01.14/CGL(CG) TX-2495: Remplace LOGLEVEL_DEVEL (level 3) 
								 with LOGLEVEL_BIGDEVEL (level 4)
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.h"
#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_constants.h"

LogEntry ** old_legacy_logsys_makelist(LogSystem *log, LogFilter *lf)
{
	LogEntry *entry, **entries;
	int maxcount;		/* all active entries in disk */
	int count = 0;		/* number checked so far */
	int n = 0;		/* number accepted */
	time_t now;

	writeLog(LOGLEVEL_BIGDEVEL, "Entering old_legacy_logsys_makelist");
	old_legacy_logsys_rewind(log);
	time(&now);
	old_legacy_logsys_refreshmap(log);

	maxcount = log->map->numused + 256;
	writeLog(LOGLEVEL_BIGDEVEL, "maxcount=%d", maxcount);
	
	entries = old_legacy_log_malloc((maxcount + 1) * sizeof(*entries));

	/* 4.00/CD: use lastindex instead of numused  to prevent cases when numused is not uptodate */
	writeLog(LOGLEVEL_BIGDEVEL, "log->map->lastindex=%d",log->map->lastindex);
	while (n < maxcount && count < log->map->lastindex) {
		writeLog(LOGLEVEL_BIGDEVEL, "begin while (n=%d, count=%d)", n, count);

		if ((entry = old_legacy_logentry_get(log)) == NULL) {
			writeLog(LOGLEVEL_ALLDEVEL, "old_legacy_logentry_get(log) return NULL when n=%d and count=%d", n, count);
			break;
		}
		writeLog(LOGLEVEL_BIGDEVEL, "set entry with old_legacy_logentry_get(log) (entry is not NULL)");

		if (entry->record_ctime < now) {
			writeLog(LOGLEVEL_ALLDEVEL, "entry->record_ctime < now  and count=%d, incrementing count", count);
			++count;
		}

		if (!old_legacy_logentry_passesfilter(entry, lf)) {
			writeLog(LOGLEVEL_ALLDEVEL, "old_legacy_logentry_passesfilter(entry, lf) is false => call old_legacy_logentry_free");
			old_legacy_logentry_free(entry);
		} else {
			writeLog(LOGLEVEL_ALLDEVEL, "old_legacy_logentry_passesfilter(entry, lf) is true => entries[n++] = entry, n=%d", n);
			entries[n++] = entry;
		}
		writeLog(LOGLEVEL_ALLDEVEL, "end while");
	}

	writeLog(LOGLEVEL_BIGDEVEL, "log->indexes_count=%d", log->indexes_count);
	/* store entry count before sorting to have the whole number of matching entries instead of the maxcount which is applied just after sorting */
	log->indexes_count = n;

	if (n == 0)
	{
		free(entries);
		return (NULL);
	}

	/* Do the sort (wont take long if there is no sorting in the filter). */
	writeLog(LOGLEVEL_BIGDEVEL, "sort entries");
	entries[n] = NULL;
	n = old_legacy_logsys_sortentries(entries, n, lf);
	entries[n] = NULL;

	if (n < maxcount) {
		writeLog(LOGLEVEL_BIGDEVEL,  "n < maxcount => call realloc");
		/* Less than assumed amount was returned, shrink.
		 * libmalloc shouldn't do any copying, just bookkeep. */
		entries = old_legacy_log_realloc(entries, (n + 1) * sizeof(*entries));
	}
	return (entries);
}

LogIndex *old_legacy_logsys_list_indexed(LogSystem *log, LogFilter *lf) {
	LogIndex *indexes;
	LogEntry **entries;
	int matchingIndexes; /* number of matching entries */

	writeLog(LOGLEVEL_BIGDEVEL,  "Entering old_legacy_logsys_list_indexed");
	/* retrieve the indexes comforming to the filter 
	* allocation of indexes is done here */
	entries = old_legacy_logsys_makelist(log, lf);

	if (entries == NULL)
		return NULL;

	matchingIndexes = 0;
	while (entries[matchingIndexes] != NULL)
		matchingIndexes++;

	indexes = old_legacy_log_malloc((matchingIndexes + 1) * sizeof(LogIndex));

	matchingIndexes = 0;
	while (entries[matchingIndexes] != NULL)
	{
		indexes[matchingIndexes] = entries[matchingIndexes]->record_idx;
		logentry_free(entries[matchingIndexes]);
		matchingIndexes++;
	}

    indexes[matchingIndexes] = 0;
	return indexes;
}

int old_legacy_logsys_entry_count(LogSystem* log, LogFilter* filter)
{
	LogEntry *entry;
	int maxcount;		/* all active entries in disk */
	int count = 0;		/* number checked so far */
	time_t now;
	int nbIndexes = 0;
	
/*	if (log->indexes == NULL) {
		log->indexes = old_legacy_logsys_list_indexed(log,filter);
	}
	return log->indexes_count;
*/
		/* We must to count filtered entries but not allow memory to be sure not to explode in Out Of Memory */
	writeLog(LOGLEVEL_BIGDEVEL, "Entering old_legacy_logsys_entry_count");
	/* Init the DB */
	old_legacy_logsys_rewind(log);
	time(&now);
	old_legacy_logsys_refreshmap(log);
	maxcount = log->map->numused + 256;
	writeLog(LOGLEVEL_BIGDEVEL, "maxcount=%d", maxcount);
	
	/* 4.00/CD: use lastindex instead of numused  to prevent cases when numused is not uptodate */
	writeLog(LOGLEVEL_DEVEL, "log->map->lastindex=%d", log->map->lastindex);
	
	while (nbIndexes < maxcount && count < log->map->lastindex) {
		writeLog(LOGLEVEL_BIGDEVEL, "begin while (nbIndexes=%d, count=", nbIndexes, count);
		
		if ((entry = old_legacy_logentry_get(log)) == NULL) {
			writeLog(LOGLEVEL_BIGDEVEL, "old_legacy_logentry_get(log) return NULL when nbIndexes=%d and count=%d", nbIndexes, count);
			break;
		}
		writeLog(LOGLEVEL_BIGDEVEL, "set entry with old_legacy_logentry_get(log) (entry is not NULL)");

		if (entry->record_ctime < now) {
			writeLog(LOGLEVEL_BIGDEVEL, "entry->record_ctime < now  and count=%d, incrementing count", count);
			++count;
		}
		
		if (old_legacy_logentry_passesfilter(entry, filter)) {
			writeLog(LOGLEVEL_BIGDEVEL, "old_legacy_logentry_passesfilter(entry, filter) is true => nbIndexes++, nbIndexes=%d", nbIndexes);
			nbIndexes++;
		} else {
			writeLog(LOGLEVEL_BIGDEVEL, "old_legacy_logentry_passesfilter(entry, lf) is false => not in count");
		}
		old_legacy_logentry_free(entry);
		writeLog(LOGLEVEL_BIGDEVEL, "end while");
	}

	writeLog(LOGLEVEL_BIGDEVEL, "nbIndexes=%d (count=%d)", nbIndexes, count);

	return nbIndexes;
}
