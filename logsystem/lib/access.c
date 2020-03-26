/*========================================================================
        TradeXpress

        File:		logsystem/access.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994-2003 Telecom Finland/EDI Operations
        Copyright Generix-Group 1990-2011

	Routines used to access fields in logentries.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: access.c 55239 2019-11-19 13:50:31Z sodifrance $")
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

#include "logsystem.sqlite.h"
#include "logsystem.h"

extern double atof();

LogField * sqlite_findfield(LogLabel *label, char *name)
{
	LogField *field = NULL;
	int idx = 0;

	if (!name)
		return NULL;

	while ((field = label->fields + idx++) != NULL)
	{
		/* last field */
		if (!field->name.string)
			return NULL;

		if (!strcmp(field->name.string, name))
		{
			return field;
		}
	}

	return NULL;
}

LogField * sqlite_logsys_getfield(LogSystem *log, char *name)
{
	LogField *f;
	
	f = sqlite_findfield(log->label, name);
	if (f)
		return (f);

	/* Not a real field. Any of the "pseudo" fields ? */
	if (!strcmp(name, "_OWNER"))
    {
		static LogField owner;

		owner.type = PSEUDOFIELD_OWNER;
		owner.header.string = "User";
		/* 3.03 8 -> 16 */
		owner.format.string = "%-16.16s";

		return (&owner);
	}
	return (NULL);
}

LogIndex sqlite_logentry_getindex(LogEntry *entry)
{
	return entry->idx;
}

/*JRE 06.15 BG-81*/
LogIndexEnv sqlite_logentry_getindex_env(LogEntryEnv *entry)
{
	return entry->idx;
}
/*End JRE*/

/* Access by name
 * try to find the corresponding field inside
 *-------------------------------------------------------------------------------*/
#define SQLITE_LOGENTRY_GETBYNAME(returntype,methodtype,defaultreturned) \
returntype sqlite_logentry_get##methodtype##byname(LogEntry *entry, char* name) \
{ \
	LogField *f = sqlite_findfield(entry->logsys->label, name); if (!f) return defaultreturned; \
	return (*(returntype *) &entry->record->buffer[f->offset]); \
}

SQLITE_LOGENTRY_GETBYNAME(TimeStamp,time,0)
SQLITE_LOGENTRY_GETBYNAME(Number,number,0)
SQLITE_LOGENTRY_GETBYNAME(Integer,integer,0)
SQLITE_LOGENTRY_GETBYNAME(LogIndex,index,0)
SQLITE_LOGENTRY_GETBYNAME(char *,text,"")

int sqlite_logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type)
{
        int ret=0;

        LogField *f = sqlite_findfield(i_entry->logsys->label, i_name);
        if (f)
        {
                *o_type = f->type;
                *o_data = &i_entry->record->buffer[f->offset];
                ret=1;
        }
        return ret;
}

#define SQLITE_LOGENTRY_SETBYNAME(returntype,methodtype) \
void sqlite_logentry_set##methodtype##byname(LogEntry *entry, char* name, returntype value) \
{ \
	LogField *f; \
	if (!entry) \
		return; \
	f = sqlite_findfield(entry->logsys->label, name); \
	if (f) \
		*(returntype *) &entry->record->buffer[f->offset] = value; \
}

SQLITE_LOGENTRY_SETBYNAME(TimeStamp,time)
SQLITE_LOGENTRY_SETBYNAME(Number,number)
SQLITE_LOGENTRY_SETBYNAME(Integer,integer)
SQLITE_LOGENTRY_SETBYNAME(LogIndex,index)

void sqlite_logentry_settextbyname(LogEntry *entry, char *name, char *value)
{
	LogField *f;
	if (!entry)
		return;
	f = sqlite_findfield(entry->logsys->label, name);
	if (f)
		sqlite_log_strncpy(&entry->record->buffer[f->offset], value, f->size);
}

/* Access by field.
 * Caller must have called logsys_getfield() earlier.
 *-------------------------------------------------------------------------------*/
#define SQLITE_LOGENTRY_GETBYFIELD(returntype,methodtype) \
returntype sqlite_logentry_get##methodtype##byfield(LogEntry *entry, LogField *f) \
{ \
	return (*(returntype *) &entry->record->buffer[f->offset]); \
}

SQLITE_LOGENTRY_GETBYFIELD(TimeStamp,time)
SQLITE_LOGENTRY_GETBYFIELD(Number,number)
SQLITE_LOGENTRY_GETBYFIELD(Integer,integer)
SQLITE_LOGENTRY_GETBYFIELD(LogIndex,index)

char * sqlite_logentry_gettextbyfield(LogEntry *entry, LogField *f)
{
	return (&entry->record->buffer[f->offset]);
}

#define SQLITE_LOGENTRY_SETBYFIELD(returntype,methodtype) \
void sqlite_logentry_set##methodtype##byfield(LogEntry *entry, LogField *f, returntype value) \
{ \
	*(returntype *) &entry->record->buffer[f->offset] = value; \
}

SQLITE_LOGENTRY_SETBYFIELD(TimeStamp,time)
SQLITE_LOGENTRY_SETBYFIELD(Number,number)
SQLITE_LOGENTRY_SETBYFIELD(Integer,integer)
SQLITE_LOGENTRY_SETBYFIELD(LogIndex,index)

void sqlite_logentry_settextbyfield(LogEntry *entry, LogField *f, char *value)
{
	sqlite_log_strncpy(&entry->record->buffer[f->offset], value, f->size);
}

/* Set from string into anytype field. Again, either named- or by field-pointer. */
void sqlite_logentry_setbyfield(LogEntry *entry, LogField *f, char *s)
{
	void *addr = &entry->record->buffer[f->offset];

	switch (f->type) 
    {
		case FIELD_TEXT: sqlite_log_strncpy(addr, s, f->size); break;
		case FIELD_TIME:
			{
				/* This uses only the start-time of the range, i.e. 1.2.1999 means 1.2.1999 00:00:00 AM */
				time_t tms[2];
				tr_parsetime(s, tms);
				*(TimeStamp *) addr = tms[0];
			}
			break;
		case FIELD_INDEX:  *(LogIndex *) addr = atoi(s); break;
		case FIELD_NUMBER:  *(Number *)  addr = atof(s); break;
		case FIELD_INTEGER: *(Integer *) addr = atoi(s); break;
		default:
			/* oops */
			break;
	}
}

void sqlite_logentry_setbyname(LogEntry *entry, char *name, char *s)
{
	LogField *f = sqlite_logsys_getfield(entry->logsys, name);
	if (f)
		sqlite_logentry_setbyfield(entry, f, s);
}

static void * sqlite_logentry_getvalue(LogEntry *entry, LogField *field)
{
	return (&entry->record->buffer[field->offset]);
}

static void sqlite_logentry_setvalue(LogEntry *entry, LogField *f, void *addr)
{
	if (f->type == FIELD_TEXT)
		sqlite_log_strncpy(&entry->record->buffer[f->offset], addr, f->size);
	else
		memcpy(&entry->record->buffer[f->offset], addr, f->size);
}

/* Copy fields from an oldstyle record into a newstyle one.
 * If type changes field is not copied.
 * Fields can overlap, but records cannot point to same space.
 * Reserved fields are not copied. */
void sqlite_logentry_copyfields(LogEntry *oldent, LogEntry *newent)
{
	LogField *oldfield = oldent->logsys->label->fields;
	LogField *newfield;
	DiskLogRecord *offset_of = 0;

	for ( ; oldfield->type; ++oldfield)
    {
		/* Skip fields before user-defined ones. Unnecessary obfuscation without offsetof macro...  */
		if (oldfield->offset < (int32) offset_of->header.rest)
			continue;

		newfield = sqlite_logsys_getfield(newent->logsys, oldfield->name.string);
		if (newfield == NULL)
			continue;

		if (oldfield->type != newfield->type)
			continue;

		sqlite_logentry_setvalue(newent, newfield, sqlite_logentry_getvalue(oldent, oldfield));
	}
}

/* Copy 'from' into 'to'.
 * result is no longer than size-1 and is always zero-terminated. */
void sqlite_log_strncpy(char *to, char *from, int size)
{
	mbsncpy( to, from, size - 1);
}

/* Append 'add' into 'txt'.
 * result is no longer than size-1 and is always zero-terminated. */
void sqlite_log_strncat(char *txt, char *add, int size)
{
	int len, addlen, phylen;

	len = mbstowcs( NULL, txt, 0 );
	addlen = mbstowcs( NULL, add, 0);
	size--;

	if (len + addlen > size)
		addlen = size - len;
	if (addlen <= 0)
		return;

	phylen = strlen(txt);

	txt += phylen;
	mbsncpy( txt, add, addlen );
}

/* Remove every substring 'sub' from 'txt'. */
void sqlite_log_strsubrm(char *txt, char *sub)
{
	int len, sublen;

	if (*sub == 0)
		return;

	sublen = strlen(sub);
	len = strlen(txt);
	while (len >= sublen) 
    {
		if (*txt != *sub || memcmp(txt, sub, sublen))
        {
			++txt;
			--len;
			continue;
		}
		memcpy(txt, txt + sublen, len + 1);
		len -= sublen;
	}
}

