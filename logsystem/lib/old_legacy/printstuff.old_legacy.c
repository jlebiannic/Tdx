#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		printstuff.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Printing logentries.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: printstuff.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 18.02.97/MW	Printformat and pad value for pseudo field user
			(in master mode) encreased from 8 to 16
  4.02 05.07.02/CD      Added column separator when printing by format
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_prototypes.h"

char *
old_legacy_logfield_typename(type)
	int type;
{
	switch (type) {
	case FIELD_NONE:
		return ("NONE");
	case FIELD_INDEX:
		return ("INDEX");
	case FIELD_TIME:
		return ("TIME");
	case FIELD_TEXT:
		return ("TEXT");
	case FIELD_NUMBER:
		return ("NUMBER");
	case FIELD_INTEGER:
		return ("INTEGER");

	default: {
		static char buffer[64];
		sprintf(buffer, "TYPE%d", type);
		return (buffer);
		}
	}
}

int
old_legacy_logentry_printfield(fp, entry, f, fmt)
	FILE *fp;
	LogEntry *entry;
	LogField *f;
	char *fmt;
{
	void *value = &entry->record_buffer[f->offset];

	if (!fmt)
		fmt = f->format.string;

	switch (f->type) {
	default:
		return (tr_mbFprintf(fp, "TYPE%d", f->type));
	case FIELD_INDEX:
        return (tr_mbFprintf(fp, fmt, (Number) *(LogIndex *) value + 0.2));
	case FIELD_NUMBER:
		return (tr_mbFprintf(fp, fmt, *(Number *) value));
	case FIELD_TEXT:
		return (tr_mbFprintf(fp, fmt, (char *) value));
	case FIELD_TIME:
		return (tr_mbFprintf(fp, "%s", tr_timestring(fmt, *(TimeStamp *) value)));
	case FIELD_INTEGER:
		return (tr_mbFprintf(fp, fmt, *(Integer *) value));
	}
}

int
old_legacy_logfield_printheader(fp, f)
	FILE *fp;
	LogField *f;
{
	return (tr_mbFprintf(fp, "%s", f->header.string));
#if 0
	/*
	 * Append space to align headers with
	 * real lines.
	 */
	int fieldwidth;
	char *cp;

	switch (f->type) {
	default:
	case FIELD_INDEX:
	case FIELD_NUMBER:
	case FIELD_INTEGER:
	case FIELD_TEXT:
		cp = f->format.string;
		while (*cp && (*cp < '0' || *cp > '9'))
			++cp;
		fieldwidth = atoi(cp);
		break;
	case FIELD_TIME:
		cp = tr_timestring(f->format.string, 0);
		fieldwidth = strlen(cp);
		break;
	}
	while (cc++ < fieldwidth)
		putc(' ', fp);
#endif
}

void old_legacy_logheader_printbyformat(FILE *fp, PrintFormat *pf)
{
	FormatRecord *r;
	int cc, n = 0;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		if (n++)
		   /* 4.02/CD */
		   tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
		cc = 0;
		if (r->field)
			cc = old_legacy_logfield_printheader(fp, r->field);
		while (cc++ < r->width)
			tr_mbFputs(" ", fp);
	}
	tr_mbFputs("\n", fp);
}

int
old_legacy_logentry_printbyformat(fp, entry, pf)
	FILE *fp;
	LogEntry *entry;
	PrintFormat *pf;
{
	FormatRecord *r;
	int cc, n = 0;

	if (pf->logsys_compiled_to != entry->logsys)
		old_legacy_logsys_compileprintform(entry->logsys, pf);

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		if (!pf->is_namevalue && n++)
		   tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
		cc = 0;
		if (r->field)
			cc = old_legacy_logentry_printfield(fp, entry, r->field, r->format);
		while (!pf->is_namevalue && cc++ < r->width)
			tr_mbFputs(" ", fp);
	}
	if (!pf->is_namevalue)
		tr_mbFputs("\n", fp);
}

int
old_legacy_logsys_printheaderbynames(fp, log, names)
	FILE *fp;
	LogSystem *log;
	char **names;
{
	LogField *fields[256];
	int i, n;

	for (n = 0; n < SIZEOF(fields) && names[n]; ++n)
		fields[n] = old_legacy_logsys_getfield(log, names[n]);

	for (i = 0; i < n; ++i) {
		if (i)
			tr_mbFputs(" ", fp);
		if (fields[i]) {
			old_legacy_logfield_printheader(fp, fields[i]);
		}
	}
	tr_mbFputs("\n", fp);
}

int
old_legacy_logentry_printbynames(fp, entry, names)
	FILE *fp;
	LogEntry *entry;
	char **names;
{
	LogField *fields[256];
	int i, n;

	for (n = 0; n < SIZEOF(fields) && names[n]; ++n)
		fields[n] = old_legacy_logsys_getfield(entry->logsys, names[n]);

	for (i = 0; i < n; ++i) {
		if (i)
			tr_mbFputs(" ", fp);
		if (fields[i]) {
			old_legacy_logentry_printfield(fp, entry, fields[i], NULL);
		}
	}
	tr_mbFputs("\n", fp);
}

static FilterRecord default_filter_records[] = {
	{ "INDEX",	OP_NONE, },
	{ "CREATED",	OP_NONE, },
	{ "MODIFIED",	OP_NONE, },
};

static LogFilter default_filter = {
	default_filter_records,
	SIZEOF(default_filter_records),
	0,
	NULL,
};

static FormatRecord default_format_records[] = {
	{ "INDEX",	},
	{ "CREATED",	},
	{ "MODIFIED",	},
};

static PrintFormat default_format = {
	default_format_records,
	SIZEOF(default_format_records),
	0,
	256,
	NULL,
};

static int
old_legacy_logheader_addfield(hp, sizep, offset, f)
	char **hp;
	int *sizep;
	int offset;
	LogField *f;
{
	int pad = 0, len = strlen(f->header.string);
	char *cp;

	switch (f->type) {
	default:
	case FIELD_INDEX:
	case FIELD_NUMBER:
	case FIELD_INTEGER:
	case FIELD_TEXT:
		cp = f->format.string;
		while (*cp && (*cp < '0' || *cp > '9'))
			++cp;
		pad = atoi(cp);
		break;
	case FIELD_TIME:
		cp = tr_timestring(f->format.string, 0);
		pad = strlen(cp);
		break;
	case PSEUDOFIELD_OWNER:
		/* 3.01 8 -> 16 */
		pad = 16;
		break;
	}
	pad -= len;
	if (pad < 0)
		pad = 0;
	/*
	 * Append space to align headers with
	 * real lines.
	 */
	pad += 1;

	while (offset + len + pad + 1 >= *sizep) {
		*sizep += 64;
		*hp = old_legacy_log_realloc(*hp, *sizep);
	}
	cp = *hp + offset;

	memcpy(cp, f->header.string, len);
	memset(cp + len, ' ', pad);
	cp[len + pad] = 0;

	return (offset + len + pad);
}

/*
 * Requires a call to compilefilter !
 */
char *
old_legacy_logsys_buildheader(ls, pf)
	LogSystem *ls;
	PrintFormat *pf;
{
	LogField *field;
	FormatRecord *f;
	char *header;
	int offset;

	if (!pf || pf->format_count <= 0)
		pf = &default_format;

	if (pf->logsys_compiled_to != ls)
		old_legacy_logsys_compileprintform(ls, pf);

	header = old_legacy_log_malloc(pf->estimated_linelen);
	offset = 0;

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f) {
		if (!f->field)
			continue;

		offset = old_legacy_logheader_addfield(&header, &pf->estimated_linelen,
			offset, f->field);
	}
	return (header);
}

static int
old_legacy_logline_addfield(lp, sizep, offset, entry, f)
	char **lp;
	int *sizep;
	int offset;
	LogEntry *entry;
	LogField *f;
{
	void *value = &entry->record_buffer[f->offset];
	char *fmt = f->format.string;
	char *pos = *lp + offset;

	switch (f->type) {
	default:
		sprintf(pos, "TYPE%d", f->type);
		break;
	case FIELD_INDEX:
        sprintf(pos, fmt, (Number) *(LogIndex *) value + 0.2);
		break;
	case FIELD_NUMBER:
		sprintf(pos, fmt, *(Number *) value);
		break;
	case FIELD_TEXT:
		sprintf(pos, fmt, (char *) value);
		break;
	case FIELD_TIME:
		strcpy(pos, tr_timestring(fmt, *(TimeStamp *) value));
		break;
	case FIELD_INTEGER:
		sprintf(pos, fmt, *(Integer *) value);
		break;
	case PSEUDOFIELD_OWNER:
		/* 3.01 8 -> 16 */
		sprintf(pos, "%-16.16s", entry->logsys->owner);
		break;
	}
	pos += strlen(pos);
	*pos++ = ' ';
	*pos = 0;

	return (pos - *lp);
}

/*
 * Return a string representation of the entry,
 * fields from filter.
 * Requires a call to compilefilter !
 */
char *
old_legacy_logentry_buildline(entry, pf)
	LogEntry *entry;
	PrintFormat *pf;
{
	FormatRecord *f;
	char *line;
	int offset;
	LogSystem *ls = entry->logsys;

	if (!pf || pf->format_count <= 0)
		pf = &default_format;

	if (pf->logsys_compiled_to != ls)
		old_legacy_logsys_compileprintform(ls, pf);

	line = old_legacy_log_malloc(pf->estimated_linelen + 1);
	offset = 0;

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f) {
		if (!f->field)
			continue;

		offset = old_legacy_logline_addfield(&line, &pf->estimated_linelen,
			offset, entry, f->field);
	}
	line[offset] = 0;
	line = old_legacy_log_realloc(line, offset + 2);

	return (line);
}

