/*========================================================================
        E D I S E R V E R

        File:		check.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Make a thorough check for inconsistencies.

	Free list is scanned thru, for loops.
	Every free entry is added into a bitfield, and
	this bitfield is used later when reading every record in order.

	Found errors are reported, nothing is fixed here.
	Exit value is the number of errors detected.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: check.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **libsused = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 19.05.05/CD	Extention of allocation not done correctly in setbit.
                        resulting in a core dump when the INDEX 800000 is in 
			in the freelist
  3.02 05.12.06/LM  Ensure that bitfield is large enough in all cases
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"

static int errors;
static int column;
static int bitsize;
static unsigned char *bitfield;

static LogIndex lastprinted, printskipped = -1;

#ifdef MACHINE_MIPS
#define PROTO(x) ()
#else
#define PROTO(x) x
#endif

static int  setbit PROTO((int)), bitset PROTO((int));
static void ckcol PROTO((void)), printidx PROTO((LogIndex));
void checkhints PROTO((LogSystem *, int maxc, char *, LogHints *, int count));
extern void other_options PROTO((int argc, char **argv));


#undef PROTO

#define ERROR { ckcol(); ++errors; column = 0; }


int
do_check(argc, argv)
	int argc;
	char **argv;
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx, nxt;
	LogIndex last_actidx;

	LogHints *hints;
	int count, freelist_count, free_count, active_count;

	other_options(argc, argv);

	column = 0;
	errors = 0;

	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	printf("\n");
	printf("types:\n");
	old_legacy_logsys_dumplabeltypes(log->label);
	printf("fields:\n");
	logsys_dumplabelfields(log->label);

	printf("\n");
	printf("label:\n");
	old_legacy_logsys_dumplabel(log->label);

	printf("\n");
	printf("map:\n");
	old_legacy_logsys_refreshmap(log);
	old_legacy_logsys_dumpmap(log->map);

	printf("\n");
	printf("freelist:\n");

	/*
	 * Scan freelist.
	 */
	active_count = 0;
	free_count = 0;
	freelist_count = 0;

	lastprinted = -1;

	idx = log->map->firstfree;
	while (idx) {
		++freelist_count;

		printidx(idx);

		if (idx > log->map->lastindex) {
			ERROR
			printf("*** Entry %d above lastindex\n", idx);
		}
		entry = logentry_readraw(log, idx);
		if (entry == NULL) {
			ERROR
			printf("*** Cannot read %d\n", idx);
			break;
		}
		nxt = entry->record->header.nextfree;

		if (ACTIVE_ENTRY(entry->record)) {
			ERROR
			printf("*** Active entry %d in freelist\n", idx);
		}
		if (idx == log->map->lastfree && nxt != 0) {
			ERROR
			printf("*** Last free record %d continues %d\n",
				idx, nxt);
		}
		if (nxt == 0 && idx != log->map->lastfree) {
			ERROR
			printf("*** Freelist ends prematurely %d\n", idx);
		}
		if (setbit(idx)) {
			ERROR
			printf("*** Looping freelist %d\n", idx);
			break;
		}
		logentry_free(entry);
		idx = nxt;
	}
	ckcol();

	/*
	 * Check every record in order.
	 * Also check that lastindex is valid.
	 */
	printf("\n");
	printf("active:\n");

	idx = 0;
	last_actidx = 0;

	lastprinted = -1;

	while ((entry = logentry_readraw(log, idx + 1)) != NULL) {
		++idx;

		if (ACTIVE_ENTRY(entry->record)) {

			printidx(idx);

			last_actidx = idx;

			++active_count;

			if (bitset(idx)) {
				ERROR
				printf("*** Entry %d on freelist\n", idx);
			}
		} else {
			if (!bitset(idx)) {
				ERROR
				printf("*** Entry %d not on freelist\n", idx);
			}
			++free_count;
		}
		logentry_free(entry);
	}
	ckcol();

	printf("\n");
	printf("counts:\n");

	if (idx < log->map->lastindex) {
		ERROR
		printf("*** Cannot read lastindex %d\n", idx);
	}
	if (last_actidx > log->map->lastindex) {
		ERROR
		printf("*** Can read %d, above lastindex %d\n",
			idx, log->map->lastindex);
	}

	/*
	 * Check that counts are consistent.
	 */
	if (freelist_count + log->map->numused != log->map->lastindex) {
		ERROR
		printf("*** Differing counts, %d + %d = %d != %d\n",
			freelist_count, log->map->numused,
			freelist_count + log->map->numused,
			log->map->lastindex);
	}
	if (freelist_count != free_count) {
		ERROR
		printf("*** Differing free counts, %d != %d\n",
			freelist_count, free_count);
	}
	if (active_count != log->map->numused) {
		ERROR
		printf("*** Differing active counts, %d != %d\n",
			active_count, log->map->numused);
	}

	/*
	 * Errors in hints are not very severe,
	 * check anyway that there is no duplicates and that
	 * the number of entries has not overgrown configured limit.
	 */
	printf("\n");
	printf("hints:\n");

	hints = old_legacy_logsys_createdhints(log, &count);
	checkhints(log, log->label->creation_hints, "creation",
		hints, count);

	hints = old_legacy_logsys_modifiedhints(log, &count);
	checkhints(log, log->label->modification_hints, "modification",
		hints, count);

	if (bitfield)
		free(bitfield);

	exit(errors < 255 ? errors : 255);
}

/*
 * Mark index in bitmap.
 * Grow bitmap as needed.
 * Return previous state of index, marked or not.
 */
static int
setbit(bit)
	int bit;
{
	int was;
	int hunk = 10000;

	if (bitfield == NULL) {
		bitsize = hunk * 8;
		bitfield = (void *) malloc(bitsize / 8);
		memset(bitfield, 0, hunk);
	}
	while (bit >= bitsize) {
		/* 3.01/CD: prevent core dump by replacing > by >= 
		   otherwise we have a core when the entry 80000 (index 800000)
		   is in the free list ... should not arrive often !
		*/
		bitsize += hunk * 8;
		bitfield = (void *) realloc(bitfield, bitsize / 8);
		if (bitfield == NULL) {
			fprintf(stderr, "Out of memory, %d\n", bit);
			exit(1);
		}
		memset(bitfield + bitsize / 8 - hunk, 0, hunk);
	}
	was = (bitfield[bit / 8] & (1 << (bit % 8)));

	bitfield[bit / 8] |= (1 << (bit % 8));

	return (was);
}

/*
 * Is index marked ?
 */
static int
bitset(bit)
	int bit;
{
	if (bit >= bitsize)
		return (0);

	return (bitfield[bit / 8] & (1 << (bit % 8)));
}

/*
 * Print a newline if current column is getting near right side.
 */
static void
ckcol()
{
	if (printskipped != -1) {
		if (printskipped != lastprinted + 1
		 && printskipped != lastprinted - 1)
			column += printf("..");
		else
			column += printf(" ");
		column += printf("%d", printskipped);
	}
	if (column > 0) {
		printf("\n");
	}
	lastprinted = -1;
	printskipped = -1;
	column = 0;
}

static void
printidx(idx)
	LogIndex idx;
{
	if (lastprinted == -1) {
		if (column > 0)
			column += printf(" ");
		column += printf("%d", idx);
		lastprinted = idx;
		printskipped = -1;

	} else if (idx == lastprinted + 1) {
		printskipped = idx;
	} else if (idx == lastprinted - 1) {
		printskipped = idx;

	} else if (idx == printskipped + 1) {
		printskipped = idx;
	} else if (idx == printskipped - 1) {
		printskipped = idx;

	} else {
		if (printskipped != -1) {
			if (printskipped != lastprinted + 1
			 && printskipped != lastprinted - 1)
				column += printf("..");
			else
				column += printf(" ");
			column += printf("%d", printskipped);
		}
		column += printf(" %d", idx);
		lastprinted = idx;
		printskipped = -1;
	}
	if (column > 70)
		ckcol();
}

void
checkhints(log, maxcount, name, hints, count)
	LogSystem *log;
	int maxcount;
	char *name;
	LogHints *hints;
	int count;
{
	int i, j;
	int dupcount = 0;
	int badcount = 0;
	int overlastcount = 0;
	int freecount = 0;

	if (hints == NULL)
		return;

	if (count > maxcount) {
		ERROR
		printf("*** Excessive %s-hints, %d > %d\n",
			name, count, maxcount);
	}
	for (i = 0; i < count; ++i) {
		if (hints[i].idx <= 0)
			++badcount;
		if (hints[i].idx > log->map->lastindex)
			++overlastcount;
		if (bitset(hints[i].idx))
			++freecount;

		/*
		 * This finds every duplicate twice.
		 */
		for (j = 0; j < count; ++j)
			if (i != j && hints[i].idx == hints[j].idx)
				++dupcount;
	}
	if (badcount) {
		ERROR
		printf("*** %d illegal indexes on %s-hints\n",
			badcount, name);
	}
	if (overlastcount) {
		ERROR
		printf("*** %d indexes over last on %s-hints\n",
			overlastcount, name);
	}
	if (freecount) {
		ERROR
		printf("*** %d %s-hints on free list\n",
			freecount, name);
	}
	if (dupcount) {
		ERROR
		printf("*** %d duplicate %s-hints\n",
			dupcount / 2, name);
	}
	free(hints);
}

