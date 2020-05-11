#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		printstuff.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Printing logentries.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: printstuff.c 55487 2020-05-06 08:56:27Z jlebiannic $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 18.02.97/MW	Printformat and pad value for pseudo field user
			(in master mode) encreased from 8 to 16
  4.02 05.07.02/CD      Added column separator when printing by format
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "logsystem.h"
#include "logsystem.dao.h"
#include "runtime/tr_prototypes.h"

char * dao_logfield_typename(int type)
{
	switch (type)
    {
        case FIELD_NONE:    return ("NONE");
        case FIELD_INDEX:   return ("INDEX");
        case FIELD_TIME:    return ("TIME");
        case FIELD_TEXT:    return ("TEXT");
        case FIELD_NUMBER:  return ("NUMBER");
        case FIELD_INTEGER: return ("INTEGER");

        default:
        {
            static char buffer[64];
            sprintf(buffer, "TYPE%d", type);
            return (buffer);
        }
	}
}

int dao_logentry_printfield(FILE *fp, LogEntry *entry, LogField *f, char *fmt)
{
	void *value = &entry->record_buffer[f->offset];

	if (!fmt) {
		fmt = f->format.string;
	}

	switch (f->type)
    {
        default:            return (tr_mbFprintf(fp, "TYPE%d", f->type));
        case FIELD_INDEX:   return (tr_mbFprintf(fp, fmt, *(LogIndex *) value));
        case FIELD_NUMBER:  return (tr_mbFprintf(fp, fmt, *(Number *) value));
        case FIELD_TEXT:    {
				char *testFmt = (char*) dao_log_malloc( 64 * sizeof(char) );
				sprintf( testFmt, "%%-%d.%ds", f->size - 1, f->size - 1 );
				if ( strcmp( fmt, testFmt ) == 0 ) {
					/* format is used for right padding, but fprintf does not work right with UTF-8 strings */
					/* do the padding by hand */
					free( testFmt );
					char *tmp = (char *) dao_log_malloc( f->size * sizeof(char) * 4 ); /* allow for 4 bytes unicode chars */
					mbsncpypad( tmp, (char *)value, f->size - 1, ' ' );
					int res = tr_mbFprintf(fp, "%s", tmp);
					free( tmp );
					return res;
				}
				/* else use format as usual */
				free( testFmt );
				return tr_mbFprintf(fp, fmt, (char*) value);
		}
        case FIELD_TIME:    return tr_mbFprintf(fp, "%s", tr_timestring(fmt, *(TimeStamp *) value));
        case FIELD_INTEGER: return (tr_mbFprintf(fp, fmt, *(Integer *) value));
	}
}

int dao_logfield_printheader(FILE *fp, LogField *f)
{
	return (tr_mbFprintf(fp, "%s", f->header.string));
}

void dao_logheader_printbyformat_env(FILE *fp, PrintFormat *pf)
{
	tr_mbFprintf(fp, "Env");
	tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
	dao_logheader_printbyformat(fp, pf);
}

void dao_logheader_printbyformat(FILE *fp, PrintFormat *pf)
{
	FormatRecord *r;
	int cc, n = 0;

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (n++) {
		   /* 4.02/CD */
		   tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
		}
		cc = 0;
		if (r->field) {
			cc = dao_logfield_printheader(fp, r->field);
		}
		while (cc++ < r->width) {
			tr_mbFputs(" ", fp);
		}
	}
	tr_mbFputs("\n", fp);
}

void dao_logentry_printbyformat(FILE *fp, LogEntry *entry, PrintFormat *pf)
{
	FormatRecord *r;
	int cc, n = 0;

	if (pf->logsys_compiled_to != entry->logsys) {
		dao_logsys_compileprintform(entry->logsys, pf);
	}

	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (!pf->is_namevalue && n++) {
		   tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
		}
		cc = 0;
		if (r->field) {
			cc = dao_logentry_printfield(fp, entry, r->field, r->format);
		}
		while (!pf->is_namevalue && cc++ < r->width) {
			tr_mbFputs(" ", fp);
		}
	}
	if (!pf->is_namevalue) {
		tr_mbFputs("\n", fp);
	}
}

int dao_logsys_printheaderbynames(FILE *fp, LogSystem *log, char **names)
{
	LogField *fields[256];
	unsigned int i, n;

	for (n = 0; n < SIZEOF(fields) && names[n]; ++n)
		fields[n] = dao_logsys_getfield(log, names[n]);

	for (i = 0; i < n; ++i)
    {
		if (i) {
			tr_mbFputs(" ", fp);
		}
		if (fields[i]) {
			dao_logfield_printheader(fp, fields[i]);
		}
	}
	tr_mbFputs("\n", fp);
    return 0;
}

int dao_logentry_printbynames(FILE *fp, LogEntry *entry, char **names)
{
	LogField *fields[256];
	unsigned int i, n;

	for (n = 0; n < SIZEOF(fields) && names[n]; ++n)
		fields[n] = dao_logsys_getfield(entry->logsys, names[n]);

	for (i = 0; i < n; ++i)
    {
		if (i) {
			tr_mbFputs(" ", fp);
		}
		if (fields[i]) {
			dao_logentry_printfield(fp, entry, fields[i], NULL);
		}
	}
	tr_mbFputs("\n", fp);
    return 0;
}

static FormatRecord default_format_records[] = {
	{ "INDEX", "", NULL, 0},
	{ "CREATED", "", NULL, 0},
	{ "MODIFIED", "", NULL, 0},
};

static PrintFormat default_format = { default_format_records, SIZEOF(default_format_records), 0, 256, NULL, 0, ""};

static int dao_logheader_addfield(char **hp, int *sizep, int offset, LogField *f)
{
	int pad = 0, len = strlen(f->header.string);
	char *cp;

	switch (f->type)
    {
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
	if (pad < 0) {
		pad = 0;
	}
	/* Append space to align headers with real lines. */
	pad += 1;

	while (offset + len + pad + 1 >= *sizep)
    {
		*sizep += 64;
		*hp = dao_log_realloc(*hp, *sizep);
	}
	cp = *hp + offset;

	memcpy(cp, f->header.string, len);
	memset(cp + len, ' ', pad);
	cp[len + pad] = 0;

	return (offset + len + pad);
}

/* Requires a call to compilefilter ! */
char * dao_logsys_buildheader(LogSystem *ls, PrintFormat *pf)
{
	FormatRecord *f;
	char *header;
	int offset;

	if (!pf || pf->format_count <= 0) {
		pf = &default_format;
	}

	if (pf->logsys_compiled_to != ls) {
		dao_logsys_compileprintform(ls, pf);
	}

	header = dao_log_malloc(pf->estimated_linelen);
	offset = 0;

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f)
    {
		if (!f->field) {
			continue;
		}
		offset = dao_logheader_addfield(&header, &pf->estimated_linelen, offset, f->field);
	}
	return (header);
}

static int dao_logline_addfield(char **lp, int offset, LogEntry *entry, LogField *f)
{
	void *value = &entry->record_buffer[f->offset];
	char *fmt = f->format.string;
	char *pos = *lp + offset;

	switch (f->type)
    {
        default:                sprintf(pos, "TYPE%d", f->type);                       break;
        case FIELD_INDEX:       sprintf(pos, fmt, *(LogIndex *) value);                break;
        case FIELD_NUMBER:      sprintf(pos, fmt, *(Number *) value);                  break;
        case FIELD_TEXT:        sprintf(pos, fmt, (char *) value);                     break;
        case FIELD_TIME:        strcpy(pos, tr_timestring(fmt, *(TimeStamp *) value)); break;
        case FIELD_INTEGER:     sprintf(pos, fmt, *(Integer *) value);                 break;
        case PSEUDOFIELD_OWNER: sprintf(pos, "%-16.16s", entry->logsys->owner);        break;
	}
	pos += strlen(pos);
	*pos++ = ' ';
	*pos = 0;

	return (pos - *lp);
}

/* Return a string representation of the entry,
 * fields from filter.
 * Requires a call to compilefilter ! */
char * dao_logentry_buildline(LogEntry *entry, PrintFormat *pf)
{
	FormatRecord *f;
	char *line;
	int offset;
	LogSystem *ls = entry->logsys;

	if (!pf || pf->format_count <= 0) {
		pf = &default_format;
	}

	if (pf->logsys_compiled_to != ls) {
		dao_logsys_compileprintform(ls, pf);
	}

	line = dao_log_malloc(pf->estimated_linelen + 1);
	offset = 0;

	for (f = pf->formats; f < &pf->formats[pf->format_count]; ++f)
    {
		if (!f->field) {
			continue;
		}
		offset = dao_logline_addfield(&line, offset, entry, f->field);
	}
	line[offset] = 0;
	line = dao_log_realloc(line, offset + 2);

	return (line);
}

/*JRE 06.15 BG-81*/
int dao_logentry_printfield_env(FILE *fp, LogEntryEnv *entry, LogField *f, char *fmt)
{
	int retour;
	
	void *value = &entry->record_buffer[f->offset];

	if (!fmt) {
		fmt = f->format.string;
	}

	switch (f->type)
    {
        default:            retour = (tr_mbFprintf(fp, "TYPE%d", f->type)); break;
        case FIELD_INDEX:   retour = (tr_mbFprintf(fp, fmt, *(LogIndex *) value)); break;
        case FIELD_NUMBER:  retour = (tr_mbFprintf(fp, fmt, *(Number *) value)); break;
        case FIELD_TEXT:    retour = (tr_mbFprintf(fp, fmt, (char *) value)); break;
        case FIELD_TIME:    retour = (tr_mbFprintf(fp, "%s", tr_timestring(fmt, *(TimeStamp *) value))); break;
        case FIELD_INTEGER: retour = (tr_mbFprintf(fp, fmt, *(Integer *) value)); break;
	}
	
	return retour;
}

void dao_logentry_printbyform_env(FILE *fp, LogEntryEnv *entry, PrintFormat *pf)
{
	FormatRecord *r;
	int cc, n = 0;
	char * env;

	if (pf->logsys_compiled_to != entry->logsys) {
		dao_logsys_compileprintform(entry->logsys, pf);
	}

	env = strtok(entry->idx,".");
	tr_mbFprintf(fp,"%s",env);
	if (!pf->is_namevalue && n++) {
		tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
	}
	
	for (r = pf->formats; r < &pf->formats[pf->format_count]; ++r)
    {
		if (!pf->is_namevalue && n++) {
		   tr_mbFprintf(fp,"%s",(pf->separator?pf->separator:" "));
		}
		cc = 0;
		if (r->field) {
			cc = dao_logentry_printfield_env(fp, entry, r->field, r->format);
		}
		while (!pf->is_namevalue && cc++ < r->width)
			tr_mbFputs(" ", fp);
	}
	if (!pf->is_namevalue) {
		tr_mbFputs("\n", fp);
	}
}
/*End JRE*/

