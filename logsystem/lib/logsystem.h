/*========================================================================
        E D I S E R V E R

        File:		logsystem.h
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1997 Telecom Finland/EDI Operations
==========================================================================
  @(#)TradeXpress $Id: logsystem.h 55060 2019-07-18 08:59:51Z sodifrance $
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 02.05.95/JN	Pseudo field _OWNER for managerlog.
  3.02 15.09.97/JN	Couple of pointers added to LS struct.
   .   20.03.00/KP	Added logoperatr_*() functions
   .   06.04.00/KP	logoperator_write() had incorrect arguments. Sorry.
  4.00 05.07.02/CD      added column separator in LogFilter and PrintFormat.
  4.0? 22.09.04/JMH     suppress the logd cache (Xentries) from the logsystem struct
                        thus cache is not shared anymore by rls client
       22.04.2011/CMA   add new interface (logentry_getbyfield)  for eSignature CGI script
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation

========================================================================*/
#ifndef _LOGSYSTEM_LOGSYSTEM_H
#define _LOGSYSTEM_LOGSYSTEM_H

#include "logsystem.definitions.h"
#include "runtime/tr_constants.h"

/* BugZ_9833 */
/*----------------------------------------------------------------------------------------------*/
/* cm 07/13/2010 */
#define TYPE_LOGSYSTEM 'S'
#define TYPE_LOGENTRY  'E'

/* data is either logsystem* or logentry* */
extern void logsys_dump(void* log_data, char log_type, char* msg, char* __file__, int __line__, char* from); /* defined in io.c */
/*----------------------------------------------------------------------------------------------*/

PrintFormat*		alloc_printformat			(void);
TimeStamp			log_curtime					(void);
void*				log_malloc					(int size);
char*				log_strdup					(char* s);
int 				logdata_reconfig			(LogLabel *oldLabel,LogSystem *log);
void				logentry_copyfields			(LogEntry* oldEntry,LogEntry* newEntry);
int					logentry_copyfiles			(LogEntry* src,LogEntry* dst);
int					logentry_destroy			(LogEntry* entry);
void				logentry_free				(LogEntry* entry);
LogIndex			logentry_getindex			(LogEntry* entry);
Integer				logentry_getintegerbyfield	(LogEntry* entry,LogField* field);
Number				logentry_getnumberbyfield	(LogEntry* entry,LogField* field);
char*				logentry_gettextbyfield		(LogEntry* entry,LogField* field);
TimeStamp			logentry_gettimebyfield		(LogEntry* entry,LogField* field);
LogEntry*			logentry_new				(LogSystem* log);
void				logentry_operate			(LogEntry* entry,LogOperator* op);
void				logentry_printbyformat		(FILE* fp,LogEntry* entry,PrintFormat* format);
LogEntry*			logentry_readindex			(LogSystem* log,LogIndex index);
int					logentry_remove				(LogSystem* log,LogIndex index,char* extension);
int					logentry_removefiles		(LogEntry* entry,char* extension);
void				logentry_setbyfield			(LogEntry* entry,LogField* field,char* str);
void                logentry_setindexbyname		(LogEntry *entry, char* name, LogIndex value);
void				logentry_setintegerbyfield	(LogEntry* entry,LogField* field,Integer value);
void                logentry_setintegerbyname	(LogEntry *entry, char* name, Integer value);
void				logentry_setnumberbyfield	(LogEntry* entry,LogField* field,Number value);
void                logentry_setnumberbyname	(LogEntry *entry, char* name, Number value);
void				logentry_settextbyfield		(LogEntry* entry, LogField* field, char* str);
void                logentry_settextbyname		(LogEntry *entry, char* name, char* value);
void				logentry_settimebyfield		(LogEntry* entry, LogField* field, TimeStamp value);
void                logentry_settimebyname		(LogEntry *entry, char* name, TimeStamp value);
/* BugZ_9996: added for eSignature'CGI script */
/*
 * returns 1 if field found else 0.
 *         if field found, value and type are returned.
 *         do not freed o_data and o_type !
 */
int                 logentry_getbyname			(LogEntry *i_entry, char* i_name, void** o_data, int *o_type);
int					logentry_write				(LogEntry* entry);
int					logentry_writeraw			(LogEntry* entry);
void				logfilter_add				(LogFilter** filters, char* filterStr);
void				logfilter_addparam			(LogFilter** filters, PrintFormat** pfp, char* param);
void				logfilter_clear				(LogFilter* filter);
void				logfilter_free				(LogFilter* filter);
int 				logfilter_getmaxcount		(LogFilter* filter);
void				logfilter_insert			(LogFilter **lfp, char *name, int cmp, char *value);
int					logfilter_printformat_read	(LogFilter **lfp, PrintFormat **pfp, FILE *fp);
int					logfilter_read				(LogFilter** filters,FILE* fp);
void				logfilter_setkey			(LogFilter** filters, char* key);
void				logfilter_setmaxcount		(LogFilter** filters,int maxcount);
void				logfilter_setoffset			(LogFilter** filters, int offset);
void				logfilter_setorder			(LogFilter** filters, int order);
void				logfilter_setseparator		(LogFilter** filters,char* separator);
char**				logfilter_textual			(LogFilter* filter);
char**				logfilter_textualparam		(LogFilter* filter);
void				logfilter_textual_free		(char** values);
void				logheader_printbyformat		(FILE* fp,PrintFormat* format);
void				logheader_printbyformat_env	(FILE* fp,PrintFormat* format);
int					loglabel_checkaffinities	(LogSystem* log);
int 				loglabel_compareconfig		(LogLabel* oldLabel,LogLabel* newLabel);
LogField*			loglabel_getfield			(LogLabel* label, char* name);
void				logoperator_add				(LogOperator** operators,char* str);
void				logoperator_free			(LogOperator* op);
int					logoperator_integer			(Integer* value,OperatorRecord* op);
int					logoperator_read			(LogOperator** operators,FILE* file);
int					logoperator_number			(Number* value,OperatorRecord* op);
int					logoperator_text			(char* text,OperatorRecord* op);
int					logoperator_time			(TimeStamp* time,OperatorRecord* op);
int					logoperator_unknown			(void);
LogSystem*			logsys_alloc				(char* sysname);
int 				logsys_begin_transaction	(LogSystem* log);
void				logsys_close				(LogSystem* log);
void				logsys_compability_setup	(void);
int					logsys_compilefilter		(LogSystem* log, LogFilter* filter);
int					logsys_compileoperator		(LogSystem* log,LogOperator* op);
int					logsys_compileprintform		(LogSystem* log,PrintFormat* format);
PrintFormat*		logsys_defaultformat		(LogSystem* log);
void				logsys_dumplabelfields		(LogLabel* label);
int 				logsys_end_transaction		(LogSystem* log);
int					logsys_entry_count			(LogSystem* log, LogFilter* filter);
char*				logsys_filepath				(LogSystem* log,char* filename);
void				logsys_free					(LogSystem* log);
LogField*			logsys_getfield				(LogSystem* log, char* name);
LogIndex*			logsys_list_indexed			(LogSystem* log, LogFilter* filter);
LogEntry**			logsys_makelist				(LogSystem* log,LogFilter* filter);
PrintFormat*		logsys_namevalueformat		(LogSystem* log);
/* BugZ_9996: added for eSignature'CGI script */
/*
 * set global according to db type (sqlite or legacy)
 */
void                logsys_setdbtype();
LogSystem*			logsys_open					(char* filename, int mode);
int					logsys_openfile				(LogSystem *log, char *basename, int flags, int mode);
/* BugZ_9996: added for eSignature'CGI script */
/*
 * returns 0 if data base is open else -1.
 */
int                 logsys_opendata				(LogSystem *log);
void				logsys_operatebyfilter		(LogSystem* log,LogFilter* filter,LogOperator* op);
LogLabel*			logsys_readconfig			(char* filename, int onRebuild);
int					logsys_readlabel			(LogSystem *log);
int					logsys_refresh				(LogSystem* log);
int					logsys_removebyfilter		(LogSystem* log,LogFilter* filter,char* extension);
void				logsys_resetindex			(char* basename);
int 				logsys_rollback_transaction	(LogSystem* logsys);
void				printformat_add				(PrintFormat** format,char* str);
void				printformat_insert			(PrintFormat** formats,char* name,char* format);
int					printformat_read			(PrintFormat** formats,FILE* fp);
char*				syserr_string				(int error);

/* use in old legacy only, may be removed with */
int					logindex_passesfilter		(LogIndex index, LogFilter* filter);
int					logentry_passesfilter		(LogEntry* entry, LogFilter* filter);
LogEntry*			logentry_readraw			(LogSystem* log,LogIndex index);
LogEntry**			logsys_makelist_indexed		(LogSystem* log, LogFilter* filter);
int					tr_SQLiteMode();
/*JRE 06.15 BG-81*/
LogIndexEnv*		logsys_list_indexed_env		(LogSystem *log,LogFilter *lf);
LogEntryEnv*		logentry_readindex_env		(LogSystem *log,LogIndexEnv idx);
void				logentry_printbyform_env	(FILE *fp,LogEntryEnv *entry,PrintFormat *pf);
LogIndexEnv			logentry_getindex_env		(LogEntryEnv *entry);
int					logsys_affect_sysname		(LogSystem *log, LogEnv lsenv);
LogEnv				logenv_read					(FILE *fp);
/*End JRE*/

void 				writeLog					(int level, char *fmt, ...);

int					mbsncpy						(char* to, char* from, size_t sz);
int					mbsncpypad					(char* to, char* from, size_t size, char padding);
int 				log_UseLocaleUTF8			();
#endif
