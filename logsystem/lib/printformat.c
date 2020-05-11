/*========================================================================
        E D I S E R V E R

        File:		printformat.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: printformat.c 55487 2020-05-06 08:56:27Z jlebiannic $")
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

#include "logsystem.dao.h"
#include "runtime/tr_prototypes.h"

static void dao_topieces(char *namevalue, char **name, char **format)
{
	char *cp = namevalue;

	while (*cp && isspace(*cp))
        ++cp;	/* to 1st word */
	*name = cp;
	*format = NULL;
	while (*cp && *cp != '=')
        ++cp;		/* to 1st = */
	if (*cp)
    {
		*cp++ = 0;			/* cut = */
		*format = cp;			/* after = */
	}
}

/* 12.10.98/KP Was static */
PrintFormat * dao_alloc_printformat()
{
	PrintFormat *pf;

	pf = dao_log_malloc(sizeof(*pf));
	memset(pf, 0, sizeof(*pf));

	/* 3.10/CD Column separator */
	pf->separator = NULL;

	return (pf);
}

void dao_printformat_insert(PrintFormat **pfp, char *name, char *format)
{
	int n;
	FormatRecord *r;
	PrintFormat *pf;

	if (!*pfp)
		*pfp = dao_alloc_printformat();
	pf = *pfp;

	if (pf->format_count >= pf->format_max)
    {
		pf->format_max += 64;
		n = pf->format_max * sizeof(*pf->formats);
		pf->formats = dao_log_realloc(pf->formats, n);
	}
	r = &pf->formats[pf->format_count++];

	r->name = dao_log_strdup(name);
	r->format = format ? dao_log_strdup(format) : NULL;
	r->width = -1;
	r->field = NULL;
}

void dao_printformat_add(PrintFormat **pfp, char *namevalue)
{
	char *name, *format;

	dao_topieces(namevalue, &name, &format);
	dao_printformat_insert(pfp, name, format);
}

/* Format of input file:
 * [p]FIELDNAME=VALUE */
int dao_printformat_read(PrintFormat **pfp, FILE *fp)
{
	char *cp, buf[1024];

	while (fp && tr_mbFgets(buf, sizeof(buf), fp))
    {
		if (*buf == '\n' || *buf == '#')
			continue;
		if ((cp = strchr(buf, '\n')) != NULL)
			*cp = 0;
		dao_printformat_add(pfp, buf);
	}
	return (0);
}

int dao_printformat_write(PrintFormat *pf, FILE *fp)
{
	FormatRecord *r;

	if (!pf)
		return (0);

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (tr_mbFprintf(fp, "%s=%s\n", r->name, r->format) < 0)
        {
			return (-1);
        }
    }
	return (0);
}

void dao_printformat_clear(PrintFormat *pf)
{
	FormatRecord *r;

	if (!pf)
		return;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (r->name)
			free(r->name);
		if (r->format)
			free(r->format);
	}
	pf->format_count = 0;
	pf->estimated_linelen = 0;
}

void dao_printformat_free(PrintFormat *pf)
{
	FormatRecord *r;

	if (!pf)
		return;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (r->name)
			free(r->name);
		if (r->format)
			free(r->format);
	}
	if (pf->formats)
		free(pf->formats);

	free(pf);
}

/* Compile the fields in the printformat to point
 * to corresponding fields in the logsystem.
 * Unknown fields are ignored.
 * Returns total width of output. */
int dao_logsys_compileprintform(LogSystem *ls, PrintFormat *pf)
{
	int i, width = 0;
	LogField *field;
	FormatRecord *r;
	char *cp;

	if (!pf)
		return (0);

	pf->logsys_compiled_to = ls;

	pf->estimated_linelen = 8;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		r->width = 0;
		r->field = NULL;

		field = dao_logsys_getfield(ls, r->name);
		if (!field)
			continue;

		r->field = field;
		cp = r->format ? r->format : field->format.string;

		switch (field->type)
        {
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
			while (*cp && !isdigit(*cp))
                ++cp;
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

int dao_logfield_printformat_offset(LogField *f, PrintFormat *pf)
{
	FormatRecord *r;
	int off = 0;

	if (!pf)
		return (-1);

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (r->field)
        {
			if (r->field == f)
				return (off);
			off += r->width + 1; /* blank separating the fields */
		}
	}
	return (-1);
}

PrintFormat * dao_logsys_defaultformat(LogSystem *log)
{
	PrintFormat *pf;
	LogField *field;

	pf = dao_alloc_printformat();

	for (field = log->label->fields; field->type; ++field)
    {
		dao_printformat_insert(&pf, field->name.string, field->format.string);
    }

	return (pf);
}

PrintFormat * dao_logsys_namevalueformat(LogSystem *log)
{
	PrintFormat *pf;
	LogField *field;
	int len, maxlen = 64;
	char *buffer = dao_log_malloc(maxlen);
	char *namep;

	pf = dao_alloc_printformat();

	pf->is_namevalue = 1;

	for (field = log->label->fields; field->type; ++field)
    {
		len = strlen(field->name.string) + 1 + strlen(field->format.string) + 1 + 1;
		if (len > maxlen)
        {
			maxlen = len;
			free(buffer);
			buffer = dao_log_malloc(maxlen);
		}
		strcpy(buffer, field->name.string);
		namep = buffer + strlen(buffer);
		*namep++ = '=';

		switch (field->type)
        {
            case PSEUDOFIELD_OWNER: continue;
            case FIELD_TEXT:    strcpy(namep, "%s\n");                break;
            case FIELD_TIME:    strcpy(namep, "%d.%m.%Y %H:%M:%S\n"); break;
            case FIELD_INDEX:
            case FIELD_INTEGER: strcpy(namep, "%d\n");                break;
            case FIELD_NUMBER:  strcpy(namep, "%lf\n");               break;
		}
		dao_printformat_insert(&pf, field->name.string, buffer);
	}
	free(buffer);
	return (pf);
}

int dao_printformat_printables(PrintFormat *pf, char *name)
{
	FormatRecord *f;

	if (!pf)
		return (0);

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f)
    {
		if (!strcmp(name, f->name))
        {
			return (1);
        }
    }

	return (0);
}

