/*========================================================================
        E D I S E R V E R

        File:		orderfree.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Freelist gets ordered in modification-order,
	oldest first.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: orderfree.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <fcntl.h>

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/lstool.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"

extern int Pre_allocate;
extern void bail_out(char *, ...);

void other_options(int argc, char **argv);

struct free_list {
	LogIndex idx;
	TimeStamp stamp;
	struct free_list *link;
};

int
do_orderfree(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx;
	struct free_list *free_list, *p, **pp;

	other_options(argc, argv);

	log = logsys_open(Sysname, LS_DONTFAIL);

	old_legacy_logsys_readmap(log);

	/*
	 * Constructed a linked list from free records,
	 * sorted in modification-time, oldest first.
	 */

	free_list = NULL;
	idx = log->map->firstfree;

	while (idx) {
		if ((entry = logentry_readraw(log, idx)) == NULL)
			bail_out("Cannot read entry %d", idx);

		if ((p = (void *) malloc(sizeof(*p))) == NULL)
			bail_out("Out of memory");

		p->idx = entry->idx;
		p->stamp = entry->record_mtime;

		pp = &free_list;
		while (*pp && (*pp)->stamp > p->stamp)
			pp = &(*pp)->link;

		p->link = *pp;
		*pp = p;

		idx = entry->record_nextfree;
		logentry_free(entry);
	}
	/*
	 * Update records and map.
	 */
	if (free_list)
		log->map->firstfree = free_list->idx;

	while (free_list) {
		p = free_list;

		if ((entry = logentry_readraw(log, p->idx)) == NULL)
			bail_out("Cannot read entry %d", p->idx);

		if (p->link) {
			entry->record_nextfree = p->link->idx;
		} else {
			log->map->lastfree = p->idx;
			entry->record_nextfree = 0;
		}
		if (old_legacy_logentry_writeraw(entry))
			bail_out("Cannot write entry %d", p->idx);

		logentry_free(entry);

		free_list = p->link;
		free(p);
	}

	old_legacy_logsys_writemap(log);

	exit(0);
}

