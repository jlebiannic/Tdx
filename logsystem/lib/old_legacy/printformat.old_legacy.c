/*========================================================================
        E D I S E R V E R

        File:		printformat.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: printformat.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 03.05.95/JN	Master mode changes.
  3.02 15.01.96/JN	Off by one with linelen caused pictures of Erkki
			to appear at end of log2.
  3.03 26.02.96/JN	X=Y format for times now %d.%m.%Y
  3.04 26.07.96/JN	Wrong key-offset with printformat.
  3.05 01.08.96/JN	GENERATION not included in namevalue-format.
  3.06 26.08.96/JN	Generalization to printformat_type_offset()
  3.07 19.02.97/JN	OWNER width 16. Account for padding in keyfield-offset.
  3.08 07.03.97/JN	Off-by-1 with field->size when determining sort offset.
  3.09 26.09.97/JN	Deterministic initial values for all Format members.
  3.10 05.07.02/CD	Column separator for printing.
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <ctype.h>

#define FILTER_STRINGS

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "runtime/tr_prototypes.h"

static void
old_legacy_topieces(namevalue, name, format)
	char *namevalue;
	char **name, **format;
{
	char *cp = namevalue;

	while (*cp && isspace(*cp)) ++cp;	/* to 1st word */

	*name = cp;
	*format = NULL;

	while (*cp && *cp != '=') ++cp;		/* to 1st = */
	if (*cp) {
		*cp++ = 0;			/* cut = */
		*format = cp;			/* after = */
	}
}

/*
 * 12.10.98/KP Was static
 */
PrintFormat *
old_legacy_alloc_printformat()
{
	PrintFormat *pf;

	pf = old_legacy_log_malloc(sizeof(*pf));
	memset(pf, 0, sizeof(*pf));

	/* 3.10/CD Column separator */
	pf->separator = NULL;

	return (pf);
}

void
old_legacy_printformat_insert(pfp, name, format)
	PrintFormat **pfp;
	char *name, *format;
{
	int n;
	FormatRecord *r;
	PrintFormat *pf;

	if (!*pfp)
		*pfp = old_legacy_alloc_printformat();
	pf = *pfp;

	if (pf->format_count >= pf->format_max) {
		pf->format_max += 64;
		n = pf->format_max * sizeof(*pf->formats);

		pf->formats = old_legacy_log_realloc(pf->formats, n);
	}
	r = &pf->formats[pf->format_count++];

	r->name = old_legacy_log_strdup(name);
	r->format = format ? old_legacy_log_strdup(format) : NULL;
	r->width = -1;
	r->field = NULL;
}

void
old_legacy_printformat_add(pfp, namevalue)
	PrintFormat **pfp;
	char *namevalue;
{
	char *name, *format;

	old_legacy_topieces(namevalue, &name, &format);

	old_legacy_printformat_insert(pfp, name, format);
}

/*
 * Format of input file:
 * [p]FIELDNAME=VALUE
 */
int
old_legacy_printformat_read(pfp, fp)
	PrintFormat **pfp;
	FILE *fp;
{
	char *cp, buf[1024];

	while (fp && fgets(buf, sizeof(buf), fp)) {
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;

		old_legacy_printformat_add(pfp, buf);
	}
	return (0);
}

old_legacy_printformat_write(pf, fp)
	PrintFormat *pf;
	FILE *fp;
{
	FormatRecord *r;

	if (!pf)
		return (0);

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {

		if (tr_mbFprintf(fp, "%s=%s\n", r->name, r->format) < 0)
			return (-1);
	}
	return (0);
}

void
old_legacy_printformat_clear(pf)
	PrintFormat *pf;
{
	FormatRecord *r;

	if (!pf)
		return;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		if (r->name)
			free(r->name);
		if (r->format)
			free(r->format);
	}
	pf->format_count = 0;
	pf->estimated_linelen = 0;
}

void
old_legacy_printformat_free(pf)
	PrintFormat *pf;
{
	FormatRecord *r;

	if (!pf)
		return;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		if (r->name)
			free(r->name);
		if (r->format)
			free(r->format);
	}
	if (pf->formats)
		free(pf->formats);

	free(pf);
}

/*
 * Compile the fields in the printformat to point
 * to corresponding fields in the logsystem.
 * Unknown fields are ignored.
 * Returns total width of output.
 */
int
old_legacy_logsys_compileprintform(ls, pf)
	LogSystem *ls;
	PrintFormat *pf;
{
	int i, width = 0;
	LogField *field;
	FormatRecord *r;
	char *cp;

	if (!pf)
		return (0);

	pf->logsys_compiled_to = ls;

	pf->estimated_linelen = 8;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		r->width = 0;
		r->field = NULL;

		field = old_legacy_logsys_getfield(ls, r->name);
		if (!field) {
			continue;
		}
		r->field = field;
		cp = r->format ? r->format : field->format.string;

		switch (field->type) {
		case FIELD_TEXT:
			r->width = field->size - 1; /* size counts the \0 */
			while (*cp && !isdigit(*cp)) ++cp;
			if (*cp)
				r->width = atoi(cp);
			break;
		case FIELD_TIME:
			cp = tr_timestring(cp, (TimeStamp) 0);
			r->width = strlen(cp);
			break;
		case FIELD_INDEX:
		case FIELD_INTEGER:
		case FIELD_NUMBER:
			r->width = 8;
			while (*cp && !isdigit(*cp)) ++cp;
			if (*cp)
				r->width = atoi(cp);
			break;
		case PSEUDOFIELD_OWNER:
			r->width = 16;
			break;
		}
		i = strlen(field->header.string);
		if (r->width < i)
			r->width = i;
		width += r->width + 2;

		pf->estimated_linelen += width;
	}
	return (width);
}

int
old_legacy_logfield_printformat_offset(f, pf)
	LogField *f;
	PrintFormat *pf;
{
	FormatRecord *r;
	int off = 0;

	if (!pf) {
		return (-1);
	}
	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r) {
		if (r->field) {
			if (r->field == f)
				return (off);
			off += r->width + 1; /* blank separating the fields */
		}
	}
	return (-1);
}

PrintFormat *
old_legacy_logsys_defaultformat(log, include_system)
	LogSystem *log;
	int include_system;	/* XXX */
{
	PrintFormat *pf;
	LogField *field;

	pf = old_legacy_alloc_printformat();

	for (field = log->label->fields; field->type; ++field) {
		old_legacy_printformat_insert(&pf,
			field->name.string, field->format.string);
	}
	return (pf);
}

PrintFormat *
old_legacy_logsys_namevalueformat(log, include_system)
	LogSystem *log;
	int include_system;	/* XXX */
{
	PrintFormat *pf;
	LogField *field;
	int len, maxlen = 64;
	char *buffer = old_legacy_log_malloc(maxlen);
	char *namep;

	pf = old_legacy_alloc_printformat();

	pf->is_namevalue = 1;

	for (field = log->label->fields; field->type; ++field) {

		len = strlen(field->name.string) + 1
		    + strlen(field->format.string) + 1 + 1;
		if (len > maxlen) {
			maxlen = len;
			free(buffer);
			buffer = old_legacy_log_malloc(maxlen);
		}
#if 0
		namep = field->name.string;
		while (*namep && *namep == ' ')
			namep++;
		strcpy(buffer, namep);
		namep = buffer;
		namep += strlen(buffer);
		while (namep > buffer && namep[-1] == ' ')
			namep--;
		*namep++ = '=';
		strcpy(namep, field->format.string);
		namep += strlen(namep);
		*namep++ = '\n';
		*namep = 0;

		printformat_insert(&pf,
			field->name.string, buffer);
#else
		strcpy(buffer, field->name.string);
		namep = buffer + strlen(buffer);
		*namep++ = '=';

        if (include_system == 0)
        {
            switch (field->type) {

            case FIELD_INDEX:
            case PSEUDOFIELD_OWNER:
                continue;

            case FIELD_TEXT:
                strcpy(namep, "%s\n");
                break;
            case FIELD_TIME:
                strcpy(namep, "%d.%m.%Y %H:%M:%S\n");
                break;
            case FIELD_INTEGER:
                /*
                 *  Like indexes, generation is not included
                 *  in default namevalue-formats.
                 */
                if (field->offset ==
                    offsetof(DiskLogRecord, header.generation))
                    continue;
                strcpy(namep, "%d\n");
                break;
            case FIELD_NUMBER:
                strcpy(namep, "%lf\n");
                break;
            }
        }
        else /* we want them all (needed for migration to sqlite */
        {
            switch (field->type)
            {
                case PSEUDOFIELD_OWNER: continue;
                case FIELD_TEXT:    strcpy(namep, "%s\n");                break;
                case FIELD_TIME:    strcpy(namep, "%d.%m.%Y %H:%M:%S\n"); break;
                case FIELD_INTEGER: strcpy(namep, "%d\n");                break;
                case FIELD_INDEX:   strcpy(namep, "%.0lf\n");             break;
                case FIELD_NUMBER:  strcpy(namep, "%lf\n");               break;
            }
        }

		old_legacy_printformat_insert(&pf,
			field->name.string, buffer);
#endif
	}
	free(buffer);
	return (pf);
}

int
old_legacy_printformat_printables(pf, name)
	PrintFormat *pf;
	char *name;
{
	FormatRecord *f;

	if (!pf)
		return (0);

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f)
		if (!strcmp(name, f->name)) {
			return (1);
		}
	return (0);
}

