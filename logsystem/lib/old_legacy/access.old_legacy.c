/*========================================================================
        E D I S E R V E R

        File:		logsystem/access.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: access.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 05.04.95/JN	hi-pointer findfield was one past upper limit.
  3.02 19.02.96/JN	tr_parsetime now used for setbyname().
  3.03 18.02.97/MW	format for pseudofield owner changed from 8 to 16
			characters
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "logsystem.h"

extern double atof();

static LogField *old_legacy_findfield PROTO((LogLabel *label, char *name, int type));
void old_legacy_logentry_setbyfield   PROTO((LogEntry *entry, LogField *f, char *s));

static LogField *
old_legacy_findfield(label, name, type)
	LogLabel *label;
	char *name;
	int type;
{
	LogField *lo, *hi;
	LogField *field;
	int cmp;

	lo = label->fields + label->types[type].start;
	hi = label->fields + label->types[type].end - 1;

	while (lo <= hi) {
		field = lo + (hi - lo) / 2;

		cmp = strcmp(field->name.string, name);

		if (cmp > 0) {
			hi = field - 1;
			continue;
		}
		if (cmp < 0) {
			lo = field + 1;
			continue;
		}
		return (field);
	}
	return (NULL);
}

LogField *
old_legacy_loglabel_getfield(label, name)
	LogLabel *label;
	char *name;
{
	int type;
	LogField *f;

	for (type = 1; type < FIELD_NTYPES; ++type)
		if ((f = old_legacy_findfield(label, name, type)) != NULL)
			return (f);
	return (NULL);
}

LogField *
old_legacy_logsys_getfield(log, name)
	LogSystem *log;
	char *name;
{
	LogField *f;
	
	f = old_legacy_loglabel_getfield(log->label, name);
	if (f)
		return (f);

	/*
	 * Not a real field.
	 * Any of the "pseudo" fields ?
	 */
	if (!strcmp(name, "_OWNER")) {
		static LogField owner;

		owner.type = PSEUDOFIELD_OWNER;
		owner.header.string = "User";
		/* 3.03 8 -> 16 */
		owner.format.string = "%-16.16s";

		return (&owner);
	}
	return (NULL);
}

void *
old_legacy_logentry_getvalue(entry, field)
	LogEntry *entry;
	LogField *field;
{
	return (&entry->record->buffer[field->offset]);
}

void
old_legacy_logentry_setvalue(entry, f, addr)
	LogEntry *entry;
	LogField *f;
	void *addr;
{
	if (f->type == FIELD_TEXT)
		old_legacy_log_strncpy(&entry->record->buffer[f->offset], addr, f->size);
	else
		memcpy(&entry->record->buffer[f->offset], addr, f->size);
}

LogIndex old_legacy_logentry_getindex(LogEntry *entry)
{
	if (entry != NULL)
		return (LS_VINDEX(entry->idx, entry->record_generation));
	else
		return 0;
}

/*
 * Find the named field or return.
 * Default return value must be appended to macro when get-ting.
 */

#define FIND_OR_RETURN_(type) LogField *f = old_legacy_findfield(entry->logsys->label, name, type); if (!f) return 

char *
old_legacy_logentry_gettextbyname(entry, name)
	LogEntry *entry;
	char *name;
{
	FIND_OR_RETURN_(FIELD_TEXT) "";

	return (&entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_settextbyname(entry, name, value)
	LogEntry *entry;
	char *name;
	char *value;
{
	FIND_OR_RETURN_(FIELD_TEXT);

	old_legacy_log_strncpy(&entry->record->buffer[f->offset], value, f->size);
}

TimeStamp
old_legacy_logentry_gettimebyname(entry, name)
	LogEntry *entry;
	char *name;
{
	FIND_OR_RETURN_(FIELD_TIME) 0;

	return (*(TimeStamp *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_settimebyname(entry, name, value)
	LogEntry *entry;
	char *name;
	TimeStamp value;
{
	FIND_OR_RETURN_(FIELD_TIME);

	*(TimeStamp *) &entry->record->buffer[f->offset] = value;
}

Number
old_legacy_logentry_getnumberbyname(entry, name)
	LogEntry *entry;
	char *name;
{
	FIND_OR_RETURN_(FIELD_NUMBER) 0;

	return (*(Number *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setnumberbyname(entry, name, value)
	LogEntry *entry;
	char *name;
	Number value;
{
	FIND_OR_RETURN_(FIELD_NUMBER);

	*(Number *) &entry->record->buffer[f->offset] = value;
}

Integer
old_legacy_logentry_getintegerbyname(entry, name)
	LogEntry *entry;
	char *name;
{
	FIND_OR_RETURN_(FIELD_INTEGER) 0;

	return (*(Integer *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setintegerbyname(entry, name, value)
	LogEntry *entry;
	char *name;
	Integer value;
{
	FIND_OR_RETURN_(FIELD_INTEGER);

	*(Integer *) &entry->record->buffer[f->offset] = value;
}

LogIndex
old_legacy_logentry_getindexbyname(entry, name)
	LogEntry *entry;
	char *name;
{
	FIND_OR_RETURN_(FIELD_INDEX) 0;

	return (*(LogIndex *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setindexbyname(entry, name, value)
	LogEntry *entry;
	char *name;
	LogIndex value;
{
	FIND_OR_RETURN_(FIELD_INDEX);

	*(LogIndex *) &entry->record->buffer[f->offset] = value;
}

/* BugZ_9996: added for eSignature'CGI script */
int old_legacy_logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type)
{
        int ret=0;

        LogField *f = old_legacy_loglabel_getfield(i_entry->logsys->label, i_name);
        if (f)
        {
                *o_type = f->type;
                *o_data = &i_entry->record->buffer[f->offset];
                ret=1;
        }
        return ret;
}

/*
 * Access by field.
 * Caller must have called logsys_getfield() earlier.
 */

char *
old_legacy_logentry_gettextbyfield(entry, f)
	LogEntry *entry;
	LogField *f;
{
	return (&entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_settextbyfield(entry, f, value)
	LogEntry *entry;
	LogField *f;
	char *value;
{
	old_legacy_log_strncpy(&entry->record->buffer[f->offset], value, f->size);
}

TimeStamp
old_legacy_logentry_gettimebyfield(entry, f)
	LogEntry *entry;
	LogField *f;
{
	return (*(TimeStamp *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_settimebyfield(entry, f, value)
	LogEntry *entry;
	LogField *f;
	TimeStamp value;
{
	*(TimeStamp *) &entry->record->buffer[f->offset] = value;
}

Number
old_legacy_logentry_getnumberbyfield(entry, f)
	LogEntry *entry;
	LogField *f;
{
	return (*(Number *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setnumberbyfield(entry, f, value)
	LogEntry *entry;
	LogField *f;
	Number value;
{
	*(Number *) &entry->record->buffer[f->offset] = value;
}

Integer
old_legacy_logentry_getintegerbyfield(entry, f)
	LogEntry *entry;
	LogField *f;
{
	return (*(Integer *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setintegerbyfield(entry, f, value)
	LogEntry *entry;
	LogField *f;
	Integer value;
{
	*(Integer *) &entry->record->buffer[f->offset] = value;
}

LogIndex
old_legacy_logentry_getindexbyfield(entry, f)
	LogEntry *entry;
	LogField *f;
{
	return (*(LogIndex *) &entry->record->buffer[f->offset]);
}

void
old_legacy_logentry_setindexbyfield(entry, f, value)
	LogEntry *entry;
	LogField *f;
	LogIndex value;
{
	*(LogIndex *) &entry->record->buffer[f->offset] = value;
}

/*
 * Set from string into anytype field.
 * Again, either named- or by field-pointer.
 */
void
old_legacy_logentry_setbyfield(entry, f, s)
	LogEntry *entry;
	LogField *f;
	char *s;
{
	void *addr = &entry->record->buffer[f->offset];

	switch (f->type) {
	case FIELD_TEXT:
		old_legacy_log_strncpy(addr, s, f->size);
		break;
	case FIELD_TIME: {
		/*
		 * This uses only the start-time of the range,
		 * i.e. 1.2.1999 means 1.2.1999 00:00:00 AM
		 */
		time_t tms[2];
		tr_parsetime(s, tms);
		*(TimeStamp *) addr = tms[0];
		}
		break;
	case FIELD_INDEX:
		*(LogIndex *) addr = atoi(s);
		break;
	case FIELD_NUMBER:
		*(Number *) addr = atof(s);
		break;
	case FIELD_INTEGER:
		*(Integer *) addr = atoi(s);
		break;
	default:
		/* oops */
		break;
	}
}

void
old_legacy_logentry_setbyname(entry, name, s)
	LogEntry *entry;
	char *name;
	char *s;
{
	LogField *f = old_legacy_logsys_getfield(entry->logsys, name);
	if (f)
		old_legacy_logentry_setbyfield(entry, f, s);
}

/*
 * Copy fields from an oldstyle record into a newstyle one.
 * If type changes field is not copied.
 * Fields can overlap, but records cannot point to same space.
 *
 * Reserved fields are not copied.
 */

void
old_legacy_logentry_copyfields(oldent, newent)
	LogEntry *oldent, *newent;
{
	LogField *oldfield = oldent->logsys->label->fields;
	LogField *newfield;
	DiskLogRecord *offset_of = 0;

	for ( ; oldfield->type; ++oldfield) {

		/*
		 *  Skip fields before user-defined ones.
		 *  Unnecessary obfuscation without offsetof macro...
		 */
		if (oldfield->offset < (int32) offset_of->header.rest)
			continue;

		newfield = old_legacy_logsys_getfield(newent->logsys,
			oldfield->name.string);
		if (newfield == NULL)
			continue;

		if (oldfield->type != newfield->type)
			continue;

		old_legacy_logentry_setvalue(newent, newfield,
			old_legacy_logentry_getvalue(oldent, oldfield));
	}
}

/*
 * Copy 'from' into 'to'.
 *
 * result is no longer than size-1 and is always zero-terminated.
 */
void
old_legacy_log_strncpy(to, from, size)
	char *to;
	char *from;
	int size;
{
	mbsncpy( to, from, size - 1 );
}

/*
 * Append 'add' into 'txt'.
 *
 * result is no longer than size-1 and is always zero-terminated.
 */
void
old_legacy_log_strncat(txt, add, size)
	char *txt;
	char *add;
	int size;
{
	int len, addlen;

	len = strlen(txt);
	addlen = strlen(add);
	size--;

	if (len + addlen > size)
		addlen = size - len;
	if (addlen <= 0)
		return;

	txt += len;
	memcpy(txt, add, addlen);
	txt[addlen] = 0;
}

/*
 * Remove every substring 'sub' from 'txt'.
 */
void
old_legacy_log_strsubrm(txt, sub)
	char *txt;
	char *sub;
{
	int len, sublen;

	if (*sub == 0)
		return;

	sublen = strlen(sub);
	len = strlen(txt);
	while (len >= sublen) {
		if (*txt != *sub || memcmp(txt, sub, sublen)) {
			++txt;
			--len;
			continue;
		}
		memcpy(txt, txt + sublen, len + 1);
		len -= sublen;
	}
}


char **
old_legacy_logsys_getfieldnames(ls)
	LogSystem *ls;
{
	int i;
	LogLabel *label = ls->label;
	/*
	 * The last field is a dummy,
	 * so here is no +1 for the trailing NULL,
	 * as normally with char **.
	 */
	char **names = old_legacy_log_malloc(label->nfields * sizeof(*names));

	for (i = 0; i < label->nfields - 1; ++i)
		names[i] = old_legacy_log_strdup(label->fields[i].name.string);
	names[i] = NULL;

	return (names);
}

void
old_legacy_logsys_freenames(names)
	char **names;
{
	if (names) {
		char **p = names;

		while (*p)
			free(*p++);
		free(names);
	}
}

