/*========================================================================
        E D I S E R V E R

        File:		private.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

==========================================================================
  @(#)TradeXpress $Id: private.old_legacy.h 55239 2019-11-19 13:50:31Z sodifrance $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <errno.h>
#include <time.h>

#ifdef MACHINE_WNT
#include <io.h>
#include <ctype.h>
#endif

#ifndef MACHINE_MIPS
#include <stdlib.h>
#endif

#ifndef MACHINE_HPUX
extern int errno;
#endif

#include "port.old_legacy.h"

#define PROTO(x) x

extern void bail_out		PROTO((char *, ...));
#define old_legacy_bail_out bail_out

#if 0
void logsys_trace	PROTO((LogSystem *, char *, ...));
void logsys_warning	PROTO((LogSystem *, char *, ...));
#endif

int  old_legacy_logsys_docleanup	PROTO((LogSystem *));
int  old_legacy_logsys_defcleanup	PROTO((LogSystem *));

void old_legacy_logsys_trace PROTO((LogSystem *ls, char *fmt, ...)); 

void old_legacy_logentry_created	PROTO((LogEntry *));
void old_legacy_logentry_free	PROTO((LogEntry *));

TimeStamp old_legacy_log_curtime	PROTO((void));

int  old_legacy_logentry_addprog		PROTO((LogEntry *));
int  old_legacy_logentry_removeprog	PROTO((LogEntry *));
int  old_legacy_logsys_thresholdprog	PROTO((LogSystem *));
int  old_legacy_logsys_cleanupprog		PROTO((LogSystem *));
int  old_legacy_logsys_runprog	PROTO((LogSystem *, char *prog, LogIndex));

void old_legacy_logentry_purgehints	PROTO((LogEntry *));

char *old_legacy_logfield_typename		PROTO((int));

void old_legacy_logsys_dumplabelfields	PROTO((LogLabel *));
void old_legacy_logsys_dumplabeltypes	PROTO((LogLabel *));
void old_legacy_logsys_dumplabel		PROTO((LogLabel *));
void old_legacy_logsys_createdirs		PROTO((char *sysname));
int  old_legacy_logsys_copyconfig		PROTO((char *filename, char *sysname));
void old_legacy_logsys_createlabel		PROTO((LogSystem *));
void old_legacy_logsys_createdata		PROTO((LogSystem *));
void old_legacy_logsys_createdatasqlite		PROTO((LogSystem *));
void old_legacy_logsys_createhints		PROTO((LogSystem *, int count, char *file));

int  old_legacy_logsys_openfile	PROTO((LogSystem *, char *file, int omode, int amode));

int  old_legacy_logsys_readmap	PROTO((LogSystem *));
int  old_legacy_logsys_writemap	PROTO((LogSystem *));
void old_legacy_logsys_releasemap	PROTO((LogSystem *));
int  old_legacy_logsys_refreshmap	PROTO((LogSystem *));

void old_legacy_logsys_dumpmap		PROTO((LogMap *));
void old_legacy_logsys_createmap		PROTO((LogSystem *));

void old_legacy_logsys_warning PROTO((LogSystem *ls, char *fmt, ...));
void old_legacy_logentry_modified	PROTO((LogEntry *entry));
int old_legacy_logsys_was_opened PROTO((void *logsys));
int old_legacy_loglabel_free	PROTO((LogLabel *lab));
void old_legacy_logsys_blocksignals	PROTO(());
void old_legacy_logsys_restoresignals	PROTO(());

int sqlite_logsys_cleanup_lock_nonblock PROTO((LogSystem *log));
int old_legacy_logsys_cleanup_lock_nonblock PROTO((LogSystem *log));
int old_legacy_logsys_cleanup_lock PROTO((LogSystem *log));
int old_legacy_logsys_cleanup_unlock PROTO((LogSystem *log));
void old_legacy_logsys_rewind PROTO((LogSystem *log));
int old_legacy_logsys_sortentries PROTO((LogEntry **entries, int num_entries, LogFilter *lf));
int old_legacy_logentry_readraw_clustered PROTO((LogSystem *log, LogIndex idx, int count, LogEntry **entries));
int old_legacy_logsys_fillfield PROTO((LogField *field, char *string));
int old_legacy_logsys_sortfieldnames PROTO((LogLabel *label));
void old_legacy_logsys_signalsforchild PROTO(());
int old_legacy_logentry_printfield PROTO((FILE *fp, LogEntry *entry, LogField *f, char *fmt));
void old_legacy_logentry_setindexbyname PROTO((LogEntry *entry, char *name, LogIndex value));
int old_legacy_logsys_opendata  PROTO((LogSystem *log));
int old_legacy_logentry_printbynames PROTO((FILE *fp, LogEntry *entry, char **names));
void old_legacy_logentry_setbyname  PROTO((LogEntry *entry, char *name, char *s));
void old_legacy_logsys_freehints PROTO((LogHints *hints));
int old_legacy_logsys_printheaderbynames PROTO((FILE *fp, LogSystem *log, char **names));
void old_legacy_logsys_dumpheader PROTO((LogSystem *log));
void old_legacy_logentry_dump PROTO((LogEntry *entry));
int old_legacy_logentry_movefiles PROTO((LogEntry *entry, LogIndex newidx));
void old_legacy_logentry_setvalue PROTO((LogEntry *entry, LogField *f, void *addr));
void* old_legacy_logentry_getvalue PROTO((LogEntry *entry, LogField *field));
int old_legacy_log_writebuf PROTO((int fd, char *buf, int nbytes, int offset));

