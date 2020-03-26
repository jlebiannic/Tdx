/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_logsysstub.c
	Programmer:	Kari Poutiainen (KP)
	Date:		Fri Mar 19 08:05:36 EET 1999

	Copyright Generix-Group 1990-2011


	Stub routines for logsystem handling. These call the
	actual functions in tr_logsystem.c or tr_logsysrem.c
	depending on the current operation mode.


	
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_logsysstub.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  4.00 19.03.99/KP	Created
  4.01 25.05.99/HT	tr_lsResetFile and tr_lsCloseFile added.
  4.02 18.08.99/JR	tr_lsCloseFile with two modes
  4.03 04.12.99/KP	Use tr_localTryGetUniq(), changed params for 
			tr_lsTryGetUniq()
========================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*
 * Currenlty define tr_localMode() as a macro, returning
 * value of tr_useLocalLogsys.
 */
int tr_localMode() 
{
	if (tr_errorLevel > 5.0)
		fprintf(stderr, "%s : %s mode.\n", tr_programName, tr_useLocalLogsys ? "local" : "remote");

	return (tr_useLocalLogsys);
}

#if 0
#define TR_WARN_FILE_WRITE_FAILED "Failed writing to file \"%s\" (%d)."
#define TR_WARN_FILE_READ_FAILED  "Failed reading from file \"%s\" (%d)."
#define TR_WARN_FILE_COPY_FAILED  "Failed copying \"%s\" to \"%s\" (%d)."

/*
 *  Only for filter-comparison
 */
#define OPERATOR_STRINGS

#include "../logsystem/remote/rls.h"

extern double atof();
#ifndef MACHINE_LINUX
extern int errno;
#endif

/*========================================================================
========================================================================*/

static LogSysHandle *ActiveLogSysHandles = (LogSysHandle *)NULL;
#endif

/*========================================================================
  Nobody is supposed to know the pathnames, and it would not
  even be usefull with remote bases !!!
========================================================================*/

char *tfBuild__LogSystemPath(LogSysHandle *log, char *ext)
{
	return (tr_lsPath(log, ext));
}

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
=========================================================================*/

int tr_lsFindFirst(LogSysHandle *handle, int order, LogFieldHandle *key, char *path, ...)
{
	va_list		ap;
	va_start(ap, path);

	if (tr_localMode()) 
		return tr_localVFindFirst(handle, order, key, path, ap);
	else
		return tr_rlsVFindFirst(handle, order, key, path, ap);
}

/*
 *  Pick one index from either a presorted list,
 *  or ask server for next match.
 *  The list is still checked (current record is set each time).
 */
int tr_lsFindNext(LogSysHandle *handle)
{
	if (tr_localMode()) 
		return tr_localFindNext(handle);
	else
		return tr_rlsFindNext(handle);
}

void tr_lsFreeEntryList(LogSysHandle *handle)
{
	if (tr_localMode()) 
		tr_localFreeEntryList(handle);
	else
		tr_rlsFreeEntryList(handle);
}

char *tr_lsGetTextVal(LogSysHandle *sh, LogFieldHandle *fh)
{ 
	if (tr_localMode())
		return tr_localGetTextVal(sh, fh);
	else
		return tr_rlsGetTextVal(sh, fh);
}

double tr_lsGetNumVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (tr_localMode())
		return tr_localGetNumVal(sh, fh);
	else
		return tr_rlsGetNumVal(sh, fh);
}

time_t tr_lsGetTimeVal(LogSysHandle *sh, LogFieldHandle *fh)
{
	if (tr_localMode())
		return tr_localGetTimeVal(sh, fh);
	else
		return tr_rlsGetTimeVal(sh, fh);
}

int tr_lsSetTextVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	if (tr_localMode())
		return tr_localSetTextVal(sh, fh, value);
	else
		return tr_rlsSetTextVal(sh, fh, value);
}

int tr_lsSetNumVal(LogSysHandle *sh, LogFieldHandle *fh, double value)
{
	if (tr_localMode())
		return tr_localSetNumVal(sh, fh, value);
	else
		return tr_rlsSetNumVal(sh, fh, value);
}

int tr_lsSetTimeVal(LogSysHandle *sh, LogFieldHandle *fh, char *value)
{
	if (tr_localMode())
		return tr_localSetTimeVal(sh, fh, value);
	else
		return tr_rlsSetTimeVal(sh, fh, value);
}

/*============================================================================
  Copy one entry into another.
============================================================================*/

int tr_lsCopyEntry(LogSysHandle *dst, LogSysHandle *src)
{
	if (tr_localMode())
		return tr_localCopyEntry(dst, src);
	else
		return tr_rlsCopyEntry(dst, src);
}

int tr_lsCreate(LogSysHandle *handle, char *path)
{
	if (tr_localMode())
		return tr_localCreate(handle, path);
	else
		return tr_rlsCreate(handle, path);
}

/*============================================================================
  Return the path of a specific file, extension.
  Returned string is temporarily saved in the string pool.
  Next GC will destroy it.
============================================================================*/

char *tr_lsPath (LogSysHandle *handle, char *extension)
{
	if (tr_localMode())
		return tr_localPath(handle, extension);
	else
		return tr_rlsPath(handle, extension);
}

/*============================================================================
  Logsystem in handle->path is opened, and handle updated.
  If handle was already open, nothing is done.
  Errors are fatal, TCL TRUE returned.
  NULL path might be a way to "close" a base (a variable, in fact),
  but it cannot be given from TCL. It could be "close(LOG)" instead...
============================================================================*/

int tr_lsOpen(LogSysHandle *handle, char *path)
{
	if (tr_localMode())
		return tr_localOpen(handle, path);
	else
		return tr_rlsOpen(handle, path);
}

/*============================================================================
  Remove the entry and everything related to it.
  Possible entrylist is NOT released, so we can remove entries
  within a TCL filter-loop.
============================================================================*/

int tr_lsRemove(LogSysHandle *handle)
{
	if (tr_localMode())
		return tr_localRemove(handle);
	else
		return tr_rlsRemove(handle);
}

/*========================================================================
  Read a specific entry.
  Current active and entrylist are released.
  XXX entrylist is NOT released, so we can use one TCL variable
  to walk thru a filter, and do other things inside the loop.
  Recursive loops do not work, however.
========================================================================*/

int tr_lsReadEntry(LogSysHandle *handle, double d_idx)
{
	if (tr_localMode())
		return tr_localReadEntry(handle, d_idx);
	else
		return tr_rlsReadEntry(handle, d_idx);
}

/*============================================================================
  Flush out an entry, here this means to ask remote server
  to really write it to disk. Otherwise the writing
  is done by the server every now and then.
============================================================================*/

int tr_lsWriteEntry(LogSysHandle *handle)
{
	if (tr_localMode())
		return tr_localWriteEntry(handle);
	else
		return tr_rlsWriteEntry(handle);
}

/*
 *  Return TCL TRUE iff all looks right,
 *  check the connection too.
 */
int tr_lsValidEntry(LogSysHandle *handle)
{
	if (tr_localMode())
		return tr_localIsValidEntry(handle);
	else
		return tr_rlsIsValidEntry(handle);
}

/*
 *  Return 0 if fs-file, 1 if remote & exists and -1 if not.
 *  If result is NULL, this is only 'access()' call,
 *  and no error is logged.
 */
int tr_lsTryStatFile(char *pseudoname, struct stat *st_result)
{
	if (tr_localMode())
		return 0;
	else
		return tr_rlsTryStatFile(pseudoname, st_result);
}

int tr_lsTryOpenFile(char *pseudoname, char *mode, FILE **fp_result)
{
	if (tr_localMode())
		return 0;
	else
		return tr_rlsTryOpenFile(pseudoname, mode, fp_result);
}

int tr_lsTryRemoveFile(char *uglypath, int *ok_result)
{
	if (tr_localMode())
		return 0;
	else
		return tr_rlsTryRemoveFile(uglypath, ok_result);
}

int tr_lsTryCopyFile(char *source, char *destination, int *ok_result)
{
	if (tr_localMode())
		return 0;
	else
		return tr_rlsTryCopyFile(source, destination, ok_result);
}

/*
 * 4.03/KP : Changed params...
 */
int tr_lsTryGetUniq(LogSysHandle *pLog, char *tFile, char *tCounter, int MinCounter, int MaxCounter, int *Counter_result)
{
	if (tr_localMode())
		/*
		 * 4.03/KP
		 * was: "return (0);"
		 */
		return tr_localTryGetUniq(pLog, tFile, tCounter, MinCounter, MaxCounter, Counter_result);
	else
		return tr_rlsTryGetUniq(pLog, tFile, tCounter, MinCounter, MaxCounter, Counter_result);
}

char **tr_lsFetchParms(LogSysHandle *ls, char *ext)
{
	if (tr_localMode())
		return tr_localFetchParms(ls, ext);
	else
		return tr_rlsFetchParms(ls, ext);
}

void tr_lsFreeStringArray(char **ta)
{
	char **vp;

	for(vp = ta; ta && *vp; ++vp)
		free(*vp);

	free (ta);
}

int tr_lsSetTextParm(LogSysHandle *pLog, char *tExt, char *tName, char *tValue)
{
	if (tr_localMode())
		return tr_localSetTextParm(pLog, tExt, tName, tValue);
	else
		return tr_rlsSetTextParm(pLog, tExt, tName, tValue);
}

char *tr_lsGetTextParm( LogSysHandle *pLog, char *tExt, char *tName )
{
	if (tr_localMode())
		return tr_localGetTextParm(pLog, tExt, tName);
	else
		return tr_rlsGetTextParm(pLog, tExt, tName);
}

void tr_lsClearParms(LogSysHandle *pLog, char *tExt)
{
	if (tr_localMode())
		tr_localClearParms(pLog, tExt);
	else
		tr_rlsClearParms(pLog, tExt);
}

int tr_lsSaveParms(LogSysHandle *pLog, char *tExt, char **ta)
{
	if (tr_localMode())
		return tr_localSaveParms(pLog, tExt, ta);
	else
		return tr_rlsSaveParms(pLog, tExt, ta);
}

void tr_lsResetFile( FILE *fp )
{
	if (!tr_localMode())
		tr_rlsResetFile( fp );
	return;
}

int tr_lsCloseFile( FILE *fp, char mode )
{
	int rv = 0;

	/* 4.02/JR two modes (0 == fclose is done, else not) */
	if(mode == 0){
		/* default (fclose is done) */
		if (!tr_localMode()){
			rv = tr_rlsCloseFile( fp );
			if(rv >= 0) rv = fclose( fp );
		}
		else rv = fclose(fp);
	}
	else{
		/* redirect (no fclose) */
		if (!tr_localMode())
			return( tr_rlsCloseFile( fp ) );
	}
	return( rv );
}

void tr_lsOpenManualTransaction(LogSysHandle *handle)
{
	if (tr_localMode())
	{
		tr_localOpenManualTransaction(handle);
	}
}

void tr_lsCloseManualTransaction(LogSysHandle *handle)
{
	if (tr_localMode())
	{
		tr_localCloseManualTransaction(handle);
	}
}

void tr_lsRollbackManualTransaction(LogSysHandle *handle)
{
	if (tr_localMode())
	{
		tr_localRollbackManualTransaction(handle);
	}
}
