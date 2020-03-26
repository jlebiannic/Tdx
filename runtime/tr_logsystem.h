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

/*
 * General stubs for the functions. Only these should be used normally.
 */
int		tr_localMode();

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
int tr_lsTryStatFile(char *pseudoname, struct stat *st_result);
int tr_lsTryOpenFile(char *pseudoname, char *mode, FILE **fp_result);
int tr_lsTryRemoveFile(char *uglypath, int *ok_result);
int tr_lsTryCopyFile(char *source, char *destination, int *ok_result);
char **tr_lsFetchParms(LogSysHandle *ls, char *ext);
void tr_lsFreeStringArray(char **ta);
int tr_lsSetTextParm(LogSysHandle *, char *, char *, char *);
char *tr_lsGetTextParm(LogSysHandle *, char *, char *);
void tr_lsClearParms(LogSysHandle *, char *);
int tr_lsSaveParms(LogSysHandle *pLog, char *tExt, char **ta);

int tr_lsTryGetUniq(LogSysHandle *, char *, char *, int, int, int *);

void tr_lsResetFile( FILE *fp );
int tr_lsCloseFile( FILE *fp, char );

void tr_lsOpenManualTransaction(LogSysHandle *handle);
void tr_lsCloseManualTransaction(LogSysHandle *handle);
void tr_lsRollbackManualTransaction(LogSysHandle *handle);

/*
 * "Local" logsystem versions
 */
char *tr_localPath (LogSysHandle *handle, char *extension);
int tr_localFindFirst (LogSysHandle *, int, LogFieldHandle *, char *, ...);
int tr_localVFindFirst(LogSysHandle *, int, LogFieldHandle *, char *, va_list);
int tr_localFindNext(LogSysHandle *);
void tr_localFreeEntryList(LogSysHandle *);
char *tr_localGetTextVal(LogSysHandle *, LogFieldHandle *);
double tr_localGetNumVal(LogSysHandle *, LogFieldHandle *);
time_t tr_localGetTimeVal(LogSysHandle *, LogFieldHandle *);
int tr_localSetTextVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_localSetNumVal(LogSysHandle *, LogFieldHandle *, double);
int tr_localSetTimeVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_localCopyEntry(LogSysHandle *, LogSysHandle *);
int tr_localCreate(LogSysHandle *, char *);
int tr_localOpen(LogSysHandle *, char *);
int tr_localRemove(LogSysHandle *);
int tr_localReadEntry(LogSysHandle *, double);
int tr_localWriteEntry(LogSysHandle *);
int tr_localValidEntry(LogSysHandle *);
char **tr_localFetchParms(LogSysHandle *ls, char *ext);
int tr_localSetTextParm(LogSysHandle *, char *, char *, char *);
char *tr_localGetTextParm(LogSysHandle *, char *, char *);
void tr_localClearParms(LogSysHandle *, char *);
int tr_localSaveParms(LogSysHandle *pLog, char *tExt, char **ta);
int tr_localTryGetUniq(LogSysHandle *,char *, char *, int, int, int *);
int tr_localIsValidEntry(LogSysHandle *handle);


void tr_localResetFile( FILE *fp );
int tr_localCloseFile( FILE *fp );

void tr_localTryOpenCloseTransaction(LogSysHandle *handle);
void tr_localOpenManualTransaction(LogSysHandle *handle);
void tr_localCloseManualTransaction(LogSysHandle *handle);
void tr_localRollbackManualTransaction(LogSysHandle *handle);
void tr_localOpenTransaction(LogSysHandle *handle);
void tr_localCloseTransaction(LogSysHandle *handle);
void tr_localRollbackTransaction(LogSysHandle *handle);

/*
 * Remote ones.
 */
int tr_lsDecodeLoc(char *, PRLSPATH );
void tr_lsReleaseRlsPath( PRLSPATH );

char *tr_rlsPath (LogSysHandle *handle, char *extension);
int tr_rlsFindFirst (LogSysHandle *, int, LogFieldHandle *, char *, ...);
int tr_rlsVFindFirst(LogSysHandle *, int, LogFieldHandle *, char *, va_list);
int tr_rlsFindNext(LogSysHandle *);
void tr_rlsFreeEntryList(LogSysHandle *);
char *tr_rlsGetTextVal(LogSysHandle *, LogFieldHandle *);
double tr_rlsGetNumVal(LogSysHandle *, LogFieldHandle *);
time_t tr_rlsGetTimeVal(LogSysHandle *, LogFieldHandle *);
int tr_rlsSetTextVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_rlsSetNumVal(LogSysHandle *, LogFieldHandle *, double);
int tr_rlsSetTimeVal(LogSysHandle *, LogFieldHandle *, char *);
int tr_rlsCopyEntry(LogSysHandle *, LogSysHandle *);
int tr_rlsCreate(LogSysHandle *, char *);
int tr_rlsOpen(LogSysHandle *, char *);
int tr_rlsRemove(LogSysHandle *);
int tr_rlsReadEntry(LogSysHandle *, double);
int tr_rlsWriteEntry(LogSysHandle *);
int tr_rlsValidEntry(LogSysHandle *);
char **tr_rlsFetchParms(LogSysHandle *ls, char *ext);
int tr_rlsSetTextParm(LogSysHandle *, char *, char *, char *);
char *tr_rlsGetTextParm(LogSysHandle *, char *, char *);
void tr_rlsClearParms(LogSysHandle *, char *);
int tr_rlsSaveParms(LogSysHandle *pLog, char *tExt, char **ta);

int tr_rlsRemoteName(char *);
int tr_rlsTryStatFile(char *, struct stat *);
int tr_rlsTryOpenFile(char *, char *, FILE **);
int tr_rlsTryRemoveFile(char *, int *);
int tr_rlsTryCopyFile(char *, char *, int *);
char * tr_PrettyPrintFileName(char *);

int tr_rlsTryGetUniq(LogSysHandle *,char *, char *, int, int, int *);

void tr_rlsResetFile( FILE *fp );
int tr_rlsCloseFile( FILE *fp );
int tr_rlsDecodeLoc(char *pseudoname, PRLSPATH rlspath);
void tr_rlsReleaseRlsPath( PRLSPATH rlspath );
int tr_rlsIsValidEntry(LogSysHandle *handle);
#endif
