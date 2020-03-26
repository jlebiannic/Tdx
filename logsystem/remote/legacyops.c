/*========================================================================
:set ts=4 sw=4

	E D I S E R V E R

	File:		logsystem/legacyops.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1997 Telecom Finland/EDI Operations

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: legacyops.c 53221 2017-06-22 13:20:01Z tcellier $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.08 06.10.97/JN	Old base ops now here.
  3.09 27.04.99/HT  some fixes to saveparms
  3.10 18.05.99/HT	Remove possible left over files, when creating
					new entry.
  4.4.? 22.09.04/JMH refactor the cache mechanism. Since now, we keep only the current
                     record for each base in cache. cache is not anymore shared by different connections
	22.10.2004/JMH  call log_writeentry into setaavalue - debug copyentry function	bugz 1153, 1215, 1216     
  4.5 30.05.2008/CRE Change debug_prefix
  4.6 18.05.2017/TCE(CG) EI-325 correct some memory leaks
========================================================================*/

#ifdef MACHINE_WNT
#include <windows.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#endif
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "perr.h"
#include "bases.h"

#ifndef DEBUG_PREFIX
#define DEBUG_PREFIX	" [unknown] "
#endif

extern int tr_useSQLiteLogsys;

/* XLogEntry is the linked list of current entries. 
 we should only a get an entry per base in that list. */
typedef struct XLogEntry {
	struct XLogEntry *next;  /* a pointer to the next cache entry */ 
	struct logentry *entry;  /* this entry */
	Base * base; 			/*   the Base for this entry */
	/*	int       refcnt;  no more counter for lock usage */
	unsigned  flags;
#define DIRTY     1        /* contains data not in disk */
#define REMOVED   2        /* someone removed it, still users */
#define IDLE      4        /* not modified lately */
} XLogEntry;


/*
	return the current Cache entry  for this connection.
	It walks on the cache list searching for the connection's current base 
*/
static XLogEntry * cacheGetCurrent(Client *p)
{
	XLogEntry * xent;
	if(!p|| !p->internal)
		return NULL;

	
	for ( xent = (XLogEntry *) p->internal; xent; xent = xent->next)
    {	    
			if (p->base == xent->base) 
				/* found cache for this db and this client */
				return xent;
	}
	return NULL;
}

/*
	retreive the current entry for this connection and the current database
*/
static LogEntry * cacheGetCurrentEntry(Client *p)
{
	XLogEntry * xent;
	xent = cacheGetCurrent(p);
	if(xent)
			return xent->entry;	    
	
	return NULL;
}

/*
	release the current cache entry and flush pending modification to disk
	this method is necessary because the protocol does not required explicit save  
	(Not a good thing but have to deal with it)
	
*/
static XLogEntry * cacheReleaseEntry(Client *p)
{
	XLogEntry * xent;
	
	xent = cacheGetCurrent(p);
	if(xent)
	{
		/* if old entry was not stored then do it  */
		/* we should no go through this condition cause since 1.6 setavalue write this entry */
		if( (xent->flags & DIRTY) && xent->entry)
			logentry_write(xent->entry);
		
		if(xent->entry)
			logentry_free(xent->entry);

		xent->entry = 0;
		xent->flags = 0;
	}  
	return xent;
}

/*
	Cache clean 
*/
static void cacheRemoveAll(Client *p)
{
	XLogEntry * xent;
	XLogEntry * next;

	if(!p|| !p->internal)
		return ;

	xent = (XLogEntry *) p->internal;
	
	while(xent)
	{
		next = xent->next;
		if(xent->entry)
			logentry_free(xent->entry);

		free(xent);
		xent = next;
	}
}


/*
	add this entry in the cache list
	replace the current entry for this database and flush the modifications of the old one to disk.
*/
static XLogEntry * cacheAddEntry(Client *p, LogEntry *entry)
{
	XLogEntry * xent;
	
	xent = cacheReleaseEntry(p);
	
	if(!xent)
	{   /* create a cache for this DB */
		xent = log_malloc(sizeof(XLogEntry));
		xent->base = p->base;
		/* EI-325 clear cache before adding an entry, to always have maximum 1 entry */
		cacheRemoveAll(p);
		xent->next   = NULL; /* EI-325 cache size = 1 for avoiding illimited cache growing */
		p->internal = xent;
	}
	xent->entry = entry;
	xent->flags = 0;
	return (xent);
}

/*
 *  Client has (re)opened a base.
 *  p->base->internal are valid.
 */
static void
legacy_init_client(Client *p)
{
}


static void
legacy_cleanup_client(Client *p)
{
	if (p->curfilter)
		logfilter_free(p->curfilter);

	cacheRemoveAll(p);
}

static void
legacy_periodic_sync (void *internal)
{
	/* no more synchonization  
	 *
	 * perhaps we should walk on the cache list for 
     * each connection and flush modifications to disk
	 * however this is not possible due because this method does not provide
	 * a parameter to walk on the connection list */
}

/*
 *  NULL & EFTYPE  - Not for us.
 *  NULL  other    - An error.
 *  other          - The internal structure of Base.
 */
static void *
legacy_open_base(char *sysname)
{
	LogSystem *log;

    /* we are in rls server side, this is where we look
     * in what mode we should use the bases : sqlite vs old legacy */
    if (getenv("USE_SQLITE_LOGSYS") != NULL)
        tr_useSQLiteLogsys = atoi(getenv("USE_SQLITE_LOGSYS"));
    else
        tr_useSQLiteLogsys = 0;

	log = logsys_open(sysname, LS_FAILOK);
	if (log == NULL) {
		errno = EFTYPE;
		return (NULL);
	}
    
	logsys_refresh(log);

	return (log);
}

/* Close for real. */
static void
legacy_close_base(void *base_internal)
{
	LogSystem *log = base_internal;
	logsys_close(log);
}

static int
map_fieldtype(int ourtype)
{
	switch (ourtype) {
	case FIELD_TEXT:
		return (_String);
	case FIELD_TIME:
		return (_Time);
	case FIELD_NUMBER:
		return (_Double);
	case FIELD_INTEGER:
		return (_Int);
	case FIELD_INDEX:
		return (_Int);
	}
	return (0);
}

/*
 *  Return 0 if IO- or protocol-error
 *  (client terminated).
 */

/*
 *  void
 *
 *  int version
 *  int maxcount
 *  int numused
 *  int 0
 *  int 0
 */

static void
legacy_get_base_info(Client *p)
{
	LogSystem *log = p->base->internal;

	vreply(p, RESPONSE, 0,
		_Int, log->label->version,
		_Int, log->label->maxcount,
		_Int, logsys_entry_count(log,NULL),
		_Int, log->label->nfields,
		_Int, 0,
		_End);
}

/*
 *  void
 *
 *  int count
 */
static void legacy_entry_count(Client *p)
{
	vreply(p,RESPONSE,0,_Int,logsys_entry_count(p->base->internal,p->curfilter),_End);
}

/*
 *  string field-name
 *
 *  fieldtype
 *  fieldsize
 */
static void
legacy_tell_named_field(Client *p, char *fieldname)
{
	LogSystem *log = p->base->internal;
	LogField *f;

	if ((f = loglabel_getfield(log->label, fieldname)) == NULL) {
		vreply(p, RESPONSE, 0,
			_Int, FIELD_NONE,
			_Int, 0,
			_End);
	} else {
		vreply(p, RESPONSE, 0,
			_Int, map_fieldtype(f->type),
			_Int, f->size,
			_String, f->format.string,
			_String, f->header.string,
			_String, f->name.string,
			_End);
	}
}

/*
 *  int field-index (in label)
 *
 *  fieldtype
 *  fieldsize
 *  fieldname
 *  fieldformat
 *  fieldheader
 */
/*
 *  int field-index (in label)
 *
 *  fieldname/fieldtype/fieldsize
 */

static void
legacy_tell_next_field(Client *p, int fldnum, int how)
{
	LogSystem *log = p->base->internal;
	LogLabel *label = log->label;

	if (fldnum >= label->nfields - 1) {
		switch (how) {
		default:
		case TELL_FIELD_TYPE:
		case TELL_FIELD_SIZE:
			vreply(p, RESPONSE, 0,
				_Int, 0,
				_End);
			break;
		case TELL_FIELD_NAME:
			vreply(p, RESPONSE, 0,
				_String, "",
				_End);
			break;
		}
	} else {
		switch (how) {
		default:
			vreply(p, RESPONSE, 0,
				_Int, map_fieldtype(label->fields[fldnum].type),
				_Int, label->fields[fldnum].size,
				_String, label->fields[fldnum].name.string,
				_String, label->fields[fldnum].format.string,
				_String, label->fields[fldnum].header.string,
				_End);
			break;
		case TELL_FIELD_NAME:
			vreply(p, RESPONSE, 0,
				_String, label->fields[fldnum].name.string,
				_End);
			break;
		case TELL_FIELD_TYPE:
			vreply(p, RESPONSE, 0,
				_Int, map_fieldtype(label->fields[fldnum].type),
				_End);
			break;
		case TELL_FIELD_SIZE:
			vreply(p, RESPONSE, 0,
				_Int, label->fields[fldnum].size,
				_End);
			break;
		}
	}
}

/*
 *  void
 *
 *  no ack.
 */
static void
legacy_clear_filter(Client *p)
{
	LogSystem *log = p->base->internal;

	logfilter_clear(p->curfilter);
	/* Jira EI-325 avoid free null pointer*/
	if ( (log != NULL ) && (log->indexes != NULL ) ){
		free(log->indexes);
	}
	log->indexes = NULL;
	p->walkidx = 0;
}

/*
 *  int order, -n/0/+n
 *
 *  no ack.
 */
static void
legacy_set_filter_order(Client *p, int order)
{
	logfilter_setorder(&p->curfilter, order);
}

/*
 *  string keyname
 *
 *  no ack.
 */
static void
legacy_set_filter_key(Client *p, char *keyname)
{
	logfilter_setkey(&p->curfilter, keyname);
}

/*
 *  string filter
 *
 *  no ack.
 */
static void legacy_add_filter(Client *p, char *filterstring)
{
	logfilter_add(&p->curfilter, filterstring);
}

static void legacy_add_filterparam(Client *p, char *filterstring)
{
	/* XXX pass printformat server side ? */
	logfilter_addparam(&p->curfilter, NULL, filterstring);
}

/*
 *  void
 *
 *  int keytype
 */
static void
legacy_compile_filter(Client *p)
{
	LogSystem *log = p->base->internal;
	p->walkidx = 0;

	logsys_refresh(log);
	logsys_compilefilter(log, p->curfilter);

	if (p->curfilter && p->curfilter->sort_key) {
		p->keyfield = logsys_getfield(log,
			p->curfilter->sort_key);
	} else {
		p->keyfield = NULL;
	}
	if (p->keyfield) {
		vreply(p, RESPONSE, 0,
			_Int, map_fieldtype(p->keyfield->type),
			_End);
	} else {
		vreply(p, RESPONSE, 0,
			_Int, 0,
			_End);
	}
}

/*
 *  void
 *
 *  int  index
 */
static void
legacy_get_next_match(Client *p)
{
	LogSystem *log = p->base->internal;
	LogEntry *entry = NULL;
	int found = 0;
	int idx;

	/* Get rid of current entry, then get next suitable. */
    /* We suppose that the current filter has been compiled before we get here */
	if (log->indexes == NULL)
	{
		log->indexes = logsys_list_indexed(log,p->curfilter);
	}
	if (log->indexes != NULL)
	{
		/* A list exits. Are there more entries in it ? */
		if (log->indexes[p->walkidx] != 0)
		{
			entry = logentry_readindex(log, log->indexes[p->walkidx]);
			log->indexes[p->walkidx] = 0;
			if (entry != NULL)
			{
				cacheAddEntry(p,entry);
				found = 1;
				p->walkidx++;
			}
		}
		else /* Was the list exhausted ? */
		{
			free(log->indexes);
			log->indexes = NULL;
			p->walkidx = 0;
		}
	}

	if (found)
    {
		/* One more reply */
		entry = cacheGetCurrentEntry(p);
		idx = logentry_getindex(entry);

		if (p->keyfield == NULL)
		{
			vreply(p,RESPONSE,0,_Index,idx,_End);
		}
		else
		{
			switch (p->keyfield->type)
			{
				case FIELD_TEXT: vreply(p,RESPONSE,0,_Index,idx,_String,logentry_gettextbyfield(entry, p->keyfield),_End); break;
				case FIELD_TIME: vreply(p,RESPONSE,0,_Index,idx,_Time,logentry_gettimebyfield(entry, p->keyfield),_End); break;
				case FIELD_NUMBER: vreply(p,RESPONSE,0,_Index,idx,_Double,logentry_getnumberbyfield(entry, p->keyfield),_End); break;
				case FIELD_INDEX: vreply(p,RESPONSE,0,_Index,idx,_Int,logentry_getindex(entry),_End); break;
				case FIELD_INTEGER: vreply(p,RESPONSE,0,_Index,idx,_Int,logentry_getintegerbyfield(entry, p->keyfield),_End); break;
				default: vreply(p,RESPONSE,0,_Index,idx,_Int,logentry_getintegerbyfield(entry, p->keyfield),_End); break;
			}
		}
	}
	else
		vreply(p,RESPONSE,0,_Index,0,_End);/* End of matches. */

}

static void legacy_read_entry(Client *p, int idx)
{
	LogSystem *log = p->base->internal;
	LogEntry *entry = cacheGetCurrentEntry(p);
	
	if ((!entry) || (logentry_getindex(entry) != idx))
	{
		/* release current entry will force to apply current changes */
		cacheReleaseEntry(p);

		entry = logentry_readindex(log, idx);
		
		if (entry)
			cacheAddEntry(p,entry);
	}

    vreply(p,RESPONSE,0,_Index,entry?logentry_getindex(entry):0,_End);
}

static void legacy_write_entry(Client *p)
{
	LogEntry * entry;
	entry = cacheGetCurrentEntry(p);
	if(entry)
	{
		logentry_write(entry);
	}
	reply(p, RESPONSE, 0);
}

/*
 *  int fromidx
 *
 *  int error
 *
 *   named record to current record.
 *  If named (old) record is not found active,
 *  read it in temporarily but free after .
 *  Be careful if copying to itself.
 */
static void legacy_copy_entry(Client *p, int fromidx)
{
	LogSystem *log = p->base->internal;
	LogEntry *from;
	LogEntry *to;
	
	to = cacheGetCurrentEntry(p);
	if (to == NULL) {
		p->dead = 1;
		return;
	}
	from = NULL;
	/* check should be done on the physical index value */
	if( (int)(fromidx/10) != to->idx)
	{
		from = logentry_readindex(log, fromidx);

		if (from ) {
   		    logentry_copyfields(from, to);
			logentry_copyfiles(from, to);
			logentry_settimebyname(from,"MODIFIED",log_curtime());
			logentry_write(to);
			logentry_free(from);		
		}
	}
	vreply(p, RESPONSE, 0, _Int, from ? 0 : -1, _End);
}

/*
 *  void
 *
 *  int   index
 */
static void
legacy_create_new(Client *p)
{
	LogSystem *log = p->base->internal;
	LogEntry *entry;

	entry = logentry_new(log);
	if (entry == NULL) {
		vreply(p, RESPONSE, 0, _Index, 0, _End);
	} else {
		cacheAddEntry(p,entry);
		/* 3.10/HT 18.05.99 */
		logentry_removefiles(entry, NULL);

		vreply(p, RESPONSE, 0, _Index, logentry_getindex(entry), _End);
	}
}

/*
 *  int  index
 *  string ext (can be missing, if the whole entry)
 *
 *  void
 */
static void
legacy_remove_subfiles(Client *p, int idx, char *ext)
{
	LogSystem *log = p->base->internal;
	int err = 1;	
	LogEntry *entry;

	entry = logentry_readindex(log, idx);

	if (entry) {
		/*
		 *  NULL ext means all files
		 */
		if (logentry_removefiles(entry, ext) == 0)
			err = 0;
		logentry_free(entry);
	}
	vreply(p, RESPONSE, 0, _Int, err ? -1 : 0, _End);
}

static void
legacy_remove_entry(Client *p, int idx)
{
	LogSystem *log = p->base->internal;
	int err = 1;

	if(logentry_remove(log, idx, NULL) == 0)
	{
		err = 0;
	}

	vreply(p, RESPONSE, 0, _Int, err ? -1 : 0, _End);
}

/*
 *  Supplemental files (and only such) under logsystems.
 */

static BOOL
legacy_open_file(Client *p,
	int idx,              /* index in logsystem */
	char *ext,            /* the supplemental extension */
	char *mode,           /* "r", "w" or "a" */
	int perms)            /* U*X style permissions for creation */
{
	LogSystem *log = p->base->internal;
	char filename[1024];

	if (LOGSYS_EXT_IS_DIR)
		sprintf(filename, "%s%s%s%s%s%s%d", NOTNUL(log->sysname), PSEP, "files", PSEP, NOTNUL(ext), PSEP, idx);
	else
		sprintf(filename, "%s%s%s%s%d.%s", NOTNUL(log->sysname), PSEP, "files", PSEP, idx, NOTNUL(ext));

	return (do_really_open_file(p, filename, mode, perms));
}

/*
 *  Get anything in any format,
 *  from record or (suppl.) parameter-files.
 *  Conversions are made for any datatype.
 *  Now one restriction, an entry must be involved here.
 */
static void
legacy_getavalue(Client *p,
	int how,
	char *name)
{
	
	LogEntry *entry ;
	char *cp;
	double n = 0.0;
	char *s = "";
	time_t t = 0;

	entry = cacheGetCurrentEntry(p);
	/*
	 *  Client has not specified the entry with rls_point() or other ways.
	 *  Give back "EMPTY" data.
	 */
	if (!entry)
		goto oops;

	if ((cp = strchr(name, '.')) != NULL) {
		/*
		 *  This is a parameter, after all.
		 *  Decode d.COUNTER and current entry
		 *  into entry.d and COUNTER
		 */
		char *base, *ext, *path;

		*cp++ = 0;
		ext = name;
		name = cp;

		path = logsys_filepath(entry->logsys, LSFILE_FILES);
		base = path + strlen(path);

		if (LOGSYS_EXT_IS_DIR)
			sprintf(base, "/%s/%d", NOTNUL(ext), logentry_getindex(entry));
		else
			sprintf(base, "/%d.%s", logentry_getindex(entry), NOTNUL(ext));

		getaparm(p, how, path, name);
	} else {
		/*
		 *  Field in record.
		 *  Convert real types to wanted type
		 *  (some conversion dont make sense).
		 */
		time_t tims[2];
		struct tm *tm;
		LogField *field;
		char line[2048];

		field = logsys_getfield(entry->logsys, name);
		if (field) {
			switch (how) {
				/*
				 *  User want doubles.
				 */
			case GET_NUM_FIELD:
				switch (field->type) {
				case FIELD_NUMBER:
					n = logentry_getnumberbyfield(entry, field);
					break;
				case FIELD_TEXT:
					n = atof(logentry_gettextbyfield(entry, field));
					break;
				case FIELD_TIME:
					n = logentry_gettimebyfield(entry, field);
					break;
				case FIELD_INDEX:
					n = logentry_getindex(entry);
					break;
				default:
					n = logentry_getintegerbyfield(entry, field);
					break;
				}
				break;
				/*
				 *  User wants time_T
				 */
			case GET_TIME_FIELD:
				switch (field->type) {
				case FIELD_NUMBER:
					t = logentry_getnumberbyfield(entry, field);
					break;
				case FIELD_TEXT:
					s = logentry_gettextbyfield(entry, field);
					tr_parsetime(s, tims);
					t = tims[0];
					break;
				case FIELD_TIME:
					t = logentry_gettimebyfield(entry, field);
					break;
				case FIELD_INDEX:
					t = logentry_getindex(entry);
					break;
				default:
					t = logentry_getintegerbyfield(entry, field);
					break;
				}
				break;
				/*
				 *  User wants string
				 */
			case GET_TEXT_FIELD:
				s = line;
				switch (field->type) {
				case FIELD_NUMBER:
					n = logentry_getnumberbyfield(entry, field);
					sprintf(s, "%lf", n);
					break;
				case FIELD_TEXT:
					s = logentry_gettextbyfield(entry, field);
					break;
				case FIELD_TIME:
					t = logentry_gettimebyfield(entry, field);
					tm = localtime(&t);
					if (tm)
						sprintf(s, "%02d.%02d.%02d %02d:%02d:%02d",
							tm->tm_mday,
							tm->tm_mon + 1,
							tm->tm_year % 100,
							tm->tm_hour,
							tm->tm_min,
							tm->tm_sec); /* dd.mm.yy HH:MM:SS */
					else
						sprintf(s, "00.00.00 00:00:00");
					break;
				case FIELD_INDEX:
					sprintf(s, "%d", logentry_getindex(entry));
					break;
				default:
					sprintf(s, "%d", logentry_getintegerbyfield(entry, field));
					break;
				}
				break;
			}
		}
	oops:
		switch (how) {
		case GET_NUM_FIELD:
		case GET_NUM_PARM:
			vreply(p, RESPONSE, 0, _Double, n, _End);
			break;
		case GET_TIME_FIELD:
		case GET_TIME_PARM:
			vreply(p, RESPONSE, 0, _Time, t, _End);
			break;
		case GET_TEXT_FIELD:
		case GET_TEXT_PARM:
			vreply(p, RESPONSE, 0, _String, s ? s : "", _End);
			break;
		}
	}
}

static void legacy_setavalue(Client *p, int how, char *name, rlsvalue_t *value)
{
	LogEntry *entry;
	
	char *cp;
	time_t tims[2];
	struct tm *tm;
	char line[2048];
	
	entry = cacheGetCurrentEntry(p);

	if (!entry)
		return;

	if ((cp = strchr(name, '.')) != NULL) {
		/* A parameter, after all.
		 *
		 * Decode d.COUNTER and p->internal
		 * into index.d and COUNTER */
		char *base, *ext, *path;

		*cp++ = 0;
		ext = name;
		name = cp;

		path = logsys_filepath(entry->logsys, LSFILE_FILES);
		base = path + strlen(path);

		if (LOGSYS_EXT_IS_DIR)
			sprintf(base, "/%s/%d", NOTNUL(ext), logentry_getindex(entry));
        else
			sprintf(base, "/%d.%s", logentry_getindex(entry), NOTNUL(ext));

		setaparm(p, how, path, name, value);
	} else {
		/* Field in record.
		 * Convert given type to real types.
		 * (some conversion dont make sense). */
		LogField *field;

		logentry_settimebyname(entry,"MODIFIED",log_curtime());

		field = logsys_getfield(entry->logsys, name);
		if (field) {
			switch (how) {
				/* User gave double. */
			case SET_NUM_FIELD:
				switch (field->type) {
				case FIELD_NUMBER:
					logentry_setnumberbyfield(entry, field, value->d);
					break;
				case FIELD_TEXT:
					sprintf(line, "%lf", value->d);
					logentry_settextbyfield(entry, field, line);
					break;
				case FIELD_TIME:
					logentry_settimebyfield(entry, field, (time_t) value->d);
					break;
				case FIELD_INDEX: /* NO !*/
					break;
				case FIELD_INTEGER:
					logentry_setintegerbyfield(entry, field, (int) value->d);
					break;
				}
				break;
				/* User gave time_T */
			case SET_TIME_FIELD:
				switch (field->type) {
				case FIELD_NUMBER:
					logentry_setnumberbyfield(entry, field, value->t);
					break;
				case FIELD_TEXT:
					tm = localtime(&value->t);
					sprintf(line, "%02d.%02d.%02d %02d:%02d:%02d",
						tm->tm_mday,
						tm->tm_mon + 1,
						tm->tm_year % 100,
						tm->tm_hour,
						tm->tm_min,
						tm->tm_sec); /* dd.mm.yy HH:MM:SS */
					logentry_settextbyfield(entry, field, line);
					break;
				case FIELD_TIME:
					logentry_settimebyfield(entry, field, value->t);
					break;
				case FIELD_INDEX:
					break;
				case FIELD_INTEGER:
					logentry_setintegerbyfield(entry, field, (int) value->t);
					break;
				}
				break;
				/* User gave string */
			case SET_TEXT_FIELD:
				switch (field->type) {
				case FIELD_NUMBER:
					logentry_setnumberbyfield(entry, field, atof(value->s));
					break;
				case FIELD_TEXT:
					logentry_settextbyfield(entry, field, value->s);
					break;
				case FIELD_TIME:
					tr_parsetime(value->s, tims);
					logentry_settimebyfield(entry, field, tims[0]);
					break;
				case FIELD_INDEX:
					break;
				case FIELD_INTEGER:
					logentry_setintegerbyfield(entry, field, atoi(value->s));
					break;
				}
				break;
			}
		}
		/* not interested in cache anymore */
		logentry_write(entry);
	}
}

static void legacy_enum_extensions(Client *p, int idx)
{
#ifdef MACHINE_WNT
	HANDLE files;
	WIN32_FIND_DATA dd;
#define dname dd.cFileName
#else
	DIR *files;
	struct dirent *dd;
#define dname dd->d_name
#endif
	char *base, *ext, *path;

	
	start_response(p, 0);

	path = logsys_filepath(p->base->internal, LSFILE_FILES);
	base = path + strlen(path);

#ifdef MACHINE_WNT
	strcpy(base, "\\**");
	files = FindFirstFile(path, &dd);
	*base = 0;
	if (files == INVALID_HANDLE_VALUE)
		files = NULL;
#else
	files = opendir(path);
#endif
	if (files) {
		if (idx) {
			/*
			 *  What extensions under THIS entry ?
			 */
			if (LOGSYS_EXT_IS_DIR) {
				/*
				 *  files/extensions/idx
				 */
#ifdef MACHINE_WNT
				do
#else
				while ((dd = readdir(files)) != NULL)
#endif
				{
					ext = dname;
					sprintf(base, "/%s/%d", NOTNUL(ext), idx);
					if (access(path, 0) == 0)
						append_line(&p->buf, ext);
				}
#ifdef MACHINE_WNT
				while (FindNextFile(files, &dd));
#endif
			} else {
				/*
				 *  files/idx.extensions
				 */
				int  idxlen;
				char idxbuf[256];

				sprintf(idxbuf, "%d.", idx);
				idxlen = strlen(idxbuf);

#ifdef MACHINE_WNT
				do
#else
				while ((dd = readdir(files)) != NULL)
#endif
				{
					/*
					 *  digits dot  has to match.
					 */
					if (strncmp(dname, idxbuf, idxlen))
						continue;
					append_line(&p->buf, dname + idxlen);
				}
#ifdef MACHINE_WNT
				while (FindNextFile(files, &dd));
#endif
			}
		} else {
			/*
			 *  What extensions under ANY entries ?
			 */
			if (LOGSYS_EXT_IS_DIR) {
				/*
				 *  files/ext/indexes
				 */
#ifdef MACHINE_WNT
				do
#else
				while ((dd = readdir(files)) != NULL)
#endif
				{
					ext = dname;
					if (ext[0] != '.')
						append_line(&p->buf, ext);
				}
#ifdef MACHINE_WNT
				while (FindNextFile(files, &dd));
#endif
			} else {
				/*
				 *  files/indexes.extensions
				 *
				 *  Hardest this way,
				 *  first build a list of encountered
				 *  extensions, then, after running all
				 *  the files to this list,
				 *  send the accumulated list of
				 *  extensions (each extension only once)
				 *  to client.
				 */
				char *v[512];
				int i, n;

				/*
				 *  Start collecting encountered extensions.
				 */
				n = 0;

#ifdef MACHINE_WNT
				do
#else
				while ((dd = readdir(files)) != NULL)
#endif
				{
					/*
					 *  digits dot ext
					 *  Just checking for initial digit
					 *  and first dot.
					 */
					if (!isdigit(dname[0]))
						continue;
					ext = strchr(dname, '.');
					if (ext++ == NULL)
						continue;
					/*
					 *  Search from previous,
					 *  add if not found and
					 *  still room (should be).
					 */
					for (i = 0; i < n; ++i)
						if (!strcmp(ext, v[i]))
							break;
					if (i == n && n < sizeof(v) / sizeof(v[0]))
						if ((v[n] = strdup(ext)) != NULL)
							n++;
				}
#ifdef MACHINE_WNT
				while (FindNextFile(files, &dd));
#endif
				for (i = 0; i < n; ++i) {
					append_line(&p->buf, v[i]);
					free(v[i]);
				}
			}
		}
#ifdef MACHINE_WNT
		FindClose(files);
#else
		closedir(files);
#endif
	}
	flush_lines(p);
}

static int
legacy_get_curidx(Client *p)
{
	
	LogEntry * entry = cacheGetCurrentEntry(p);
	if (entry)
		return logentry_getindex(entry);
	return (0);
}

/*
 *  Delete all parameters for current LOG.d
 */
static void
do_parms(Client *p, int idx, char *ext, int what)
{
	LogSystem *log = p->base->internal;
	char *path, *base;

	path = logsys_filepath(log, LSFILE_FILES);
	base = path + strlen(path);

	if (LOGSYS_EXT_IS_DIR)
		sprintf(base, "/%s/%d", NOTNUL(ext), idx);
	else
		sprintf(base, "/%d.%s", idx, NOTNUL(ext));

	switch (what)
	{
		default:
			*(int *) 0 = 0;
		case CLEAR_PARMS:
			if ((remove(path) != 0) && (DEBUG(12)))
				debug("%sCLEAR_PARMS: failed to remove '%s' (%d)\n",DEBUG_PREFIX, NOTNUL(path), errno);
			break;
		case ENUM_PARMS:
		case FETCH_PARMS:
			start_response(p, 0);
			listparms(p, path, what);
			flush_lines(p);
			break;
	}
}

static void legacy_clear_parms(Client *p, int idx, char *ext)
{
	do_parms(p, idx, ext, CLEAR_PARMS);
}

static void legacy_enum_parms(Client *p, int idx, char *ext)
{
	do_parms(p, idx, ext, ENUM_PARMS);
}

static void legacy_fetch_parms(Client *p, int idx, char *ext)
{
	do_parms(p, idx, ext, FETCH_PARMS);
}

static BOOL legacy_stat_file(Client *p,
	int idx,              /* index in logsystem */
	char *ext,            /* the supplemental extension */
	stat_t *st_ret)  /* result */
{
	LogSystem *log = p->base->internal;
	char filename[1024];

	if (LOGSYS_EXT_IS_DIR)
		sprintf(filename, "%s%s%s%s%s%s%d", NOTNUL(log->sysname), PSEP, "files", PSEP, NOTNUL(ext), PSEP, idx);
	else
		sprintf(filename, "%s%s%s%s%d.%s", NOTNUL(log->sysname), PSEP, "files", PSEP, idx, NOTNUL(ext));

	if (stat(filename, st_ret) == -1) {
		memset(st_ret, 0, sizeof(*st_ret));
		return (0);
	}
	return (1);
}

#ifdef MACHINE_WNT
#define	X_OK		0x00
#endif

static void legacy_save_parms(Client *p, int idx, char *ext, char *s)
{
	LogSystem *log = p->base->internal;
	char *path, *base;
	FILE *fp;

	path = logsys_filepath(log, LSFILE_FILES);
	base = path + strlen(path);

	/* 3.09/HT    27.04.99 */
	if (LOGSYS_EXT_IS_DIR)
	{
		if (access( path, X_OK ))
			mkdir( path, 0755 );

		sprintf(base, "%s%s", PSEP, NOTNUL(ext));
		if ( access( path, X_OK ) )
			mkdir( path, 0755 );

		sprintf(base, "%s%s%s%d", PSEP, NOTNUL(ext), PSEP, idx);
	}
	else
		sprintf(base, "%s%d.%s", PSEP, idx, NOTNUL(ext));

	fp = fopen(path, "w");
	if (fp) {
		fprintf(fp, "%s", NOTNUL(s));
		fclose(fp);
		vreply(p, RESPONSE, 0, _Int, 0, _End);
	}
	else
		vreply(p, RESPONSE, 0, _Int, -1, _End);

}

baseops_t legacy_baseops = {

	legacy_open_base,
	legacy_close_base,
	legacy_cleanup_client,

	legacy_periodic_sync,

	legacy_get_base_info,
	legacy_tell_named_field,
	legacy_tell_next_field,

	legacy_clear_filter,
	legacy_set_filter_order,
	legacy_set_filter_key,
	legacy_add_filter,
	legacy_compile_filter,
	legacy_get_next_match,

	legacy_read_entry,
	legacy_write_entry,
	legacy_copy_entry,

	legacy_create_new,
	legacy_remove_subfiles,
	legacy_remove_entry,

	legacy_getavalue,
	legacy_setavalue,

	"TYPOCHECK",

	legacy_open_file,

	legacy_enum_extensions,

	legacy_init_client,

	legacy_get_curidx,

	legacy_clear_parms,
	legacy_enum_parms,
	legacy_fetch_parms,

	"TYPOCHECK",

	legacy_stat_file,
	
	legacy_save_parms,

	legacy_add_filterparam,

	legacy_entry_count,
};

baseops_t * init_legacyops(void)
{
	return (&legacy_baseops);
}

