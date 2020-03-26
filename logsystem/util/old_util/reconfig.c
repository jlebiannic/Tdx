/*========================================================================
        E D I S E R V E R

        File:		reconfig.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reconfigure logsystem

	Label and data are rewritten according to new description.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: reconfig.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"

extern int Pre_allocate;

void logsys_createlabel(LogSystem *log);
int logsys_copyconfig(char *cfgfile, char *sysname);
void logsys_createdirs(char *sysname);
void logsys_createhints(LogSystem *log, int count, char *name);

int logsys_copysys(LogSystem *oldlog, LogSystem *newlog);
int logsys_growdata(LogSystem *log);
int logsys_cutdata(LogSystem *log);
int logentry_copyfields_everything(LogEntry *oldent, LogEntry *newent);


int
logsys_relabelsystem(sysname, cfgfile)
	char *sysname;
	char *cfgfile;
{
	LogSystem *newlog;

	newlog = logsys_alloc(sysname);

	newlog->open_mode = O_RDWR;

	newlog->label = logsys_readconfig(cfgfile,0);
	if (newlog->label == NULL)
		bail_out("Cannot open %s", cfgfile);

	if (!Quiet) {
		logsys_dumplabelfields(newlog->label);
	}
	if (Really) {
		logsys_createlabel(newlog);
	}
	return (0);
}

int
logsys_reconfsystem(sysname, cfgfile)
	char *sysname;
	char *cfgfile;
{
	LogSystem *newlog, *oldlog;

	newlog = logsys_alloc(sysname);

	newlog->open_mode = O_RDWR;

	newlog->label = logsys_readconfig(cfgfile,0);
	if (newlog->label == NULL)
		bail_out("Cannot open %s", cfgfile);

	if (!Quiet) {
		logsys_dumplabelfields(newlog->label);
	}
	if (Really) {
		int omask = umask(0);

		/*
		 * Read old label, write new label and rewrite data.
		 */
		oldlog = logsys_open(sysname, LS_DONTFAIL);

		logsys_createlabel(newlog);
		logsys_copysys(oldlog, newlog);
		logsys_growdata(newlog);
		logsys_cutdata(newlog);
		logsys_copyconfig(cfgfile, newlog->sysname);
		logsys_createdirs(newlog->sysname);
		logsys_createhints(newlog,
			newlog->label->creation_hints, LSFILE_CREATED);
		logsys_createhints(newlog,
			newlog->label->modification_hints, LSFILE_MODIFIED);

		umask(omask);
	}
	return (0);
}

/*
 * Copy data from old system to new one.
 * Systems can be the same, with different layout, or different ones.
 *
 * Recordsize might change, choose copying direction accordingly.
 *
 * All records are rewritten (to preserve freelist).
 */
int
logsys_copysys(oldlog, newlog)
	LogSystem *oldlog, *newlog;
{
	LogEntry *from, *to;
	LogIndex idx, adj, last;

	if (old_legacy_logsys_readmap(oldlog))
		bail_out("Cannot get map");

	if (newlog->label->recordsize <= oldlog->label->recordsize) {
		/*
		 * Shrinking record, copy 1..max
		 */
		adj = 1;
		idx = 1;
		last = oldlog->map->lastindex + 1;
	} else {
		/*
		 * Growing record, copy max..1
		 */
		adj = -1;
		idx = oldlog->map->lastindex;
		last = 0;
	}
	for ( ; idx != last; idx += adj) {

		if ((from = logentry_readraw(oldlog, idx)) == NULL)
			continue;

		to = old_legacy_logentry_alloc(newlog, idx);

		logentry_copyfields_everything(from, to);

		if (logentry_writeraw(to))
			bail_out("Cannot write record %d", idx);

		logentry_free(to);

		logentry_free(from);
	}
	old_legacy_logsys_releasemap(oldlog);
    return 0;
}

/*
 * Copy fields from an oldstyle record into a newstyle one.
 * If type changes field is not copied.
 * Fields can overlap, but records cannot point to same space.
 */

int
logentry_copyfields_everything(oldent, newent)
	LogEntry *oldent, *newent;
{
	LogField *oldfield = oldent->logsys->label->fields;
	LogField *newfield;

	for ( ; oldfield->type; ++oldfield) {

		newfield = logsys_getfield(newent->logsys,
			oldfield->name.string);
		if (newfield == NULL)
			continue;

		if (oldfield->type != newfield->type)
			continue;

		old_legacy_logentry_setvalue(newent, newfield,
			old_legacy_logentry_getvalue(oldent, oldfield));
	}
    return 0;
}

/*
 * Grow datafile to contain pre-allocated number of records.
 */
int
logsys_growdata(log)
	LogSystem *log;
{
	LogEntry *entry;
	LogIndex idx;

	if (old_legacy_logsys_readmap(log))
		bail_out("Cannot get map");

	/*
	 * No need to do anything if already .GE.
	 */
	if (log->map->lastindex >= Pre_allocate) {
		old_legacy_logsys_releasemap(log);
		return (0);
	}
	/*
	 * Write as many new free records as needed.
	 * Free pointers go with the index,
	 * previous freelist is appended to the last record.
	 */
	for (idx = log->map->lastindex + 1; idx <= Pre_allocate; ++idx) {

		entry = old_legacy_logentry_alloc(log, idx);

		if (idx == Pre_allocate)
			entry->record_nextfree = log->map->lastfree;
		else
			entry->record_nextfree = idx + 1;

		if (logentry_writeraw(entry))
			bail_out("Cannot write record %d", idx);

		logentry_free(entry);
	}

	/*
	 * Update and release map.
	 */
	log->map->firstfree = log->map->lastindex + 1;
	if (log->map->lastfree == 0)
		log->map->lastfree = Pre_allocate;

	log->map->lastindex = Pre_allocate;

	if (old_legacy_logsys_writemap(log))
		bail_out("Cannot update map");

	return (0);
}

/*
 * Carve off any leftover if the file got smaller
 */
int
logsys_cutdata(log)
	LogSystem *log;
{
	int recordcount = log->map->lastindex + 1;

	if (old_legacy_logsys_opendata(log))
		bail_out("Cannot open datafile");

	if (ftruncate(log->datafd, recordcount * log->label->recordsize))
		bail_out("Cannot adjust data to new size");

	return (0);
}

