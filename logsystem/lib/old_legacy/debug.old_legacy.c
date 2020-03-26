#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		debug.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines useful (?) for debugging logsystem code.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: debug.old_legacy.c 47371 2013-10-21 13:58:37Z cdemory $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

void
old_legacy_logsys_dumpheader(log)
	LogSystem *log;
{
	LogField *field;

	for (field = log->label->fields; field->type; ++field) {
		putchar(' ');
		printf("%s", field->header.string);
	}
	printf("\n");
}

void
old_legacy_logentry_dump(entry)
	LogEntry *entry;
{
	LogSystem *log = entry->logsys;
	LogField *field;

	for (field = log->label->fields; field->type; ++field) {
		putchar(' ');
		old_legacy_logentry_printfield(stdout, entry, field, NULL);
#if 0
		if (!ACTIVE_ENTRY(entry->record))) {
			printf(" free, next %d",
				entry->record_nextfree);
		}
#endif
	}
	printf("\n");
}

void
old_legacy_logsys_dumpmap(map)
	LogMap *map;
{
	printf("magic              0x%X\n", map->magic);
	printf("version            %d\n", map->version);
	printf("numused            %d\n", map->numused);
	printf("lastindex          %d\n", map->lastindex);
	printf("firstfree          %d\n", map->firstfree);
	printf("lastfree           %d\n", map->lastfree);
}

void
old_legacy_logsys_dumplabel(label)
	LogLabel *label;
{
	int i;

	printf("magic              0x%X\n", label->magic);
	printf("version            %d\n", label->version);
	printf("maxcount           %d\n", label->maxcount);
	printf("recordsize         %d\n", label->recordsize);
	printf("nfields            %d\n", label->nfields);

	printf("preferences        0x%x\n", label->preferences);

	printf("run addprog        %s\n", label->run_addprog ?
					"true" : "false");
	printf("run removeprog     %s\n", label->run_removeprog ?
					"true" : "false");
	printf("run thresholdprog  %s\n", label->run_thresholdprog ?
					"true" : "false");
	printf("run cleanupprog    %s\n", label->run_cleanupprog ?
					"true" : "false");

	printf("cleanup_count      %d\n", label->cleanup_count);
	printf("cleanup_threshold  %d\n", label->cleanup_threshold);

	printf("creation_hints     %d\n", label->creation_hints);
	printf("modification_hints %d\n", label->modification_hints);

	printf("diskusage max      %d kB\n", label->max_diskusage);

	printf("datafile max       %d kB\n",
		label->recordsize * label->maxcount / 1024);
}

void
old_legacy_logsys_dumplabeltypes(label)
	LogLabel *label;
{
	int i;

	for (i = 0; i < SIZEOF(label->types); ++i)
		if (label->types[i].start || label->types[i].end)
			printf("%-10s start %3d end %3d\n",
				old_legacy_logfield_typename(i),
				label->types[i].start,
				label->types[i].end);
}

void
old_legacy_logsys_dumplabelfields(label)
	LogLabel *label;
{
	LogField *f;
	int total_used = 0;
	int i;

	for (f = label->fields; f->type; ++f) {

		printf("%-13s %-7s %3d %3d ",
			f->name.string,
			old_legacy_logfield_typename(f->type),
			f->offset,
			f->size);
		i = printf("%s", f->format.string);
		while (i++ < 16)
			putchar(' ');
		i = printf("'%s'", f->header.string);

		putchar('\n');

		total_used += f->size;
	}
	printf("%d used, %d free\n",
		total_used,
		label->recordsize - total_used);
}

