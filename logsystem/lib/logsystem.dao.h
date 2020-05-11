/*========================================================================
        TradeXpress

        File:		logsystem.dao.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright Generix-Group 1990-2012

==========================================================================
  @(#)TradeXpress $Id: logsystem.dao.h 55415 2020-03-04 18:11:43Z jlebiannic $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifndef _LOGSYSTEM_LOGSYSTEM_DAO_H
#define _LOGSYSTEM_LOGSYSTEM_DAO_H

#include <stdio.h>
#include "logsystem.definitions.h"

#define LSFILE_SQLITE    "/.sqlite"
#define LSFILE_SQLITE_STARTUP    "/.sqlite_startup"

#ifdef MACHINE_MIPS
#define PROTO(x) ()
#else
#define PROTO(x) x
#endif

PrintFormat*		dao_alloc_printformat			PROTO(());
void				dao_bail_out						PROTO((char*,...));
void				dao_debug_logsys_warning			PROTO((LogSystem *ls,char *fmt,...));
int					dao_docopy						PROTO((LogEntry *src, char *srcpath, LogEntry *dst, char *dstpath));
LogField*			dao_loglabel_getfield					PROTO((LogLabel *label,char *name));
int					dao_logsys_defcleanup			PROTO((LogSystem *log));
TimeStamp			dao_log_curtime					PROTO((void));
void*				dao_log_malloc					PROTO((int size));
void*				dao_log_realloc					PROTO((void *old,int size));
char*				dao_log_str3dup					PROTO((char *a,char *b,char *c));
char*				dao_log_strdup					PROTO((char *s));
void				dao_log_strncat					PROTO((char *txt,char *add,int size));
void				dao_log_strncpy					PROTO((char *to,char *from,int size));
int 				dao_logdata_reconfig				PROTO((LogLabel *oldLabel,LogSystem *log));
int					dao_logentry_addprog				PROTO((LogEntry*));
LogEntry*			dao_logentry_alloc				PROTO((LogSystem *log,LogIndex idx));
LogEntry*			dao_logentry_alloc_skipclear		PROTO((LogSystem *log,LogIndex idx));
void				dao_logentry_copyfields			PROTO((LogEntry *oldent,LogEntry *newent));
int					dao_logentry_copyfiles			PROTO((LogEntry *src,LogEntry *dst));
void				dao_logentry_created				PROTO((LogEntry*));
int					dao_logentry_destroy				PROTO((LogEntry *entry));
void				dao_logentry_free				PROTO((LogEntry *entry));
LogIndex			dao_logentry_getindex			PROTO((LogEntry *entry));
Integer				dao_logentry_getintegerbyfield	PROTO((LogEntry*,LogField*));
Number				dao_logentry_getnumberbyfield	PROTO((LogEntry*,LogField*));
char*				dao_logentry_gettextbyfield		PROTO((LogEntry *entry,LogField *f));
TimeStamp			dao_logentry_gettimebyfield		PROTO((LogEntry*,LogField*));
LogEntry*			dao_logentry_new					PROTO((LogSystem*));
void				dao_logentry_operate				PROTO((LogEntry *entry,LogOperator *o));
void				dao_logentry_printbyformat		PROTO((FILE *fp,LogEntry *entry,PrintFormat *pf));
LogEntry*			dao_logentry_readindex			PROTO((LogSystem *log,LogIndex idx));
LogEntry*			dao_logentry_readraw				PROTO((LogSystem *log,LogIndex idx));
int					dao_logentry_remove				PROTO((LogSystem *log,LogIndex idx,char *extension));
int					dao_logentry_removefiles			PROTO((LogEntry *entry,char *extension));
int					dao_logentry_removeprog			PROTO((LogEntry*));
void				dao_logentry_setbyfield			PROTO((LogEntry *entry,LogField *f,char *s));
void				dao_logentry_setintegerbyfield	PROTO((LogEntry*,LogField*,Integer));
void				dao_logentry_setnumberbyfield	PROTO((LogEntry*,LogField*,Number));
void				dao_logentry_settextbyfield		PROTO((LogEntry *entry,LogField *f,char *value));
void				dao_logentry_settimebyfield		PROTO((LogEntry*,LogField*,TimeStamp));
void                dao_logentry_setindexbyname		(LogEntry *entry, char* name, LogIndex value);
void                dao_logentry_setintegerbyname	(LogEntry *entry, char* name, Integer value);
void                dao_logentry_setnumberbyname		(LogEntry *entry, char* name, Number value);
void                dao_logentry_settextbyname		(LogEntry *entry, char* name, char* value);
void                dao_logentry_settimebyname		(LogEntry *entry, char* name, TimeStamp value);
/* BugZ_9996: added for eSignature'CGI script */
int                 dao_logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type);
int					dao_logentry_write				PROTO((LogEntry *entry));
char*				dao_logfield_typename			PROTO((int type));
void				dao_logfilter_add				PROTO((LogFilter **lfp,char *namevalue));
void				dao_logfilter_addparam			PROTO((LogFilter **lfp,PrintFormat **pfp,char *namevalue));
void				dao_logfilter_clear				PROTO((LogFilter *lf));
void				dao_logfilter_free				PROTO((LogFilter *lf));
int 				dao_logfilter_getmaxcount		PROTO((LogFilter *lf));
void				dao_logfilter_insert				PROTO((LogFilter **lfp,char *name,int cmp,char *value));
int					dao_logfilter_printformat_read	PROTO((LogFilter **lfp,PrintFormat **pfp,FILE *fp));
int					dao_logfilter_read				PROTO((LogFilter **lfp,FILE *fp));
void				dao_logfilter_setkey				PROTO((LogFilter **lfp,char *key));
void				dao_logfilter_setmaxcount		PROTO((LogFilter **lfp,int maxcount));
void				dao_logfilter_setoffset			PROTO((LogFilter **lfp,int offset));
void				dao_logfilter_setorder			PROTO((LogFilter **lfp,int order));
void				dao_logfilter_setseparator		PROTO((LogFilter **lfp,char *sep));
char**				dao_logfilter_textual			PROTO((LogFilter *lf));
char**				dao_logfilter_textualparam		PROTO((LogFilter *lf));
void				dao_logfilter_textual_free		PROTO((char **v));
void				dao_logheader_printbyformat		PROTO((FILE *fp,PrintFormat *pf));
void 				dao_logheader_printbyformat_env	PROTO((FILE *fp,PrintFormat *pf));
LogLabel*			dao_loglabel_alloc				PROTO((int size));
int					dao_loglabel_checkaffinities		PROTO((LogSystem* log));
int 				dao_loglabel_compareconfig		PROTO((LogLabel *oldLabel,LogLabel *newLabel));
LogField*			dao_loglabel_getfield			PROTO((LogLabel*,char*));
void				dao_logoperator_add				PROTO((LogOperator **lop,char *namevalue));
void				dao_logoperator_free				PROTO((LogOperator *lo));
int					dao_logoperator_integer			PROTO((Integer*,OperatorRecord*));
int					dao_logoperator_number			PROTO((Number*,OperatorRecord*));
int					dao_logoperator_read				PROTO((LogOperator**,FILE*));
int					dao_logoperator_text				PROTO((char*,OperatorRecord*));
int					dao_logoperator_time				PROTO((TimeStamp*,OperatorRecord*));
int					dao_logoperator_unknown			PROTO(());
LogSystem*			dao_logsys_alloc					PROTO((char*));
LogLabel*			dao_logsys_arrangefields			PROTO((LogField*,int));
int 				dao_logsys_begin_transaction		PROTO((LogSystem*));
int					dao_logsys_cleanupprog			PROTO((LogSystem*));
void				dao_logsys_close					PROTO((LogSystem*));
void				dao_logsys_compability_setup		PROTO(());
int					dao_logsys_compilefilter			PROTO((LogSystem*,LogFilter*));
int					dao_logsys_compileoperator		PROTO((LogSystem*,LogOperator*));
int					dao_logsys_compileprintform		PROTO((LogSystem*,PrintFormat*));
int					dao_logsys_copyconfig			PROTO((char*,char*));
void				dao_logsys_createdirs			PROTO((char*));
void				dao_logsys_createlabel			PROTO((LogSystem*));
PrintFormat*		dao_logsys_defaultformat			PROTO((LogSystem*));
void				dao_logsys_dumplabel				PROTO((LogLabel*));
void				dao_logsys_dumplabelfields		PROTO((LogLabel*));
void				dao_logsys_dumplabeltypes		PROTO((LogLabel*));
int 				dao_logsys_end_transaction		PROTO((LogSystem*));
int 				dao_logsys_entry_count			PROTO((LogSystem*,LogFilter*));
char*				dao_logsys_filepath				PROTO((LogSystem*,char*));
void				dao_logsys_free					PROTO((LogSystem*));
LogField*			dao_logsys_getfield				PROTO((LogSystem*,char*));
LogIndex*			dao_logsys_list_indexed			PROTO((LogSystem*,LogFilter*));
LogEntry**			dao_logsys_makelist				PROTO((LogSystem*,LogFilter*));
PrintFormat*		dao_logsys_namevalueformat		PROTO((LogSystem*));
LogSystem*			dao_logsys_open					PROTO((char*,int));
int					dao_logsys_openfile				PROTO((LogSystem*,char*,int,int));
void				dao_logsys_operatebyfilter		PROTO((LogSystem*,LogFilter*,LogOperator*));
LogLabel*			dao_logsys_readconfig			PROTO((char*,int));
int					dao_logsys_readlabel				PROTO((LogSystem *log));
int					dao_logsys_removebyfilter		PROTO((LogSystem*,LogFilter*,char*));
void				dao_logsys_resetindex			PROTO((char*));
int 				dao_logsys_rollback_transaction	PROTO((LogSystem*));
int					dao_logsys_runprog				PROTO((LogSystem*,char*,LogIndex));
int					dao_logsys_thresholdprog			PROTO((LogSystem*));
void				dao_logsys_trace					PROTO((LogSystem*,char*,...));
void				dao_logsys_voidclose				PROTO((int));
void				dao_logsys_warning				PROTO((LogSystem*,char*,...));
void				dao_printformat_add				PROTO((PrintFormat**,char*));
void				dao_printformat_insert			PROTO((PrintFormat**,char*,char*));
int					dao_printformat_read				PROTO((PrintFormat**,FILE*));
char*				dao_syserr_string				PROTO((int));

/* 
 * functions that calls sqlite3 lib method directly 
 */
#define MAX_INT_DIGITS 10

int					log_daoreadbufsfiltered			PROTO((LogSystem *log,LogIndex **pIndexes, LogFilter *lf));
int					log_daoreadbuf					PROTO((LogEntry *entry));
int					log_daowritebuf					PROTO((LogEntry *entry,int rawmode));
LogIndex			logentry_dao_new					PROTO((LogSystem *));
int					logentry_dao_destroy				PROTO((LogEntry *entry));
int					logsys_dao_entry_count			PROTO((LogSystem *log, LogFilter *lf));
#if 0
int					logsys_dao_lastindex				PROTO((LogSystem *));
#endif
/* trigger method for external program (cleanup, remove, add, threshold) */
int					logsys_trigger_setup				PROTO((LogSystem *log,char **pErrMsg));
/* create a .sqlite file for a given logsystem */
int					logsys_createdata_dao			PROTO((LogSystem *log,int mode));
int 				dao_logsys_sync_label			PROTO((LogSystem *log));
int                 dao_get_removed_entries_count(LogSystem *log);
/* read the <db>/.sqlite_startup and exec sql request */
void                dao_exec_at_open(LogSystem* log);
/* add a column to the existing base */
int log_daocolumnadd(LogSystem *log,LogField* column);
/*JRE 06.15 BG-81*/
LogEntryEnv*		dao_logentry_alloc_env			PROTO((LogSystem *log,LogIndexEnv idx));
void				dao_logentryenv_free				PROTO((LogEntryEnv *entry));
LogIndexEnv			dao_logentry_getindex_env		PROTO((LogEntryEnv *entry));
void				dao_logentry_printbyform_env	PROTO((FILE *fp,LogEntryEnv *entry,PrintFormat *pf));
LogEntryEnv*		dao_logentry_readindex_env		PROTO((LogSystem *log,LogIndexEnv idx));
LogIndexEnv*		dao_logsys_list_indexed_env		PROTO((LogSystem *log,LogFilter *lf));
int					log_daoreadbufsfiltermultenv	PROTO((LogSystem *log,LogIndexEnv **pIndexes, LogFilter *lf));
int					log_daoreadbuf_env				PROTO((LogEntryEnv *entry));
int					logsys_dao_entry_count_env		PROTO((LogSystem *log, LogFilter *lf));
int					dao_logsys_affect_sysname		PROTO((LogSystem *log, LogEnv lsenv));
LogEnv				dao_logenv_read					PROTO((FILE *fp));
int 				log_dao_attach_database_env		PROTO((LogSystem *log));
int					dao_logsys_opendata				PROTO((LogSystem *log));
/*End JRE*/
int					dao_logentry_writeraw			(LogEntry* entry);
#undef PROTO
#endif
