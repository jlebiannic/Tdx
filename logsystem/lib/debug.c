#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		debug.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines useful (?) for debugging logsystem code.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: debug.c 47371 2013-10-21 13:58:37Z cdemory $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <sys/types.h>

#include "logsystem.sqlite.h"

int sqlite_logentry_printfield(FILE *fp, LogEntry *entry, LogField *f, char *fmt);

void sqlite_logsys_dumpheader(LogSystem *log)
{
	LogField *field;

	for (field = log->label->fields; field->type; ++field)
    {
		putchar(' ');
		printf("%s", field->header.string);
	}
	printf("\n");
}

void sqlite_logentry_dump(LogEntry *entry)
{
	LogSystem *log = entry->logsys;
	LogField *field;

	for (field = log->label->fields; field->type; ++field)
    {
		putchar(' ');
		sqlite_logentry_printfield(stdout, entry, field, NULL);
	}
	printf("\n");
}

void sqlite_logsys_dumplabel(LogLabel *label)
{
	printf("magic              0x%X\n", label->magic);
	printf("version            %d\n", label->version);
	printf("maxcount           %d\n", label->maxcount);
	printf("recordsize         %d\n", label->recordsize);
	printf("nfields            %d\n", label->nfields);
	printf("preferences        0x%x\n", label->preferences);
	printf("run addprog        %s\n", label->run_addprog ?  "true" : "false");
	printf("run removeprog     %s\n", label->run_removeprog ?  "true" : "false");
	printf("run thresholdprog  %s\n", label->run_thresholdprog ?  "true" : "false");
	printf("run cleanupprog    %s\n", label->run_cleanupprog ?  "true" : "false");
	printf("cleanup_count      %d\n", label->cleanup_count);
	printf("cleanup_threshold  %d\n", label->cleanup_threshold);
	printf("diskusage max      %d kB\n", label->max_diskusage);
	printf("datafile max       %d kB\n", label->recordsize * label->maxcount / 1024);
}

void sqlite_logsys_dumplabelfields(LogLabel *label)
{
	LogField *f;
	int total_used = 0;
	int i;

	for (f = label->fields; f->type; ++f)
    {
		printf("%-13s %-7s %3d %3d ", f->name.string, sqlite_logfield_typename(f->type), f->offset, f->size);
		i = printf("%s", f->format.string);
		while (i++ < 16)
        {
			putchar(' ');
        }
		i = printf("'%s'", f->header.string);
		putchar('\n');
		total_used += f->size;
	}
	printf("%d used, %d free\n", total_used, label->recordsize - total_used);
}

