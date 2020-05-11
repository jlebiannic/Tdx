/*========================================================================
        E D I S E R V E R

        File:		tr_logsystem.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

	Logsystem interface structures in TCL.

        Copyright (c) 1995 Telecom Finland/EDI Operations
==========================================================================
	Record all changes here and update the above string accordingly.
	3.00 03.10.94/JN	Created.
	3.01 04.12.99/KP	Changed tr_lsTryGetUniq() parameters
========================================================================*/

#ifndef _TR_LOGSYSTEM_H
#define _TR_LOGSYSTEM_H

#include <stdio.h>
#include <sys/stat.h>
#include <stdarg.h>

/*
 *  This structure is for decoding pseudo filepath names
 *  10.09.98/HT
 */

typedef struct rlspath {
	long            idx;
	char            *ext;
	char            *location;
	unsigned        is_rls:1,
			:0;
	} RLSPATH, *PRLSPATH;


typedef struct LogFieldHandle LogFieldHandle;
typedef struct LogSysHandle LogSysHandle;

struct LogFieldHandle {
	char *name;		/* Field name */
	int  type;		/* Type during compilation */
	void *next;		/* Next field used by logentry */
	void *logField;		/* Real data for the field */
};

struct LogSysHandle {
	LogFieldHandle *fields;	/* Field handles used by this entry */
	int   autoFlush;	/* Entry written immediately after a change */
	int   readOnly;		/* System is not modified */

	int   autoCommit;	/* use transaction with flushing (commit on flush) */
	int   transactionMode; /* 0: automatic, 1:manual */
	int   transactionState; /* 0: closed, 1:opened */

	/* These are zero at startup. */
	char  *path;		/* Path of the logsystem */
	void  *logsys;		/* Real logsystem data */
	void  *filter;		/* Filter used with find() */
	void  *logEntry;	/* Direct pointer to current logEntry */
	int   *indexList;	/* Entries that matched the last search with sqlite method */
	void **entryList;	/* Entries that matched the last search with old legacy method */
	int   listIndex;	/* Next pick index in the above list */
	void  *rfu1;
	void  *rfu2;            /* available as necessary */
	LogSysHandle *next_logsyshandle;
};

char *tfBuild__LogSystemPath(LogSysHandle *, char *);

FILE *tr_lsOpenFile(char *, char *);

char *tr_lsPath (LogSysHandle *handle, char *extension);
int tr_lsFindFirst (LogSysHandle *, int, LogFieldHandle *, char *, ...);
int tr_lsVFindFirst(LogSysHandle *, int, LogFieldHandle *, char *, va_list);
int tr_lsFindNext(LogSysHandle *);
void tr_lsFreeEntryList(LogSysHandle *);
char *tr_lsGetTextVal(LogSysHandle *, LogFieldHandle *);
double tr_lsGetNumVal(LogSysHandle *, LogFieldHandle *);
time_t tr_lsGetTimeVal(LogSysHandle *, LogFieldHandle *);
int tr_lsSetTextVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_lsSetNumVal(LogSysHandle *, LogFieldHandle *, double);
int tr_lsSetTimeVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_lsCopyEntry(LogSysHandle *, LogSysHandle *);
int tr_lsCreate(LogSysHandle *, char *);
int tr_lsOpen(LogSysHandle *, char *);
int tr_lsRemove(LogSysHandle *);
int tr_lsReadEntry(LogSysHandle *, double);
int tr_lsWriteEntry(LogSysHandle *);
int tr_lsValidEntry(LogSysHandle *);
char **tr_lsFetchParms(LogSysHandle *ls, char *ext);
void tr_lsFreeStringArray(char **ta);
int tr_lsSetTextParm(LogSysHandle *, char *, char *, char *);
char *tr_lsGetTextParm(LogSysHandle *, char *, char *);
void tr_lsClearParms(LogSysHandle *, char *);
int tr_lsSaveParms(LogSysHandle *pLog, char *tExt, char **ta);
int tr_lsTryGetUniq(LogSysHandle *, char *, char *, int, int, int *);
int tr_lsCloseFile( FILE *fp, char );
void tr_lsOpenManualTransaction(LogSysHandle *handle);
void tr_lsCloseManualTransaction(LogSysHandle *handle);
void tr_lsRollbackManualTransaction(LogSysHandle *handle);
void tr_lsTryOpenCloseTransaction(LogSysHandle *handle);
void tr_lsOpenTransaction(LogSysHandle *handle);
void tr_lsCloseTransaction(LogSysHandle *handle);

/*
 * "Local" logsystem versions
 */


#endif
