/*========================================================================
        E D I S E R V E R

        File:		fixhints.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Every record is checked, and
	hints are re-built.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: fixhints.c 55239 2019-11-19 13:50:31Z sodifrance $")
LIBRARY(liblogsystem_version)
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 23.08.96/JN	Manually rmed .created/.modified got mode 
  4.01 31.07.02/CD      Changes in fixhints in order to use LSFILE_MODIFIED
			when removing modified hints file
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"

extern void *calloc();

void other_options(int argc, char **argv);

/*
 * Sort given index and time
 * into list (list_size members).
 * older members are pushed downwards in list.
 */
static void
sort_into(list, list_size, idx, stamp)
	LogHints *list;
	int list_size;
	LogIndex idx;
	TimeStamp stamp;
{
	int i, pos;

	for (pos = 0; pos < list_size; ++pos)
		if (list[pos].stamp < stamp) {
			for (i = list_size; --i > pos; )
				list[i] = list[i - 1];

			list[pos].idx = idx;
			list[pos].stamp = stamp;
			break;
		}
}

int
do_fixhints(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogLabel *label;
	LogIndex idx;
	LogHints *created, *modified;
	int fd;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL);

	old_legacy_logsys_readmap(log);

	label = log->label;

	/*
	 * Remove unneeded files and
	 * exit if no work to do.
	 */
	created = modified = NULL;

	if (label->creation_hints <= 0) {
		(void) unlink(logsys_filepath(log, LSFILE_CREATED));
	} else {
		created = calloc(label->creation_hints, sizeof(*created));
	}
	if (label->modification_hints <= 0) {
		/* CD 4.01 use modified instead of created !! */
		(void) unlink(logsys_filepath(log, LSFILE_MODIFIED));
	} else {
		modified = calloc(label->modification_hints, sizeof(*modified));
	}
	if (created == NULL && modified == NULL)
		exit(0);

	for (idx = 1; (entry = logentry_readraw(log, idx)) != NULL; ++idx) {

		if (ACTIVE_ENTRY(entry->record)) {
			if (created)
				sort_into(created, label->creation_hints,
					idx, entry->record_ctime);
			if (modified)
				sort_into(modified, label->modification_hints,
					idx, entry->record_mtime);
		}
		logentry_free(entry);
	}

	if (created) {
		fd = logsys_openfile(log, LSFILE_CREATED,
			O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (fd >= 0) {
			old_legacy_log_writebuf(fd, created,
				label->creation_hints * sizeof(*created),
				0);
			close(fd);
		}
	}
	if (modified) {
		fd = logsys_openfile(log, LSFILE_MODIFIED,
			O_CREAT | O_WRONLY | O_TRUNC, 0644);
		if (fd >= 0) {
			old_legacy_log_writebuf(fd, modified,
				label->modification_hints * sizeof(*modified),
				0);
			close(fd);
		}
	}
	exit(0);
}

