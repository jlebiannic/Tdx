/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_logsystem.c
	Programmer:	Mikko Valimaki
	Date:		Wed Sep  2 14:08:14 EET 1992

	Copyright (c) 1994 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_logsyslocal.c 55071 2019-07-29 12:52:29Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 28.09.94/JN	Created, tr_logsys.c used as a template.
  3.01 03.10.94/JN	Cosmetics.
  3.02 03.04.95/JN	tr_lsCreate second argument was unused.
  3.03 05.04.95/JN	logentries leaked like pigs.
  3.04 08.02.96/JN	linting.
  3.05 03.03.96/JN	valid(LOG) -> tr_lsValidEntry(&LOG)
  3.06 02.04.96/JN	LOGSYS_EXT_IS_DIR
       25.06.98/JN	cosmetics, varargs -> stdarg
  3.07 24.03.99/KP	Renamed all tr_ls* -> tr_local*
			va_list version of tr_lsFindFirst
			tr_localFetchParms()
			tr_localSaveParms()
  3.08 27.05.99/KP	In tr_localSaveParm() try to create one level
			of path if it does not exist.
  3.09 21.06.99/KP	tr_localGetTextParm returned NULL if requested
			parameter did not exist. Now returns "".
  3.10 22.06.99/KP	tr_localCopyFields() did not copy ext. files.
  3.11 12.08.99/KP	Small bug in the call of logentry_copyfiles()
			in tr_localCopyFields().
  3.12 30.11.99/KP	When removing logentry, destroy actual entry only
			if all files are deleteed ok. (Caused trouble in NT)
  3.13 01.12.99/KP	When creating a new entry, remove all possibly
			existing subfiles, just in case.
  3.14 04.12.99/KP	tr_localTryGetUniq()
  3.15 15.12.99/KP	tr_localGetTextParm() returned NULL if file did not exist.
  4.00 12.09.06/LM  	tr_localCopyEntry return 0 if there is an error BugZ 6346
  4.01 15.09.06/LM	restore SQLite merge because previous release was erroneously
  			build from OLD_LEGACY branch !!!
  4.02 10.02.14/TCE(CG) TX-2497 improve code reliability when trying to open a file that doesn't exist
  4.03 17.04.15/YLI(CG) TX-2606 correction : modify pointer end line computation when '/n' is absent.
  4.04 07.09.16/SCH(CG) TX-2896 : change field= filter shape by field<0 or 'field<>' by field>0
  4.05 06.06.19/CDE TX-3136 : use ascii char 1 instead of number "0" in field comparaison to retrieve values starting by "("
  4.06 10.07.2019/ORE(SF) TX-3123: UNICODE adaptation

========================================================================*/

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef MACHINE_WNT
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_logsystem.h"

static void setaparm(char *path, char *name, char *value);

/*========================================================================
  Variable argument list:
	LogSysHandle   * log handle,
	int              sorting order,
	LogFieldHandle * sorting key,
	char           * logsysname,
	filter ops
		LogFieldHandle *fh,
		int comparison,
		int valuetype;
		{char *, double, time_t, int} value,
	NULL terminates list
========================================================================*/

int tr_localFindFirst(LogSysHandle *handle, int order, LogFieldHandle *key, char *path, ...)
{
	int result;
	va_list		ap;
	va_start(ap, path);

	result = tr_localVFindFirst(handle, order, key, path, ap);

	va_end(ap);

	return result;
}

/*
 * A va_list version of the tr_lsFindFirst. The one above is just a wrapper
 * for this. (KP)
 */
int tr_localVFindFirst( LogSysHandle *handle, int order, LogFieldHandle *key, char *path, va_list ap)
{
	LogFieldHandle	*field;
	int		cmp;
	union {
		char  *s;
		double d;
		time_t t;
		int    i;
	} value;

    
	char *valp, valbuf[4096];

	/* Release anything hanging from the handle. */
	tr_localFreeEntryList(handle);
	/* Make sure the system is open. label created ... */
	tr_localOpen(handle, path);

	/* Reset filter and insert new values to it. */
	logfilter_clear(handle->filter);

	if (key) {
		logfilter_setorder((LogFilter **) &handle->filter, order);
		logfilter_setkey  ((LogFilter **) &handle->filter, key->name);
	}

	while ((field = va_arg(ap, LogFieldHandle *)) != NULL) {
		cmp   = va_arg(ap, int);

		/* This is silly. */
		switch (va_arg(ap, int)) {
		case 0:
			value.s = va_arg(ap, char *);
			valp = value.s;
			break;
		case 1:
			value.d = va_arg(ap, double);
			sprintf(valbuf, "%lf", value.d);
			valp = valbuf;
			break;
		case 2:
			value.t = va_arg(ap, time_t);
			sprintf(valbuf, "zero + %ld", (long) value.t);
			valp = valbuf;
			break;
		default:
			value.i = va_arg(ap, int);
			sprintf(valbuf, "%d", value.i);
			valp = valbuf;
			break;
		}

		/* TX-2896 07/09/2016 SCH(CG) : change field= filter shape by field<0 or field<> by field >0 */
		if (tr_SQLiteMode() == 1) {
			if (*valp == '\0') {
				if (cmp==OP_EQ) {
					cmp = OP_LT;
				} else {
					cmp = OP_GT;
				}
				/* TX-3136 use char with code 001 instead of char "0" */
				valp = strdup("\001");
			}
		}
		/* End TX-2896 */
		logfilter_insert((LogFilter **) &handle->filter, field->name, cmp, valp);
	}
	va_end(ap);

	logsys_compilefilter(handle->logsys, handle->filter);

    if (tr_SQLiteMode() == 0)
    {
        LogEntry **entrylist;

        entrylist = logsys_makelist_indexed(handle->logsys, handle->filter);

        if (entrylist == NULL || entrylist[0] == NULL) {
            /* None found. (This check for zero count is just paranoia.) */
            if (entrylist){
                free(entrylist);
			}
            return (0);
        }
        /* At least one. Take it off from the list. */
        handle->logEntry = entrylist[0];
        entrylist[0] = NULL;

        if (entrylist[1] == NULL) {
            /* Just this one entry in the list. We do not need the list anymore. */
            free(entrylist);
        } else {
            /* Multiple entries in the list. Remember the place for find(). */
            handle->listIndex = 1;
            handle->entryList = (void **) entrylist;
        }
    }
    else
    {
        LogIndex *indexlist;

        indexlist = logsys_list_indexed((LogSystem *)handle->logsys,handle->filter);

        if (indexlist == NULL)
        {
            /* None found. This check for zero count is just paranoia. */
            free(indexlist);
            return (0);
        }

        /* At least one. Take it off from the list. */
        handle->logEntry = logentry_readindex((LogSystem *) handle->logsys, indexlist[0]);

        indexlist[0] = 0;

        if (indexlist[1] == 0) {
            /* Just this one entry in the list. We do not need the list anymore. */
            free(indexlist);
        } else {
            /* Multiple entries in the list. Remember the place for find(). */
            handle->listIndex = 1;
            handle->indexList = (void *) indexlist;
        }
    }
	return (1);	/* ok */
}

int tr_localFindNext(LogSysHandle *handle)
{
	/* Free previous active entry. */
	if (handle->logEntry){
		logentry_free(handle->logEntry);
	}
	handle->logEntry = NULL;

    if (tr_SQLiteMode() == 0)
    {
        if (handle->entryList)
        {
            /* A list exits. Are there more entries in it ? */
            handle->logEntry = handle->entryList[handle->listIndex];
            handle->entryList[handle->listIndex] = NULL;

            if (handle->logEntry){
                handle->listIndex++;
			}
            /* Was the list exhausted ? */
            if (handle->entryList[handle->listIndex] == NULL)
            {
                free(handle->entryList);
                handle->entryList = NULL;
                handle->listIndex = 0;
            }
        }
    }
    else
    {
        if (handle->indexList)
        {
            /* A list exits. Are there more entries in it ? */
            if (handle->indexList[handle->listIndex] != 0)
            {
                handle->logEntry = logentry_readindex(handle->logsys, handle->indexList[handle->listIndex]);
                handle->indexList[handle->listIndex] = 0;
            }
            if (handle->logEntry){
                handle->listIndex++;
			}
            /* Was the list exhausted ? */
            if (handle->indexList[handle->listIndex] == 0)
            {
                free(handle->indexList);
                handle->indexList = NULL;
                handle->listIndex = 0;
            }
        }
    }
	return (!!handle->logEntry); /* True if ok */
}

/* When listIndex is incremented, list entries should be
 * removed from the list at the same time (we dont look back
 * in the list). */
void tr_localFreeEntryList(LogSysHandle *handle)
{
	if (handle->logEntry){
		logentry_free(handle->logEntry);
	}
	handle->logEntry = NULL;

    if (tr_SQLiteMode() == 0)
    {
        if (handle->entryList)
        {
            int i = handle->listIndex;
            if (i < 0){
                i = 0;
			}
            for ( ; handle->entryList[i]; ++i){
                logentry_free(handle->entryList[i]);
			}
            free(handle->entryList);
        }
        handle->entryList = NULL;
    }
    else
    {
        if (handle->indexList){
            free(handle->indexList);
		}
        handle->indexList = NULL;
    }
	handle->listIndex = 0;
}

/*============================================================================
  If the database does not contain such a field, do not bother to log error.
  There has already been at least one warning noting the fact, when
  the field-handle was compiled.

  These should be macroized...
  Time manipulation is not in balance...
============================================================================*/

char *tr_localGetTextVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (TR_EMPTY);
	}
	if (!fh->logField) {
		return (TR_EMPTY);
	}
	return tr_MemPool(logentry_gettextbyfield(sh->logEntry, fh->logField));
}

double tr_localGetNumVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	LogField *field = fh->logField;

	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0.0);
	}
	if (!fh->logField) {
		return (0.0);
	}
	if (field->type == FIELD_INDEX){
		return (logentry_getindex(sh->logEntry));
	}

	return (logentry_getnumberbyfield(sh->logEntry, field));
}

time_t tr_localGetTimeVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	if (!fh->logField) {
		return (0);
	}
	return (logentry_gettimebyfield(sh->logEntry, fh->logField));
}

int tr_localSetTextVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	if (!fh->logField) {
		return (0);
	}
	logentry_settextbyfield(sh->logEntry, fh->logField, value);

	if (!sh->autoFlush) {
		/*
		 * Skip write if "autoflush off".
		 * User is expected to explicitly "flush(LOG)".
		 */
		return (1);
	}
	return (!logentry_write(sh->logEntry));
}

int tr_localSetNumVal(LogSysHandle *sh, LogFieldHandle *fh, double value)
{
	LogField *field = fh->logField;

	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	if (!fh->logField) {
		return (0);
	}
	if (field->type == FIELD_INDEX){
		logentry_setintegerbyfield(sh->logEntry, field,
			(Integer) (value + 0.5));
	} else {
		logentry_setnumberbyfield(sh->logEntry, field,
			(Number) value);
	}
	
	if (!sh->autoFlush) {
		return (1);
	}
	return (!logentry_write(sh->logEntry));
}

int tr_localSetTimeVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	time_t tims[2];

	if (!sh->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	if (!fh->logField) {
		return (0);
	}
	tr_parsetime(value, tims);

	logentry_settimebyfield(sh->logEntry, fh->logField, tims[0]);

	if (!sh->autoFlush) {
		return (1);
	}
	return (!logentry_write(sh->logEntry));
}

/*============================================================================
  Copy one entry into another.
============================================================================*/

int tr_localCopyEntry (LogSysHandle *to, LogSysHandle *from)
{
	int rc;

	if (!to->logEntry || !from->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}

	if (from->logEntry == to->logEntry) {
		/* Gotcha ! */
	} else {
		logentry_copyfields(from->logEntry, to->logEntry);
	}

	/*
	 * 3.10/KP : Copy extension files also!
	 * 3.11/KP : Should be from->logEntry and to->logEntry !!!
	 * 	logentry_copyfiles(from, to);
	 */
	rc = logentry_copyfiles(from->logEntry, to->logEntry);
    if (rc) { 
		return (0);
	}

	if (!to->autoFlush) {
		return (1);
	}
	return (!logentry_write(to->logEntry));
}

/*============================================================================
  Create a new entry in system.
============================================================================*/

int tr_localCreate (LogSysHandle *handle, char *path)
{
	LogEntry *entry;

	/*
	 * Free old junk and make sure the system is open.
	 */
	tr_localFreeEntryList(handle);
	tr_localOpen(handle, path);

	entry = logentry_new(handle->logsys);
	if (entry == NULL){
		tr_Fatal(TR_WARN_CANNOT_CREATE_NEW, handle->path);
	} else {
		/*
		 * 3.13/KP : Remove all subfiles, just-in-case.
		 */
		logentry_removefiles(entry, NULL);
	}

	handle->logEntry  = entry;
    return 0;
}

/*============================================================================
  Remove the entry and everything related to it.
============================================================================*/

int tr_localRemove(LogSysHandle *handle)
{
	int ok = 1;
	LogEntry *entry = handle->logEntry;

	if (!entry) {
		tr_Log (TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	/*
	 * All files there is under this index
	 * and the index itself.
	 */
	/*
	 * 3.12/KP : Destroy actual entry if and ONLY IF all files have been
	 * safely removed.
	 */
	if (logentry_removefiles(entry, NULL)){
		ok = 0;
	} else {
		if (logentry_destroy(entry)) {
		ok = 0;
		}
	}

	handle->logEntry = NULL;

	return (ok);
}

/*============================================================================
  Return the path of a specific file, extension.
  Returned string is temporarily saved in the string pool.
  Next GC will destroy it.
============================================================================*/

char* tr_localPath (LogSysHandle *handle, char *extension)
{
	extern int LOGSYS_EXT_IS_DIR;
	char tmp[1024];

	LogSystem *logsys = handle->logsys;
	LogEntry  *entry  = handle->logEntry;

	if (!entry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (TR_EMPTY);
	}
	if (LOGSYS_EXT_IS_DIR) {
		sprintf (tmp, "%s/files/%s/%d", logsys->sysname,
			extension, logentry_getindex(entry));
	} else {
		sprintf (tmp, "%s/files/%d.%s", logsys->sysname,
			logentry_getindex(entry), extension);
	}
	return (tr_MemPool(tmp));
}

/*============================================================================
  Logsystem fields are searched and pointers are inserted into list.
  Missing field causes a warning message, and the count of misses is returned,
  if someone would be interested on that...
============================================================================*/

int tr_localCompileFields(LogSysHandle *handle)
{
	LogFieldHandle *lfh;
	LogField *field;
	int n = 0;

	if (handle->logsys == NULL) {
		return (-1);
	}

	for (lfh = handle->fields; lfh; lfh = lfh->next)
	{
		lfh->logField = NULL;
		field = logsys_getfield(handle->logsys, lfh->name);

		if (field == NULL)
		{
			tr_Log(TR_WARN_LOGFIELD_UNKNOWN, handle->path, lfh->name);
			++n;
			continue;
		}
		if (field->type != lfh->type)
		{
			tr_Log(TR_WARN_LOGFIELD_MISMATCH, handle->path, lfh->name, field->type);
			++n;
			continue;
		}
		lfh->logField = field;
	}
	return (n);
}

/*============================================================================
  Logsystem in handle->path is opened, and handle updated.
  If handle was already open, nothing is done.
  Errors are fatal.
============================================================================*/

int tr_localOpen(LogSysHandle *handle, char *path)
{
	char *pathcopy;
	int mode = 0;	/* default read/write */

	if (path == NULL) {
		tr_Fatal(TR_FATAL_NO_LOGSYS);
	}

	if (handle->path == NULL || strcmp(handle->path, path) != 0) {

		pathcopy = tr_strdup(path);

		if (handle->logsys) {
			logsys_close(handle->logsys);
			tr_free(handle->path);
		}
		if (handle->readOnly) {
			mode = LS_READONLY;
		}
		
		handle->path = pathcopy;
		handle->logsys = logsys_open(handle->path, LS_FAILOK | mode);

		if (handle->logsys == NULL) {
			tr_Fatal(TR_FATAL_LOGSYS_OPEN_FAILED, handle->path);
		}
		
		tr_localCompileFields(handle);

		/* manual transaction is ... manual 
		 * so by default OFF */
		handle->transactionMode = 0;

		/* IF   we are in autoflush mode OFF 
		 * AND  we are in autocommit mode ON
		 * THEN try to open/close a transaction */
		tr_localTryOpenCloseTransaction(handle);
	}
    return 0;
}

/*============================================================================
  Flush out an entry.
============================================================================*/

int tr_localWriteEntry(LogSysHandle *handle)
{
	int success;
	if (!handle->logEntry) {
		tr_Log(TR_WARN_LOGENTRY_NOT_SET);
		return (0);
	}
	success = (!logentry_write(handle->logEntry));

	/* IF   we are in autoflush mode OFF 
	 * AND  we are in autocommit mode ON
	 * THEN try to open/close a transaction */
	tr_localTryOpenCloseTransaction(handle);

	return success;
}

void tr_localTryOpenCloseTransaction(LogSysHandle *handle)
{
	/* only in autoflush mode OFF */
	if (handle->autoFlush == 1) {
		return;
	}
	
	/* AND only if autocommmit mode ON */
	if (handle->autoCommit == 0) {
		return;
	}
	
	/* close the pending SQL transaction if exists */
	tr_localCloseTransaction(handle);
	/* ... and (re)open a new one */
	tr_localOpenTransaction(handle);
}

void tr_localOpenTransaction(LogSysHandle *handle)
{
	/* open if closed/non opened */
	if (handle->transactionState == 0) 
	{
		logsys_begin_transaction(handle->logsys);
		handle->transactionState = 1;
	}
}

void tr_localCloseTransaction(LogSysHandle *handle)
{
	/* close if opened */
	if (handle->transactionState == 1)
	{
		logsys_end_transaction(handle->logsys);
		handle->transactionState = 0;
	}
}

void tr_localRollbackTransaction(LogSysHandle *handle)
{
	/* attempt rollback on an opened transac 
	 * rollback means also closing */
	if (handle->transactionState == 1)
	{
		logsys_rollback_transaction(handle->logsys);
		handle->transactionState = 0;
	}
}

void tr_localOpenManualTransaction(LogSysHandle *handle)
{
	/* manual transaction not allowed in autocommit mode ON */
	if (handle->autoCommit == 1) {
		return;
	}
	
	if (handle->transactionMode == 0)
	{
		tr_localOpenTransaction(handle);
		handle->transactionMode = 1;
	}
}

void tr_localCloseManualTransaction(LogSysHandle *handle)
{
	/* manual transaction not allowed in autocommit mode ON */
	if (handle->autoCommit == 1) {
		return;
	}
	
	if (handle->transactionMode == 1)
	{
		tr_localCloseTransaction(handle);
		handle->transactionMode = 0;
	}
}

void tr_localRollbackManualTransaction(LogSysHandle *handle)
{
	/* manual transaction not allowed in autocommit mode ON */
	if (handle->autoCommit == 1) {
		return;
	}
	
	if (handle->transactionMode == 1)
	{
		tr_localRollbackTransaction(handle);
		handle->transactionMode = 0;
	}
}

/*========================================================================
  Read a specific entry.
  Current active and entrylist are released.
========================================================================*/

int tr_localReadEntry(LogSysHandle *handle, double d_index)
{
	int idx = d_index;

	tr_localFreeEntryList(handle);
	tr_localOpen(handle, handle->path);

	handle->logEntry = logentry_readindex(handle->logsys, idx);

	return (!!handle->logEntry);	/* True if ok */
}

/*========================================================================
  if valid(LOGENTRY) then
   ...
========================================================================*/

int tr_localIsValidEntry(LogSysHandle *handle)
{
	return (handle && handle->logEntry);
}

/*
 * tr_localFetchParms(LogSysHandle *, char *ext)
 */

char **tr_localFetchParms(LogSysHandle *ls, char *ext)
{
	char **ta;
	char *path, buf[1024];
	FILE *fp;
	int idx=0, size=256, inc=256;

	/*
	 * Allocate initial array
	 */
	ta = calloc(size, sizeof(char *));
	if (!ta) {
		tr_OutOfMemory();  /* exit on error */
	}

	/*
	 * Generate path to file
	 */
	path = tr_lsPath(ls, ext);
	
	/*
	 * Read the file one line at a time, into a char ** array
	 * growing array if neccessary.
	 * The array should be freed using tr_lsFreeStringArray().
	 */
	fp = fopen(path, "r");
	if (!fp) {
		return NULL;
	}
	
	while (fgets(buf, sizeof(buf), fp)) {
		if (idx >= (size-1)) {
			ta = realloc(ta, ((size+inc)*sizeof(char*)));
			if (!ta) {
				fclose(fp);
				return NULL;
			}
 			size += inc;
		}
		ta[idx++] = strdup(buf);
	}
	ta[idx] = NULL; /* Make sure there is NULL last, to end the array */

	fclose(fp);

	return (ta);
}

int tr_localSetTextParm(LogSysHandle *pLog, 
                char *tExt,
                char *tName,
                char *tValue)
{
	setaparm(tr_lsPath(pLog, tExt), tName, tValue);
	return (1);
}

/*
 * XXX This assumes that name/value separator is '='
 */
char *tr_localGetTextParm( LogSysHandle *pLog,
               char     *tExt,
               char     *tName )
{
	char *path, line[8192], *s = NULL, *value, *cp;
	FILE *fp;
	int namelen;

	path = tr_lsPath(pLog, tExt);
	namelen = strlen(tName);

	fp = fopen(path, "r");
	if (!fp) {
		/*
		 * 3.15/KP : Return empty string (instead of NULL)
		 *
		 * return NULL;
		 */
		return (tr_strdup(""));
	}

	while (fgets(line, sizeof(line), fp)) {
		if (!strncmp(line, tName, namelen) && line[namelen] == '=') {
			value = line + namelen + 1;
			cp = value + strlen(value) - 1;
			while (*cp == '\r' || *cp == '\n') {
				*cp-- = 0;
			}
			
			s = tr_strdup(value);
			break;
		}
	}

	fclose(fp);

	if (s) {
		return (s);
	} else {
		return (tr_strdup(""));
	}
}

void tr_localClearParms(LogSysHandle *pLog, char *tExt)
{
	char *path;

	path = tr_lsPath(pLog, tExt);
	unlink(path);
}

/*
 * This is always a destructive write, i.e. all parameters are saved
 * and all previous contents are lost.
 */
int tr_localSaveParms(LogSysHandle *pLog, char *tExt, char **ta)
{
	FILE *fp;
	char *cp, *path, **vp;
	int dirfix = 1;

	path = tr_lsPath(pLog, tExt);

again:
	fp = fopen(path, "w");
	if (!fp) {
		/*
		 *  3.08/KP : New subdir ? Create it, if so.
		 */
		if (errno == ENOENT && dirfix && LOGSYS_EXT_IS_DIR) {
			dirfix = 0;
			if ((cp = strrchr(path, '/')) != NULL) {
				*cp = 0;
				mkdir(path, 0777);
				*cp = '/';
				goto again;
			}
			errno = ENOENT;
		}
		return (1);
	}

	for (vp = ta; ta && *vp; ++vp) {
		fprintf(fp, "%s\n", *vp);
	}
	
	fclose (fp);

	return (0);
}

/*
 * 3.14/KP
 * tr_getuniq() uses this one in local mode.
 *
 * NOTE: We use a separate lock file for locking purposes. This is due
 * the fact that the lock created by lock() function is released when
 * ANY file descriptor for oour process pointing to the locked file is
 * close()'s. Naturally this does not suit us, as getvalue()/setvalue()
 * pair performs its own open()/close()'s.
 */
int tr_localTryGetUniq(
    LogSysHandle *pLog,
    char *tExt,
    char *tCounter,
    int MinCounter,
    int MaxCounter,
    int *Counter_result)
{
	char *CounterFile;
	char CounterName[256], buf[256], CounterLock[1024]; 

#ifdef MACHINE_WNT
	int retries;
	DWORD nm;
	HANDLE h = INVALID_HANDLE_VALUE;
#else
	int n, fd = -1;
#endif

	*Counter_result = 0;


	/*
	 * Not much that we can do without the logentry.
	 */

	if (!pLog) {
		return 0;
	}

	CounterFile = tr_lsPath(pLog, "COUNTERS");
	sprintf(CounterLock, "%s.lock", CounterFile);

	/*
	 * Just in case.
	 */
	tr_DirFixup(CounterLock);

#ifdef MACHINE_WNT

#define CLOSE_COUNTER_FD if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

	if ((h = CreateFile(CounterLock,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL,
		NULL)) == INVALID_HANDLE_VALUE) {
		fprintf(stderr, "Cannot lock counter %s (%d)\n",
			CounterLock, GetLastError());
		goto out;
	}
	retries = 0;
retry:
	if (!LockFile(h, 0, 0, 1, 0)) {
		if (++retries <= 8) {
			Sleep(retries);
			goto retry;
		}
		fprintf(stderr, "Cannot lock counter %s (%d)\n",
			CounterLock, GetLastError());
		goto out;
	}


#else /* NT */

#define CLOSE_COUNTER_FD if (fd >= 0) close(fd);

	if ((fd = open(CounterLock, O_RDWR | O_CREAT, 0666)) == -1) {
		fprintf(stderr, "Cannot lock counter %s (%d)\n",
			CounterLock, errno);
		goto out;
	}
	if (lockf(fd, F_LOCK, 0)) {
		fprintf(stderr, "Cannot lock counter %s (%d)\n",
			CounterLock, errno);
		goto out;
	}
#endif

	/*
	 * Read in current counter value
	 */
	sprintf(CounterName, "%s.%s", tExt, tCounter);
	*Counter_result = atoi(tr_am_dbparmget(pLog, "COUNTERS", CounterName));

	if (*Counter_result < MinCounter || (*Counter_result > MaxCounter && MaxCounter > 0)) {
		*Counter_result = MinCounter;
	}
	
	/*
	 * ...and write a new one.
	 */

	sprintf(buf, "%d", *Counter_result + 1);
	tr_am_dbparmset(pLog, "COUNTERS", CounterName, buf);

#ifdef MACHINE_WNT

	if (!UnlockFile(h, 0, 0, 1, 0)) {
		fprintf(stderr, "Warning: unlocking counter %s (%d)\n",
			CounterLock, GetLastError());
	}
	
	if (!CloseHandle(h)) {
		fprintf(stderr, "Close failed with counter %s (%d)\n",
			CounterLock, GetLastError());
		goto out;
	}
	h = INVALID_HANDLE_VALUE;

#else /* NT */

	if (lockf(fd, F_ULOCK, 0)) {
		fprintf(stderr, "Warning: unlocking counter %s (%d)\n",
			CounterLock, errno);
	}
	if (close(fd)) {
		fprintf(stderr, "Close failed with counter %s (%d)\n",
			CounterLock, errno);
		goto out;
	}
	fd = -1;
#endif

out:
	CLOSE_COUNTER_FD;

	return (1);
}

static void setaparm(char *path, char *name, char *value)
{
	int namelen = strlen(name);
	FILE *fp;
	char *cp;
	char *buf;
	int bufsize;
	int fd;
	char oldparm[256];
	struct stat statbuf;
	
    /*
     * First try to stat file. If it exists, copy the whole file to memory
     * for further processing.
     */

	/* TX-2497 TCE(CG) code modification to avoid crash of transport.exe if coutner file not exists */
	int test = -1;

    fd = open(path, O_RDONLY, 0);
	if (fd != -1) {
		test = fstat(fd, &statbuf);
	}

	if (fd != -1 && test == 0) {
	/* END TX-2497 */
        int n = -1;

        bufsize = statbuf.st_size + 8192; /* should be more than enough */
        buf = calloc(1, bufsize);

        if (buf) {
            if ((n = read(fd, buf, bufsize)) > 0) {
                /*
                 * Now find all occurences of our param in buffer,
                 * and remove them.
                 */
                char *c1, *c2;
                buf[n] = 0; /* Set the terminating null character to buf */
                n = n+1;
                sprintf(oldparm, "%s=", name);
                c1 = buf;

                while ((c1 = strstr(c1, oldparm)) != NULL) {
                    /* Check that c1 really points to beginning of line,
                     * that is, either c1=buf (first line) or c1-- == \n
                     */

                    if ((c1 == buf) || ((c1 > buf) && (*(c1-1) == '\n'))) {
                        /*
                         * Now c1 points to beginning of the old param,
                         * set c2 to the terminating newline of that...
                         */
                        c2 = strchr(c1, '\n');

                        if (c2) {
							c2 = c2 + 1;
                    }
						/*TX-2606 YLI(CG) 17/04/2015 let c2 pointing beyond the end of line, if the end of line is not '/n'*/
						else{
							c2 = c1 + strlen(c1);
                    }
						/*END TX-2606*/;
						memcpy(c1, c2, (n - (int)(c2 - buf)));
                    }
                    else {
                        c1++;   /* Move forward, just in case we are stuck... */
                }
 
            }
            }

            close (fd);
#ifdef MACHINE_WNT
            fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, _S_IREAD | _S_IWRITE);
#else
            fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
#endif

            if (fd) {
                write(fd, buf, strlen(buf));
                close(fd);
            }
            free (buf);
            buf = NULL;
        }
		else {
			tr_OutOfMemory();  /* exit on error */
		}
    }

    {
        /*
         *  Create file if it did not exist, permissions default inside fopen() XXX
         *  No more content than the new namevalue.
         */
        int rv, retryopen = 1;
        char *cp, csave;

        while ((fp = fopen(path, "a")) == NULL) {
            if (errno == ENOENT && retryopen--) {

                if ((cp = strrchr(path, '/' )) != NULL
                 || (cp = strrchr(path, '\\')) != NULL) {
                    csave = *cp;
                    *cp = 0;
                    rv = mkdir(path, 0755);
                    *cp = csave;
                    if (rv == 0) {
                        continue;
                }
            }
            }
            return;
        }

        if (fp) {
            fprintf(fp, "%s=%s\n", name, value);
            fclose(fp);
        }
    }

}
