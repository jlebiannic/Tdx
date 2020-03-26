/*========================================================================
        TradeXpress

        File:		logsystem.sqlite.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright Generix-Group 1990-2012

        This file is taken from logsystem.h and has been restricted
        to the sqlite case.
==========================================================================
  @(#)TradeXpress $Id: logsystem.sqlite.h 55415 2020-03-04 18:11:43Z jlebiannic $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifndef _LOGSYSTEM_LOGSYSTEM_SQLITE_H
#define _LOGSYSTEM_LOGSYSTEM_SQLITE_H

#include <stdio.h>
#include "logsystem.definitions.h"

#define LSFILE_SQLITE    "/.sqlite"
#define LSFILE_SQLITE_STARTUP    "/.sqlite_startup"

#ifdef MACHINE_MIPS
#define PROTO(x) ()
#else
#define PROTO(x) x
#endif

PrintFormat*		sqlite_alloc_printformat			PROTO(());
void				sqlite_bail_out						PROTO((char*,...));
void				sqlite_debug_logsys_warning			PROTO((LogSystem *ls,char *fmt,...));
int					sqlite_docopy						PROTO((LogEntry *src, char *srcpath, LogEntry *dst, char *dstpath));
LogField*			sqlite_findfield					PROTO((LogLabel *label,char *name));
int					sqlite_logsys_defcleanup			PROTO((LogSystem *log));
TimeStamp			sqlite_log_curtime					PROTO((void));
void*				sqlite_log_malloc					PROTO((int size));
void*				sqlite_log_realloc					PROTO((void *old,int size));
char*				sqlite_log_str3dup					PROTO((char *a,char *b,char *c));
char*				sqlite_log_strdup					PROTO((char *s));
void				sqlite_log_strncat					PROTO((char *txt,char *add,int size));
void				sqlite_log_strncpy					PROTO((char *to,char *from,int size));
int 				sqlite_logdata_reconfig				PROTO((LogLabel *oldLabel,LogSystem *log));
int					sqlite_logentry_addprog				PROTO((LogEntry*));
LogEntry*			sqlite_logentry_alloc				PROTO((LogSystem *log,LogIndex idx));
LogEntry*			sqlite_logentry_alloc_skipclear		PROTO((LogSystem *log,LogIndex idx));
void				sqlite_logentry_copyfields			PROTO((LogEntry *oldent,LogEntry *newent));
int					sqlite_logentry_copyfiles			PROTO((LogEntry *src,LogEntry *dst));
void				sqlite_logentry_created				PROTO((LogEntry*));
int					sqlite_logentry_destroy				PROTO((LogEntry *entry));
void				sqlite_logentry_free				PROTO((LogEntry *entry));
LogIndex			sqlite_logentry_getindex			PROTO((LogEntry *entry));
Integer				sqlite_logentry_getintegerbyfield	PROTO((LogEntry*,LogField*));
Number				sqlite_logentry_getnumberbyfield	PROTO((LogEntry*,LogField*));
char*				sqlite_logentry_gettextbyfield		PROTO((LogEntry *entry,LogField *f));
TimeStamp			sqlite_logentry_gettimebyfield		PROTO((LogEntry*,LogField*));
LogEntry*			sqlite_logentry_new					PROTO((LogSystem*));
void				sqlite_logentry_operate				PROTO((LogEntry *entry,LogOperator *o));
void				sqlite_logentry_printbyformat		PROTO((FILE *fp,LogEntry *entry,PrintFormat *pf));
LogEntry*			sqlite_logentry_readindex			PROTO((LogSystem *log,LogIndex idx));
LogEntry*			sqlite_logentry_readraw				PROTO((LogSystem *log,LogIndex idx));
int					sqlite_logentry_remove				PROTO((LogSystem *log,LogIndex idx,char *extension));
int					sqlite_logentry_removefiles			PROTO((LogEntry *entry,char *extension));
int					sqlite_logentry_removeprog			PROTO((LogEntry*));
void				sqlite_logentry_setbyfield			PROTO((LogEntry *entry,LogField *f,char *s));
void				sqlite_logentry_setintegerbyfield	PROTO((LogEntry*,LogField*,Integer));
void				sqlite_logentry_setnumberbyfield	PROTO((LogEntry*,LogField*,Number));
void				sqlite_logentry_settextbyfield		PROTO((LogEntry *entry,LogField *f,char *value));
void				sqlite_logentry_settimebyfield		PROTO((LogEntry*,LogField*,TimeStamp));
void                sqlite_logentry_setindexbyname		(LogEntry *entry, char* name, LogIndex value);
void                sqlite_logentry_setintegerbyname	(LogEntry *entry, char* name, Integer value);
void                sqlite_logentry_setnumberbyname		(LogEntry *entry, char* name, Number value);
void                sqlite_logentry_settextbyname		(LogEntry *entry, char* name, char* value);
void                sqlite_logentry_settimebyname		(LogEntry *entry, char* name, TimeStamp value);
/* BugZ_9996: added for eSignature'CGI script */
int                 sqlite_logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type);
int					sqlite_logentry_write				PROTO((LogEntry *entry));
char*				sqlite_logfield_typename			PROTO((int type));
void				sqlite_logfilter_add				PROTO((LogFilter **lfp,char *namevalue));
void				sqlite_logfilter_addparam			PROTO((LogFilter **lfp,PrintFormat **pfp,char *namevalue));
void				sqlite_logfilter_clear				PROTO((LogFilter *lf));
void				sqlite_logfilter_free				PROTO((LogFilter *lf));
int 				sqlite_logfilter_getmaxcount		PROTO((LogFilter *lf));
void				sqlite_logfilter_insert				PROTO((LogFilter **lfp,char *name,int cmp,char *value));
int					sqlite_logfilter_printformat_read	PROTO((LogFilter **lfp,PrintFormat **pfp,FILE *fp));
int					sqlite_logfilter_read				PROTO((LogFilter **lfp,FILE *fp));
void				sqlite_logfilter_setkey				PROTO((LogFilter **lfp,char *key));
void				sqlite_logfilter_setmaxcount		PROTO((LogFilter **lfp,int maxcount));
void				sqlite_logfilter_setoffset			PROTO((LogFilter **lfp,int offset));
void				sqlite_logfilter_setorder			PROTO((LogFilter **lfp,int order));
void				sqlite_logfilter_setseparator		PROTO((LogFilter **lfp,char *sep));
char**				sqlite_logfilter_textual			PROTO((LogFilter *lf));
char**				sqlite_logfilter_textualparam		PROTO((LogFilter *lf));
void				sqlite_logfilter_textual_free		PROTO((char **v));
void				sqlite_logheader_printbyformat		PROTO((FILE *fp,PrintFormat *pf));
void 				sqlite_logheader_printbyformat_env	PROTO((FILE *fp,PrintFormat *pf));
LogLabel*			sqlite_loglabel_alloc				PROTO((int size));
int					sqlite_loglabel_checkaffinities		PROTO((LogSystem* log));
int 				sqlite_loglabel_compareconfig		PROTO((LogLabel *oldLabel,LogLabel *newLabel));
LogField*			sqlite_loglabel_getfield			PROTO((LogLabel*,char*));
void				sqlite_logoperator_add				PROTO((LogOperator **lop,char *namevalue));
void				sqlite_logoperator_free				PROTO((LogOperator *lo));
int					sqlite_logoperator_integer			PROTO((Integer*,OperatorRecord*));
int					sqlite_logoperator_number			PROTO((Number*,OperatorRecord*));
int					sqlite_logoperator_read				PROTO((LogOperator**,FILE*));
int					sqlite_logoperator_text				PROTO((char*,OperatorRecord*));
int					sqlite_logoperator_time				PROTO((TimeStamp*,OperatorRecord*));
int					sqlite_logoperator_unknown			PROTO(());
LogSystem*			sqlite_logsys_alloc					PROTO((char*));
LogLabel*			sqlite_logsys_arrangefields			PROTO((LogField*,int));
int 				sqlite_logsys_begin_transaction		PROTO((LogSystem*));
int					sqlite_logsys_cleanupprog			PROTO((LogSystem*));
void				sqlite_logsys_close					PROTO((LogSystem*));
void				sqlite_logsys_compability_setup		PROTO(());
int					sqlite_logsys_compilefilter			PROTO((LogSystem*,LogFilter*));
int					sqlite_logsys_compileoperator		PROTO((LogSystem*,LogOperator*));
int					sqlite_logsys_compileprintform		PROTO((LogSystem*,PrintFormat*));
int					sqlite_logsys_copyconfig			PROTO((char*,char*));
void				sqlite_logsys_createdirs			PROTO((char*));
void				sqlite_logsys_createlabel			PROTO((LogSystem*));
PrintFormat*		sqlite_logsys_defaultformat			PROTO((LogSystem*));
void				sqlite_logsys_dumplabel				PROTO((LogLabel*));
void				sqlite_logsys_dumplabelfields		PROTO((LogLabel*));
void				sqlite_logsys_dumplabeltypes		PROTO((LogLabel*));
int 				sqlite_logsys_end_transaction		PROTO((LogSystem*));
int 				sqlite_logsys_entry_count			PROTO((LogSystem*,LogFilter*));
char*				sqlite_logsys_filepath				PROTO((LogSystem*,char*));
void				sqlite_logsys_free					PROTO((LogSystem*));
LogField*			sqlite_logsys_getfield				PROTO((LogSystem*,char*));
LogIndex*			sqlite_logsys_list_indexed			PROTO((LogSystem*,LogFilter*));
LogEntry**			sqlite_logsys_makelist				PROTO((LogSystem*,LogFilter*));
PrintFormat*		sqlite_logsys_namevalueformat		PROTO((LogSystem*));
LogSystem*			sqlite_logsys_open					PROTO((char*,int));
int					sqlite_logsys_openfile				PROTO((LogSystem*,char*,int,int));
void				sqlite_logsys_operatebyfilter		PROTO((LogSystem*,LogFilter*,LogOperator*));
LogLabel*			sqlite_logsys_readconfig			PROTO((char*,int));
int					sqlite_logsys_readlabel				PROTO((LogSystem *log));
int					sqlite_logsys_removebyfilter		PROTO((LogSystem*,LogFilter*,char*));
void				sqlite_logsys_resetindex			PROTO((char*));
int 				sqlite_logsys_rollback_transaction	PROTO((LogSystem*));
int					sqlite_logsys_runprog				PROTO((LogSystem*,char*,LogIndex));
int					sqlite_logsys_thresholdprog			PROTO((LogSystem*));
void				sqlite_logsys_trace					PROTO((LogSystem*,char*,...));
void				sqlite_logsys_voidclose				PROTO((int));
void				sqlite_logsys_warning				PROTO((LogSystem*,char*,...));
void				sqlite_printformat_add				PROTO((PrintFormat**,char*));
void				sqlite_printformat_insert			PROTO((PrintFormat**,char*,char*));
int					sqlite_printformat_read				PROTO((PrintFormat**,FILE*));
char*				sqlite_syserr_string				PROTO((int));

/* 
 * functions that calls sqlite3 lib method directly 
 */
#define MAX_INT_DIGITS 10

int					log_sqlitereadbufsfiltered			PROTO((LogSystem *log,LogIndex **pIndexes, LogFilter *lf));
int					log_sqlitereadbuf					PROTO((LogEntry *entry));
int					log_sqlitewritebuf					PROTO((LogEntry *entry,int rawmode));
LogIndex			logentry_sqlite_new					PROTO((LogSystem *));
int					logentry_sqlite_destroy				PROTO((LogEntry *entry));
int					logsys_sqlite_entry_count			PROTO((LogSystem *log, LogFilter *lf));
#if 0
int					logsys_sqlite_lastindex				PROTO((LogSystem *));
#endif
/* trigger method for external program (cleanup, remove, add, threshold) */
int					logsys_trigger_setup				PROTO((LogSystem *log,char **pErrMsg));
/* create a .sqlite file for a given logsystem */
int					logsys_createdata_sqlite			PROTO((LogSystem *log,int mode));
int 				sqlite_logsys_sync_label			PROTO((LogSystem *log));
int                 sqlite_get_removed_entries_count(LogSystem *log);
/* read the <db>/.sqlite_startup and exec sql request */
void                sqlite_exec_at_open(LogSystem* log);
/* add a column to the existing base */
int log_sqlitecolumnadd(LogSystem *log,LogField* column);
/*JRE 06.15 BG-81*/
LogEntryEnv*		sqlite_logentry_alloc_env			PROTO((LogSystem *log,LogIndexEnv idx));
void				sqlite_logentryenv_free				PROTO((LogEntryEnv *entry));
LogIndexEnv			sqlite_logentry_getindex_env		PROTO((LogEntryEnv *entry));
void				sqlite_logentry_printbyform_env	PROTO((FILE *fp,LogEntryEnv *entry,PrintFormat *pf));
LogEntryEnv*		sqlite_logentry_readindex_env		PROTO((LogSystem *log,LogIndexEnv idx));
LogIndexEnv*		sqlite_logsys_list_indexed_env		PROTO((LogSystem *log,LogFilter *lf));
int					log_sqlitereadbufsfiltermultenv	PROTO((LogSystem *log,LogIndexEnv **pIndexes, LogFilter *lf));
int					log_sqlitereadbuf_env				PROTO((LogEntryEnv *entry));
int					logsys_sqlite_entry_count_env		PROTO((LogSystem *log, LogFilter *lf));
int					sqlite_logsys_affect_sysname		PROTO((LogSystem *log, LogEnv lsenv));
LogEnv				sqlite_logenv_read					PROTO((FILE *fp));
int 				log_sqlite_attach_database_env		PROTO((LogSystem *log));
int					sqlite_logsys_opendata				PROTO((LogSystem *log));
/*End JRE*/

#undef PROTO
#endif
