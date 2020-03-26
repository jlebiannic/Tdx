#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		memalloc.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Memory allocation for logsystem.

	Look at variables logsys_*_allocated
	when a leak is suspected.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: memalloc.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 08.01.2014/CGL(CG) TX-2495 Remplace LOGLEVEL_DEVEL (level 3) with LOGLEVEL_BIGDEVEL (level 4)
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "logsystem.h"
#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_constants.h"

void *malloc();
void *realloc();

/*
 * Counters to aid when core leaks.
 */

int logsys_entries_allocated;
int logsys_systems_allocated;
int logsys_labels_allocated;
int logsys_maps_allocated;

void *
old_legacy_log_malloc(size)
	int size;
{
	void *p = NULL;

	writeLog(LOGLEVEL_ALLDEVEL, "Entering old_legacy_log_malloc(%d)", size);
	p = malloc(size);
	if (p == NULL) {
		old_legacy_logsys_warning((void *) NULL, "Out of memory");
		exit(1);
	}
	return (p);
}

void *
old_legacy_log_realloc(old, size)
	void *old;
	int size;
{
	void *p = NULL;

	writeLog(LOGLEVEL_ALLDEVEL, "Entering old_legacy_log_realloc(*, %d)", size);
	if (old == NULL)
		p = malloc(size);
	else
		p = realloc(old, size);
	if (p == NULL) {
		old_legacy_logsys_warning((void *) NULL, "Out of memory");
		exit(1);
	}
	return (p);
}

char *
old_legacy_log_strdup(s)
	char *s;
{
	void *p = NULL;

	p = old_legacy_log_malloc(strlen(s) + 1);
	strcpy(p, s);
	return (p);
}

char *
old_legacy_log_str3dup(a, b, c)
	char *a, *b, *c;
{
	void *p = NULL;

	p = old_legacy_log_malloc(strlen(a) + strlen(b) + strlen(c) + 1);
	strcpy(p, a);
	strcat(p, b);
	strcat(p, c);

	return (p);
}

/*
 * As a convenience, save errno here.
 * After io-error, memory is often released
 * before returning error-indication.
 * Saving errno here simplifies this.
 */
static void
old_legacy_release(p)
	void *p;
{
	int e = errno;

	free(p);
	errno = e;
}

LogEntry *
old_legacy_logentry_alloc(log, idx)
	LogSystem *log;
	LogIndex idx;
{
	LogEntry *entry = old_legacy_logentry_alloc_skipclear(log, idx);

	if (entry->record)
		memset(entry->record, 0, log->label->recordsize);
	return (entry);
}

LogEntry *
old_legacy_logentry_alloc_skipclear(log, idx)
	LogSystem *log;
	LogIndex idx;
{
	LogEntry *entry;

	writeLog(LOGLEVEL_BIGDEVEL, "Begin old_legacy_logentry_alloc_skipclear");
	entry = old_legacy_log_malloc(sizeof(*entry));
	if (entry == NULL) {
		writeLog(LOGLEVEL_NORMAL, "ERROR : old_legacy_log_malloc return NULL (entry == NULL)");
	}

	entry->idx = idx;
	entry->logsys = log;
	/*CGL (CG) TX-2495 Remplace LOGLEVEL_DEVEL (level 3) with LOGLEVEL_BIGDEVEL (level 4)*/
	writeLog(LOGLEVEL_BIGDEVEL, "entry setted (idx & logsys)");

	if (log->mmapping) {
		entry->record = NULL;
	} else {
		entry->record = old_legacy_log_malloc(log->label->recordsize);
		if (entry->record == NULL) {
			writeLog(LOGLEVEL_NORMAL, "ERROR : old_legacy_log_malloc return NULL (entry->record == NULL)");
		}
	}
	++logsys_entries_allocated;
	writeLog(LOGLEVEL_BIGDEVEL, "logsys_entries_allocated=%d", logsys_entries_allocated);
	/*Fin CGL*/

	return (entry);
}

void
old_legacy_logentry_free(entry)
	LogEntry *entry;
{
	LogSystem *logsys = entry->logsys;
	void *record = entry->record;

	--logsys_entries_allocated;

	/*
	 * free the record only if it was not using mmapped io.
	 */
	if (record && ( logsys == NULL ||
			record < logsys->mmapping ||
			record >= logsys->mmapping_end)) {
		old_legacy_release(record);
	}
	entry->record = NULL;

	old_legacy_release(entry);
}

static char *
old_legacy_find_first_sep(path)
	char *path;
{
	for ( ; ; ++path) {
		switch (*path) {
		case 0:
			return (NULL);
#ifdef MACHINE_WNT
		case '\\':
#endif
		case '/':
			return (path);
		}
	}
}

LogSystem *
old_legacy_logsys_alloc(sysname)
	char *sysname;
{
	LogSystem *log;
	char *cp, *cp2, *owner;

	log = old_legacy_log_malloc(sizeof(*log));
	memset(log, 0, sizeof(*log));

	log->sysname = old_legacy_log_strdup(sysname);

	log->labelfd = -1;
	log->mapfd = -1;
	log->datafd = -1;

	log->walkidx = 1;
	log->mmapping = 0;

	log->map = old_legacy_log_malloc(sizeof(*log->map));
	memset(log->map, 0, sizeof(*log->map));

	++logsys_maps_allocated;
	++logsys_systems_allocated;

	/*
	 *  Second last path segment is the owner (normally)
	 */
	owner = log->sysname;
	for (cp = owner; cp2 = old_legacy_find_first_sep(cp); ) {
		owner = cp;
		cp = cp2 + 1;
	}
	log->owner = old_legacy_log_strdup(owner);
	if (cp = old_legacy_find_first_sep(log->owner, '/'))
		*cp = 0;

	return (log);
}

void old_legacy_logsys_free(log)
	LogSystem *log;
{
	--logsys_systems_allocated;

	if (log->map) {
		--logsys_maps_allocated;
		old_legacy_release(log->map);
	}
	if (log->owner) {
		old_legacy_release(log->owner);
	}
	old_legacy_release(log->sysname);
	old_legacy_release(log);
}

LogLabel *
old_legacy_loglabel_alloc(size)
	int size;
{
	LogLabel *label;

	label = old_legacy_log_malloc(size);
	memset(label, 0, size);

	++logsys_labels_allocated;

	return (label);
}

old_legacy_loglabel_free(lab)
	LogLabel *lab;
{
	--logsys_labels_allocated;

	old_legacy_release(lab);
}

