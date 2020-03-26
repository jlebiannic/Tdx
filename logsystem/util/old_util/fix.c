/*========================================================================
        E D I S E R V E R

        File:		fix.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	fix:
	Try to make the logsystem usable after a mishap.

	Every record is checked, and
	freelist is reconstructed from inactive ones.
	Map is updated.
	If bad entries show up as active after this,
	they have to be removed manually.

	Hints are removed, they will grow back as system lives.

	Freelist gets ordered in index-order, biggest indexes first.
	It is easiest to do, but results in unnatural ordering of
	indexes in later insertions.

	pack:
	Pack active entries to the beginning of file
	and rearrange freelist.
	Hints are removed here too.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: fix.c 55239 2019-11-19 13:50:31Z sodifrance $")
LIBRARY(liblogsystem_version)
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 23.08.96/JN	Manually rmed .created/.modified got mode -------
  3.02 29.06.05/CD  Force rmunref after fix and pack
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <fcntl.h>

#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"

extern int Pre_allocate;

void other_options(int argc, char **argv);
int do_rmunref(int argc, char **argv);
    
static void zero_hints();

int
do_fix(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx, previdx;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL);

	old_legacy_logsys_readmap(log);

	log->map->numused = 0;
	log->map->firstfree = 0;
	log->map->lastfree = 0;
	log->map->lastindex = 1;

	previdx = 0;

	for (idx = 1; (entry = logentry_readraw(log, idx)) != NULL; ++idx) {

		log->map->lastindex = idx;

		if (ACTIVE_ENTRY(entry->record)) {

			if (entry->record_idx != LS_VINDEX(idx, entry->record_generation)
			 || entry->record_nextfree) {

				entry->record_idx = LS_VINDEX(idx, entry->record_generation);
				entry->record_nextfree = 0;

				logentry_writeraw(entry);
			}
			log->map->numused++;
		} else {

			log->map->firstfree = idx;
			if (log->map->lastfree == 0)
				log->map->lastfree = idx;

			if (entry->record_nextfree != previdx) {
				entry->record_nextfree = previdx;

				logentry_writeraw(entry);
			}
			previdx = idx;
		}
		logentry_free(entry);
	}

	old_legacy_logsys_writemap(log);

	zero_hints(log);

    /* 3.02/CD force rmunref to clean extension direcory */
    do_rmunref(argc, argv);

	exit(0);
}

int
do_pack(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx, previdx, newidx;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL);

	old_legacy_logsys_readmap(log);

	log->map->numused = 0;
	log->map->firstfree = 0;
	log->map->lastfree = 0;
	log->map->lastindex = 1;

	previdx = 1;

	/*
	 * Read every active entry there is
	 * and write back packed next to each other.
	 */
	for (idx = 1; (entry = logentry_readraw(log, idx)) != NULL; ++idx) {

		if (ACTIVE_ENTRY(entry->record)) {

			if (previdx != idx) {
				/*
				 *  Writing back only after
				 *  the in- and out-indexes
				 *  got different (a hole was seen).
				 */
				newidx = LS_VINDEX(previdx, entry->record_generation);
				old_legacy_logentry_movefiles(entry, newidx);

				entry->idx = previdx;
				entry->record_idx = newidx;
				entry->record_nextfree = 0;

				logentry_writeraw(entry);
			}
			++previdx;
			log->map->numused++;
		}
		log->map->lastindex = idx;
		logentry_free(entry);
	}
	log->map->firstfree = 0;
	log->map->lastfree  = 0;

	/*
	 * Read entries above packed active ones
	 * and mark as available.
	 */
	for (idx = previdx; (entry = logentry_readraw(log, idx)) != NULL; ++idx) {

		if (idx > Pre_allocate) {
			/*
			 *  Exceeding pre-allocation.
			 */
			if (ftruncate(log->datafd, idx * log->label->recordsize))
				bail_out("Cannot adjust data to new size");

			log->map->lastindex = idx - 1;
			break;
		}
		if (log->map->firstfree == 0)
			log->map->firstfree = idx;
		log->map->lastfree = idx;

		entry->record_ctime = 0;
		if (idx < log->map->lastindex)
			entry->record_nextfree = idx + 1;
		else
			entry->record_nextfree = 0;

		log->map->lastindex = idx;

		logentry_writeraw(entry);
		logentry_free(entry);
	}
	old_legacy_logsys_writemap(log);

	zero_hints(log);

    /* 3.02/CD force rmunref to clean extension directory */
    do_rmunref(argc, argv);

	exit(0);
}

static void
zero_hints(log)
	LogSystem *log;
{
	int fd, count;

	/*
	 * Truncate or remove hints.
	 */
	count = log->label->creation_hints;
	if (count > 0) {
		fd = logsys_openfile(log, LSFILE_CREATED,
			O_RDWR | O_TRUNC, 0644);
		if (fd >= 0)
			close(fd);
	} else {
		(void) unlink(logsys_filepath(log, LSFILE_CREATED));
	}

	count = log->label->modification_hints;
	if (count > 0) {
		fd = logsys_openfile(log, LSFILE_MODIFIED,
			O_RDWR | O_TRUNC, 0644);
		if (fd >= 0)
			close(fd);
	} else {
		(void) unlink(logsys_filepath(log, LSFILE_MODIFIED));
	}
}

int
do_salvage(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx;
	time_t now;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL);

	old_legacy_logsys_readmap(log);

	log->map->numused = 0;
	log->map->firstfree = 0;
	log->map->lastfree = 0;
	log->map->lastindex = 1;

	time(&now);

	for (idx = 1; (entry = logentry_readraw(log, idx)) != NULL; ++idx) {

		log->map->lastindex = idx;

		if (entry->record_ctime == 0)
			entry->record_ctime = now;
		if (entry->record_mtime == 0)
			entry->record_mtime = now;
		entry->record_idx = LS_VINDEX(idx, entry->record_generation);
		entry->record_nextfree = 0;

		logentry_writeraw(entry);
		log->map->numused++;
		logentry_free(entry);
	}

	old_legacy_logsys_writemap(log);

	zero_hints(log);

	exit(0);
}

