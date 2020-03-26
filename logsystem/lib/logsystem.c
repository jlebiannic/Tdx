#include "conf/local_config.h"
/*========================================================================
        TradeXpress

        File:		logsystem.sqlite.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright (c) 2006 Illicom groupe Influe

        This file is the interface between the interface of logsystem/lib
        ("the outside")
        and the sqlite vs old_legacy parts of logsystem/lib
        ("the inside")
        It is where we say whether we use sqlite or old legacy
        according to the tr_useSQLiteLogsys 
        (setted in modules using logsystem/lib)
==========================================================================
  @(#)TradeXpress $Id: logsystem.c 55239 2019-11-19 13:50:31Z sodifrance $ $Name:  $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

/* this is the only file where you should those three included */
#include "private.h"
#include "logsystem.h"
#include "logsystem.sqlite.h"
#include "old_legacy/logsystem.old_legacy.h"
#include "old_legacy/private.old_legacy.h"


/* Set in .tclrc, SQLite ou old_legacy */
int	tr_useSQLiteLogsys;

/* are we migrating from old legacy to sqlite ? 
 * by default : no */
int logsys_migrate= 0;

char *liblogsystem_version = "@(#)TradeXpress $Id: logsystem.c 55239 2019-11-19 13:50:31Z sodifrance $";
int liblogsystem_version_n = 3014;

extern char    *tr_programName;
extern double	tr_errorLevel;

int tr_SQLiteMode() 
{
	if (tr_errorLevel > 5.0)
		fprintf(stderr, "%s : %s mode.\n", tr_programName, tr_useSQLiteLogsys ? "SQLite" : "old_legacy");

	return (tr_useSQLiteLogsys);
}

#define EXECSQLITEOROLD(action) if (tr_SQLiteMode()) { sqlite_##action; } else { old_legacy_##action; }
#define EXECSQLITEOROLDRET(action) if (tr_SQLiteMode()) { return (sqlite_##action); } else { return (old_legacy_##action); }

PrintFormat* alloc_printformat()
{
    EXECSQLITEOROLDRET(alloc_printformat())
}

TimeStamp old_legacy_log_curtime();
TimeStamp log_curtime()
{
    EXECSQLITEOROLDRET(log_curtime())
}

void* log_malloc(int size)
{
    EXECSQLITEOROLDRET(log_malloc(size))
}

char * log_strdup(char *s)
{
    EXECSQLITEOROLDRET(log_strdup(s))
}

#define old_legacy_logdata_reconfig(a,b)	0
int logdata_reconfig(LogLabel *oldLabel,LogSystem *log)
{
    EXECSQLITEOROLDRET(logdata_reconfig(oldLabel,log))
}

void logentry_copyfields(LogEntry* oldEntry,LogEntry* newEntry)
{
    EXECSQLITEOROLD(logentry_copyfields(oldEntry,newEntry))
}

int old_legacy_logentry_copyfiles(LogEntry* src,LogEntry* dst);
int logentry_copyfiles(LogEntry* src,LogEntry* dst)
{
    EXECSQLITEOROLDRET(logentry_copyfiles(src,dst))
}

int logentry_destroy(LogEntry* entry)
{
    EXECSQLITEOROLDRET(logentry_destroy(entry))
}

void old_legacy_logentry_free(LogEntry* entry);
void logentry_free(LogEntry* entry)
{
    EXECSQLITEOROLD(logentry_free(entry))
}

LogIndex logentry_getindex(LogEntry* entry)
{
    EXECSQLITEOROLDRET(logentry_getindex(entry))
}

Integer logentry_getintegerbyfield(LogEntry* entry, LogField* field)
{
    EXECSQLITEOROLDRET(logentry_getintegerbyfield(entry, field))
}

Number logentry_getnumberbyfield(LogEntry* entry, LogField* field)
{
    EXECSQLITEOROLDRET(logentry_getnumberbyfield(entry,field))
}

char* logentry_gettextbyfield(LogEntry* entry, LogField* field)
{
    EXECSQLITEOROLDRET(logentry_gettextbyfield(entry,field))
}

TimeStamp logentry_gettimebyfield(LogEntry* entry, LogField* field)
{
    EXECSQLITEOROLDRET(logentry_gettimebyfield(entry,field))
}

LogEntry* logentry_new(LogSystem* log)
{
    EXECSQLITEOROLDRET(logentry_new(log))
}

void logentry_operate(LogEntry* entry, LogOperator* operator)
{
    EXECSQLITEOROLD(logentry_operate(entry,operator))
}

void old_legacy_logentry_printbyformat(FILE* fp,LogEntry* entry,PrintFormat* format);
void logentry_printbyformat(FILE* fp,LogEntry* entry,PrintFormat* format)
{
    EXECSQLITEOROLD(logentry_printbyformat(fp,entry,format))
}

LogEntry* logentry_readindex(LogSystem* log, LogIndex index)
{
    EXECSQLITEOROLDRET(logentry_readindex(log,index))
}

LogEntry* logentry_readraw(LogSystem* log,LogIndex index)
{
    EXECSQLITEOROLDRET(logentry_readraw(log,index))
}

int logentry_remove(LogSystem* log, LogIndex index, char* extension)
{
    EXECSQLITEOROLDRET(logentry_remove(log,index,extension))
}

int logentry_removefiles(LogEntry* entry, char* extension)
{
    EXECSQLITEOROLDRET(logentry_removefiles(entry,extension))
}

void old_legacy_logentry_setbyfield(LogEntry* entry, LogField* field, char* str);
void logentry_setbyfield(LogEntry* entry, LogField* field, char* str)
{
    EXECSQLITEOROLD(logentry_setbyfield(entry,field,str))
}

void logentry_setintegerbyfield(LogEntry* entry, LogField* field, Integer value)
{
    EXECSQLITEOROLD(logentry_setintegerbyfield(entry,field,value))
}

void logentry_setnumberbyfield(LogEntry* entry, LogField* field, Number value)
{
    EXECSQLITEOROLD(logentry_setnumberbyfield(entry,field,value))
}

void logentry_settextbyfield(LogEntry* entry, LogField* field, char* str)
{
    EXECSQLITEOROLD(logentry_settextbyfield(entry,field,str))
}

void logentry_settimebyfield(LogEntry* entry, LogField* field, TimeStamp value)
{
    EXECSQLITEOROLD(logentry_settimebyfield(entry,field,value))
}

void logentry_setindexbyname(LogEntry *entry, char* name, LogIndex value)
{
    EXECSQLITEOROLD(logentry_setindexbyname(entry,name,value))
}

void logentry_setintegerbyname(LogEntry *entry, char* name, Integer value)
{
    EXECSQLITEOROLD(logentry_setintegerbyname(entry,name,value))
}

void logentry_setnumberbyname(LogEntry *entry, char* name, Number value)
{
    EXECSQLITEOROLD(logentry_setnumberbyname(entry,name,value))
}

void logentry_settextbyname(LogEntry *entry, char* name, char* value)
{
    EXECSQLITEOROLD(logentry_settextbyname(entry,name,value))
}

void logentry_settimebyname(LogEntry *entry, char* name, TimeStamp value)
{
    EXECSQLITEOROLD(logentry_settimebyname(entry,name,value))
}

/* BugZ_9996: added for eSignature'CGI script */
/*
 * returns 1 if field found else 0.
 *         if field found, value and type are returned.
 *         do not freed o_data and o_type !
 */
int logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type)
{
   EXECSQLITEOROLD(logentry_getbyname(i_entry, i_name, o_data, o_type))
}

int logentry_write(LogEntry* entry)
{
    EXECSQLITEOROLDRET(logentry_write(entry))
}

int sqlite_logentry_writeraw(LogEntry *logentry);
int logentry_writeraw(LogEntry* entry)
{
    EXECSQLITEOROLDRET(logentry_writeraw(entry))
}

void logfilter_add(LogFilter** filters, char* filterStr)
{
    EXECSQLITEOROLD(logfilter_add(filters,filterStr))
}

void logfilter_addparam(LogFilter** filters, PrintFormat** pfp, char* param)
{
    EXECSQLITEOROLD(logfilter_addparam(filters,pfp,param))
}

void logfilter_clear(LogFilter* filter)
{
    EXECSQLITEOROLD(logfilter_clear(filter))
}

void logfilter_free(LogFilter* filter)
{
    EXECSQLITEOROLD(logfilter_free(filter))
}

int logfilter_getmaxcount(LogFilter* filter)
{
    EXECSQLITEOROLDRET(logfilter_getmaxcount(filter))
}

void logfilter_insert(LogFilter **lfp, char *name, int cmp, char *value)
{
    EXECSQLITEOROLD(logfilter_insert(lfp,name,cmp,value))
}

int logfilter_printformat_read(LogFilter **lfp, PrintFormat **pfp, FILE *fp)
{
    EXECSQLITEOROLDRET(logfilter_printformat_read(lfp,pfp,fp))
}

int logfilter_read(LogFilter** filters,FILE* fp)
{
    EXECSQLITEOROLDRET(logfilter_read(filters,fp))
}

void logfilter_setkey(LogFilter** filters, char* key)
{
    EXECSQLITEOROLD(logfilter_setkey(filters,key))
}

void logfilter_setmaxcount(LogFilter** filters,int maxcount)
{
    EXECSQLITEOROLD(logfilter_setmaxcount(filters,maxcount))
}

void logfilter_setoffset(LogFilter** filters, int offset)
{
    EXECSQLITEOROLD(logfilter_setoffset(filters,offset))
}

void logfilter_setorder(LogFilter** filters, int order)
{
    EXECSQLITEOROLD(logfilter_setorder(filters,order))
}

void logfilter_setseparator(LogFilter** filters,char* separator)
{
    EXECSQLITEOROLD(logfilter_setseparator(filters,separator))
}

char** logfilter_textual(LogFilter* filter)
{
    EXECSQLITEOROLDRET(logfilter_textual(filter))
}

char** logfilter_textualparam(LogFilter* filter)
{
    EXECSQLITEOROLDRET(logfilter_textualparam(filter))
}

void logfilter_textual_free(char** values)
{
    EXECSQLITEOROLD(logfilter_textual_free(values))
}

#define old_legacy_logheader_printbyformat_env(a,b) 0
void logheader_printbyformat_env(FILE* fp,PrintFormat* format)
{
    EXECSQLITEOROLD(logheader_printbyformat_env(fp,format))
}

void logheader_printbyformat(FILE* fp,PrintFormat* format)
{
    EXECSQLITEOROLD(logheader_printbyformat(fp,format))
}

#define old_legacy_loglabel_checkaffinities(a) 0
int loglabel_checkaffinities(LogSystem* log)
{
    EXECSQLITEOROLDRET(loglabel_checkaffinities(log))
}

#define old_legacy_loglabel_compareconfig(a,b) -1
int loglabel_compareconfig(LogLabel* oldLabel,LogLabel* newLabel)
{
    EXECSQLITEOROLDRET(loglabel_compareconfig(oldLabel,newLabel))
}

LogField* loglabel_getfield(LogLabel* label, char* name)
{
    if (tr_SQLiteMode() == 0)
    {
        return old_legacy_loglabel_getfield(label,name);
    }
    else
        return sqlite_findfield(label,name);
}

void logoperator_add(LogOperator** operators,char* str)
{
    EXECSQLITEOROLD(logoperator_add(operators,str))
}

void logoperator_free(LogOperator* operator)
{
    EXECSQLITEOROLD(logoperator_free(operator))
}

int logoperator_integer(Integer* value,OperatorRecord* operator)
{
    EXECSQLITEOROLDRET(logoperator_integer(value,operator))
}

int logoperator_number(Number* value,OperatorRecord* operator)
{
    EXECSQLITEOROLDRET(logoperator_number(value,operator))
}

int logoperator_read(LogOperator** operators,FILE* file)
{
    EXECSQLITEOROLDRET(logoperator_read(operators,file))
}

int logoperator_text(char* text,OperatorRecord* operator)
{
    EXECSQLITEOROLDRET(logoperator_text(text,operator))
}

int logoperator_time(TimeStamp* time,OperatorRecord* operator)
{
    EXECSQLITEOROLDRET(logoperator_time(time,operator))
}

int logoperator_unknown()
{
    EXECSQLITEOROLDRET(logoperator_unknown())
}

LogSystem* logsys_alloc(char*sysname)
{
    EXECSQLITEOROLDRET(logsys_alloc(sysname))
}

#define old_legacy_logsys_begin_transaction(a) 0
int logsys_begin_transaction(LogSystem* log)
{
    EXECSQLITEOROLDRET(logsys_begin_transaction(log))
}

void logsys_close(LogSystem* log)
{
    EXECSQLITEOROLD(logsys_close(log))
}

void logsys_compability_setup()
{
    EXECSQLITEOROLD(logsys_compability_setup())
}

int logsys_compilefilter(LogSystem* log, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logsys_compilefilter(log,filter))
}

int logsys_compileoperator(LogSystem* log,LogOperator* operator)
{
    EXECSQLITEOROLDRET(logsys_compileoperator(log,operator))
}

int logsys_compileprintform(LogSystem* log,PrintFormat* format)
{
    EXECSQLITEOROLDRET(logsys_compileprintform(log,format))
}

PrintFormat* logsys_defaultformat(LogSystem* log)
{
    if (tr_SQLiteMode() == 0)
        return old_legacy_logsys_defaultformat(log,0);
    else
        return sqlite_logsys_defaultformat(log);
}

void logsys_dumplabelfields(LogLabel* label)
{
    EXECSQLITEOROLD(logsys_dumplabelfields(label))
}

#define old_legacy_logsys_end_transaction(a) 0
int logsys_end_transaction(LogSystem* log)
{
    EXECSQLITEOROLDRET(logsys_end_transaction(log))
}

int logsys_entry_count(LogSystem* log, LogFilter* filter)
{
	EXECSQLITEOROLDRET(logsys_entry_count(log,filter))
}

char* logsys_filepath(LogSystem* log,char* filename)
{
    EXECSQLITEOROLDRET(logsys_filepath(log,filename))
}

void logsys_free(LogSystem* log)
{
    EXECSQLITEOROLD(logsys_free(log))
}

LogField* logsys_getfield(LogSystem* log, char* name)
{
    EXECSQLITEOROLDRET(logsys_getfield(log,name))
}

LogIndex* logsys_list_indexed(LogSystem* log, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logsys_list_indexed(log,filter))
}

LogEntry** logsys_makelist(LogSystem* log,LogFilter* filter)
{
    EXECSQLITEOROLDRET(logsys_makelist(log,filter))
}

PrintFormat* logsys_namevalueformat(LogSystem* log)
{
    if (tr_SQLiteMode() == 0)
        return old_legacy_logsys_namevalueformat(log,logsys_migrate); /* if we are migrating from old legacy to sqlite, we need all fields */ 
    else
        return sqlite_logsys_namevalueformat(log);
}

/* BugZ_9996: added for eSignature'CGI script */
/*
 * set global according to db type (sqlite or legacy)
 */
void logsys_setdbtype()
{
  extern int tr_useSQLiteLogsys;

  if (getenv("USE_SQLITE_LOGSYS") != NULL)
  {
    tr_useSQLiteLogsys = atoi(getenv("USE_SQLITE_LOGSYS"));
  }
  else
  {
    tr_useSQLiteLogsys = 0;
  }
}

LogSystem* logsys_open(char* filename, int mode)
{
    EXECSQLITEOROLDRET(logsys_open(filename,mode))
}

int logsys_openfile(LogSystem *log, char *basename, int flags, int mode)
{
    EXECSQLITEOROLDRET(logsys_openfile(log,basename,flags,mode))
}

/* BugZ_9996: added for eSignature'CGI script */
/*
 * returns 0 if data base is open else -1.
 */
int logsys_opendata(LogSystem *log)
{
   EXECSQLITEOROLDRET(logsys_opendata(log))
}

void logsys_operatebyfilter(LogSystem* log,LogFilter* filter,LogOperator* operator)
{
    EXECSQLITEOROLD(logsys_operatebyfilter(log,filter,operator))
}

LogLabel* logsys_readconfig(char* filename, int onRebuild)
{
    if (tr_SQLiteMode() == 0)
        return old_legacy_logsys_readconfig(filename);
    else
        return sqlite_logsys_readconfig(filename,onRebuild);
}

int logsys_readlabel(LogSystem *log)
{
    EXECSQLITEOROLDRET(logsys_readlabel(log))
}

int logsys_removebyfilter(LogSystem* log,LogFilter* filter,char* extension)
{
    EXECSQLITEOROLDRET(logsys_removebyfilter(log,filter,extension))
}

#define old_legacy_logsys_resetindex(a) return;
void logsys_resetindex(char* basename)
{
    EXECSQLITEOROLD(logsys_resetindex(basename))
}

#define old_legacy_logsys_rollback_transaction(a) 0
int logsys_rollback_transaction(LogSystem* logsys)
{
    EXECSQLITEOROLDRET(logsys_rollback_transaction(logsys))
}

void printformat_add(PrintFormat** format,char* str)
{
    EXECSQLITEOROLD(printformat_add(format,str))
}

void printformat_insert(PrintFormat** formats,char* name,char* format)
{
    EXECSQLITEOROLD(printformat_insert(formats,name,format))
}

int printformat_read(PrintFormat** formats,FILE* fp)
{
    EXECSQLITEOROLDRET(printformat_read(formats,fp))
}

char* syserr_string(int error)
{
    EXECSQLITEOROLDRET(syserr_string(error))
}

/* 
 * not used on sqlite
 */

#define sqlite_logsys_refreshmap(a) 0
int logsys_refresh(LogSystem* log)
{
    EXECSQLITEOROLDRET(logsys_refreshmap(log))
}

#define sqlite_logindex_passesfilter(a,b) 0
int logindex_passesfilter(LogIndex index, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logindex_passesfilter(index,filter))
}

#define sqlite_logentry_passesfilter(a,b) 0
int logentry_passesfilter(LogEntry* entry, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logentry_passesfilter(entry,filter))
}

#define sqlite_logsys_makelist_indexed(a,b) 0
LogEntry** logsys_makelist_indexed(LogSystem* log, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logsys_makelist_indexed(log,filter))
}

/*JRE 06.15 BG-81*/
#define old_legacy_logentry_getindex_env(a) 0
LogIndexEnv logentry_getindex_env(LogEntryEnv* entry)
{
    EXECSQLITEOROLDRET(logentry_getindex_env(entry))
}
  	 
#define old_legacy_logentry_printbyform_env(a,b,c) 0
void logentry_printbyform_env(FILE* fp,LogEntryEnv* entry,PrintFormat* format)
{
    EXECSQLITEOROLD(logentry_printbyform_env(fp,entry,format))
}

#define old_legacy_logentry_readindex_env(a,b) 0
LogEntryEnv* logentry_readindex_env(LogSystem* log, LogIndexEnv index)
{
    EXECSQLITEOROLDRET(logentry_readindex_env(log,index))
}

#define old_legacy_logsys_list_indexed_env(a,b) 0
LogIndexEnv* logsys_list_indexed_env(LogSystem* log, LogFilter* filter)
{
    EXECSQLITEOROLDRET(logsys_list_indexed_env(log,filter))
}

#define old_legacy_logenv_read(a) 0
LogEnv logenv_read(FILE* fp)
{
    EXECSQLITEOROLDRET(logenv_read(fp))
}

#define old_legacy_logsys_affect_sysname(a,b) 0
int logsys_affect_sysname(LogSystem* log, LogEnv lsenv)
{
    EXECSQLITEOROLDRET(logsys_affect_sysname(log,lsenv))
}
/*End JRE*/

void writeLog(int level, char *fmt, ...) {
	va_list ap;
	int dotrace = 0;
	
	if (getenv("LOGSYSTEM_TRACE")) {
		dotrace = 1;
		fprintf(stderr, "[LOGSYSTEM_TRACE (%.f)] ", tr_errorLevel);
	} else if (level <= tr_errorLevel) {
		dotrace = 1;
		switch (level) {
			case LOGLEVEL_NONE:
				fprintf(stderr, "[ ERROR ] ");
				break;
			case LOGLEVEL_NORMAL:
				fprintf(stderr, "[trace] ");
				break;
			case LOGLEVEL_DEBUG:
				fprintf(stderr, "[debug] ");
				break;
			case LOGLEVEL_DEVEL:
				fprintf(stderr, "[devel] ");
				break;
			default:
				fprintf(stderr, "[debug (%d)] ", level);
				break;
		}
	}
	if (dotrace > 0) {

		va_start(ap, fmt);

		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		fflush(stderr);

		va_end(ap);
	}
}

