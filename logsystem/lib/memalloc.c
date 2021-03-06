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
static char *version = "@(#)TradeXpress $Id: memalloc.c 55500 2020-05-12 14:34:55Z jlebiannic $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "private.h"
#include "logsystem.dao.h"
#include "data-access/commons/daoFactory.h"

void *malloc();
void *realloc();

/* Counters to aid when core leaks. */

int logsys_entries_allocated;
int logsys_systems_allocated;
int logsys_labels_allocated;

void * dao_log_malloc(int size)
{
	void *p = NULL;

	p = malloc(size);
	if (p == NULL)
    {
		dao_logsys_warning((void *) NULL, "Out of memory");
		exit(1);
	}
	return (p);
}

void * dao_log_realloc(void *old, int size)
{
	void *p = NULL;

	if (old == NULL) {
		p = malloc(size);
	} else {
		p = realloc(old, size);
	}

	if (p == NULL)
    {
		dao_logsys_warning((void *) NULL, "Out of memory");
		exit(1);
	}
	return (p);
}

char * dao_log_strdup(char *s)
{
	void *p = NULL;

	p = dao_log_malloc(strlen(s) + 1);
	strcpy(p, s);
	return (p);
}

char * dao_log_str3dup(char *a, char *b, char *c)
{
	void *p = NULL;

	p = dao_log_malloc(strlen(a) + strlen(b) + strlen(c) + 1);
	strcpy(p, a);
	strcat(p, b);
	strcat(p, c);

	return (p);
}

/* As a convenience, save errno here.
 * After io-error, memory is often released
 * before returning error-indication.
 * Saving errno here simplifies this. */
static void dao_release(void *p)
{
	int e = errno;

	free(p);
	errno = e;
}

LogEntry *dao_logentry_alloc_skipclear(LogSystem *log, LogIndex idx)
{
	LogEntry *entry;

	entry = dao_log_malloc(sizeof(*entry));

	entry->idx = idx;
	entry->logsys = log;
    entry->record = dao_log_malloc(log->label->recordsize);
	
	++logsys_entries_allocated;

	return (entry);
}

LogEntry *dao_logentry_alloc(LogSystem *log, LogIndex idx)
{
	LogEntry *entry = dao_logentry_alloc_skipclear(log, idx);

	if (entry->record) {
		memset(entry->record, 0, log->label->recordsize);
	}

	return (entry);
}

void dao_logentry_free(LogEntry *entry)
{
	void *record = entry->record;

	--logsys_entries_allocated;

	if (record) {
		dao_release(record);
	}
	entry->record = NULL;

	dao_release(entry);
}

static char *dao_find_first_sep(char *path)
{
	for ( ; ; ++path)
    {
		switch (*path)
        {
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

/**
 * Jira TX-3199 DAO
 * */
static char* getLastPathForSep(char *dirPath, char sep) {
	int n = strlen(dirPath);
	char *lastPath = dirPath + n;

	while (0 < n && dirPath[--n] != sep)
		;
	if (dirPath[n] == sep) {
		lastPath = dirPath + n + 1;
	} else {
		lastPath = dirPath;
	}
	return lastPath;
}

 #ifdef MACHINE_WNT
static char* getLastPath(char *dirPath) {
	getLastPathForSep(dirPath, '\\');
}
#else
static char* getLastPath(char *dirPath) {
	getLastPathForSep(dirPath, '/');
}
#endif

LogSystem *dao_logsys_alloc(char *sysname)
{
	LogSystem *log;
	char *cp, *cp2, *owner;
	log = dao_log_malloc(sizeof(*log));
	memset(log, 0, sizeof(*log));

	log->sysname = dao_log_strdup(sysname);
	// Jira TX-3199 DAO: get current table
	log->table = dao_log_strdup(getLastPath(sysname));
	// Jira TX-3199 DAO: création du dao correspondant à la valeur de 1
	log->dao = daoFactory_create(1);

	log->labelfd = -1;
	log->datafd = -1;

	log->walkidx = 0;

	++logsys_systems_allocated;

	/* Second last path segment is the owner (normally) */
	owner = log->sysname;
	for (cp = owner; (cp2 = dao_find_first_sep(cp)); )
    {
		owner = cp;
		cp = cp2 + 1;
	}
	log->owner = dao_log_strdup(owner);

	if ((cp = dao_find_first_sep(log->owner))) {
		*cp = 0;
	}

	return (log);
}

void dao_logsys_free(LogSystem *log)
{
	--logsys_systems_allocated;

	if (log->owner) {
		dao_release(log->owner);
	}

	dao_release(log->sysname);
	dao_release(log);
}

LogLabel *dao_loglabel_alloc(int size)
{
	LogLabel *label;

	label = dao_log_malloc(size);
	memset(label, 0, size);

	++logsys_labels_allocated;

	return (label);
}

int dao_loglabel_free(LogLabel *lab)
{
	--logsys_labels_allocated;
	dao_release(lab);
    return 0;
}

/*JRE 06.15 BG-81*/
LogEntryEnv *dao_logentry_alloc_env(LogSystem *log, LogIndexEnv idx)
{
	LogEntryEnv *entry;

	entry = dao_log_malloc(sizeof(*entry));

	entry->idx = idx;
	entry->logsys = log;
    entry->record = dao_log_malloc(log->label->recordsize);
	
	++logsys_entries_allocated;

	return (entry);
}

void dao_logentryenv_free(LogEntryEnv *entry)
{
	void *record = entry->record;

	--logsys_entries_allocated;

	if (record) {
		dao_release(record);
	}
	entry->record = NULL;

	dao_release(entry);
}
/*End JRE*/

