/*========================================================================
        TradeXpress

        File:		logsystem.sqlite.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright (c) 2006 Illicom groupe Influe

        This file is taken from logsystem.h and has been restricted
        to the sqlite case.
==========================================================================
  @(#)TradeXpress $Id: logsystem.old_legacy.h 50932 2015-09-23 16:58:04Z jregniez $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
========================================================================*/

#ifndef _LOGSYSTEM_LOGSYSTEM_OLD_LEGACY_H
#define _LOGSYSTEM_LOGSYSTEM_OLD_LEGACY_H

#include "../logsystem.definitions.h"

#define LSMAGIC_MAP   (0x2ED1DB00 | (int) 'm')
#define LSMAGIC_DATA  (0x2ED1DB00 | (int) 'd')
#define LSFILE_DATA      "/.data"
#define LSFILE_MAP       "/.map"
#define LSFILE_CREATED   "/.created"
#define LSFILE_MODIFIED  "/.modified"
#define LSFILE_CLEANLOCK "/.cleanlock"

#define record_idx         record->header.idx
#define record_nextfree    record->header.nextfree
#define record_ctime       record->header.ctime
#define record_mtime       record->header.mtime
#define record_generation  record->header.generation
#define record_reserved1   record->header.reserved1

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

#ifdef MACHINE_MIPS
#define PROTO(x) ()
#else
#define PROTO(x) x
#endif

void			logentry_printbyformat					PROTO((FILE *, LogEntry *, PrintFormat *));
PrintFormat*	old_legacy_alloc_printformat			PROTO(());
void*			old_legacy_log_malloc					PROTO((int));
void*			old_legacy_log_realloc					PROTO((void *, int));
char*			old_legacy_log_str3dup					PROTO((char *, char *, char *));
char*			old_legacy_log_strdup					PROTO((char *));
void			old_legacy_log_strncat					PROTO((char *, char *, int));
void			old_legacy_log_strncpy					PROTO((char *, char *, int));
void			old_legacy_log_strsubrm					PROTO((char *, char *));
LogEntry*		old_legacy_logentry_alloc 				PROTO((LogSystem *, LogIndex));
LogEntry*		old_legacy_logentry_alloc_skipclear 	PROTO((LogSystem *, LogIndex));
char*			old_legacy_logentry_buildline			PROTO((LogEntry *, PrintFormat *));
void			old_legacy_logentry_copyfields			PROTO((LogEntry *, LogEntry *));
int				old_legacy_logentry_destroy				PROTO((LogEntry *));
LogEntry*		old_legacy_logentry_get					PROTO((LogSystem *));
LogIndex		old_legacy_logentry_getindex			PROTO((LogEntry *));
Integer			old_legacy_logentry_getintegerbyfield	PROTO((LogEntry *, LogField *));
Integer			old_legacy_logentry_getintegerbyname	PROTO((LogEntry *, char *));
Number			old_legacy_logentry_getnumberbyfield	PROTO((LogEntry *, LogField *));
Number			old_legacy_logentry_getnumberbyname		PROTO((LogEntry *, char *));
char*			old_legacy_logentry_gettextbyfield		PROTO((LogEntry *, LogField *));
char*		    old_legacy_logentry_gettextbyname		PROTO((LogEntry *, char *));
TimeStamp		old_legacy_logentry_gettimebyfield		PROTO((LogEntry *, LogField *));
TimeStamp		old_legacy_logentry_gettimebyname		PROTO((LogEntry *, char *));
LogEntry*		old_legacy_logentry_new					PROTO((LogSystem *));
void			old_legacy_logentry_operate				PROTO((LogEntry *, LogOperator *));
int				old_legacy_logentry_passesfilter		PROTO((LogEntry*,LogFilter*));
LogEntry*		old_legacy_logentry_readindex			PROTO((LogSystem *, LogIndex));
int				old_legacy_logentry_readlock			PROTO((LogEntry *));
LogEntry*		old_legacy_logentry_readraw				PROTO((LogSystem *, LogIndex));
int				old_legacy_logentry_remove				PROTO((LogSystem *, LogIndex, char *));
int				old_legacy_logentry_removefiles			PROTO((LogEntry *, char *));
void			old_legacy_logentry_setintegerbyfield	PROTO((LogEntry *, LogField *, Integer));
void			old_legacy_logentry_setintegerbyname	PROTO((LogEntry *, char *, Integer));
void			old_legacy_logentry_setnumberbyfield	PROTO((LogEntry *, LogField *, Number));
void			old_legacy_logentry_setnumberbyname		PROTO((LogEntry *, char *, Number));
void			old_legacy_logentry_settextbyfield		PROTO((LogEntry *, LogField *, char *));
void			old_legacy_logentry_settextbyname		PROTO((LogEntry *, char *, char *));
void			old_legacy_logentry_settimebyfield		PROTO((LogEntry *, LogField *, TimeStamp));
void			old_legacy_logentry_settimebyname		PROTO((LogEntry *, char *, TimeStamp));
/* BugZ_9996: added for eSignature'CGI script */
int                             old_legacy_logentry_getbyname(LogEntry *i_entry, char* i_name, void** o_data, int *o_type);
int				old_legacy_logentry_unlock				PROTO((LogEntry *));
int				old_legacy_logentry_write				PROTO((LogEntry *));
int				old_legacy_logentry_writelock			PROTO((LogEntry *));
int				old_legacy_logentry_writeraw			PROTO((LogEntry *));
int				old_legacy_logfd_readlock				PROTO((int fd));
int				old_legacy_logfd_unlock					PROTO((int fd));
int				old_legacy_logfd_writelock				PROTO((int fd));
void			old_legacy_logfilter_add				PROTO((LogFilter **, char *));
void			old_legacy_logfilter_addparam			PROTO((LogFilter**,PrintFormat**,char*));
void			old_legacy_logfilter_clear				PROTO((LogFilter *));
void			old_legacy_logfilter_free				PROTO((LogFilter *));
char*			old_legacy_logfilter_getkey				PROTO((LogFilter *));
int				old_legacy_logfilter_getmaxcount		PROTO((LogFilter *));
int				old_legacy_logfilter_getorder			PROTO((LogFilter *));
char*			old_legacy_logfilter_getseparator		PROTO((LogFilter *)); /* 4.00/CD */
void			old_legacy_logfilter_insert				PROTO((LogFilter **, char *, int, char *));
int				old_legacy_logfilter_printformat_read	PROTO((LogFilter **, PrintFormat **, FILE *));
int				old_legacy_logfilter_printformat_write	PROTO((LogFilter *,  PrintFormat *,  FILE *));
int				old_legacy_logfilter_read				PROTO((LogFilter **, FILE *));
void			old_legacy_logfilter_setkey				PROTO((LogFilter **, char *));
void			old_legacy_logfilter_setmaxcount		PROTO((LogFilter **, int));
void			old_legacy_logfilter_setoffset			PROTO((LogFilter **, int));
void			old_legacy_logfilter_setorder			PROTO((LogFilter **, int));
void			old_legacy_logfilter_setseparator		PROTO((LogFilter **, char *)); /* 4.00/CD */
char**			old_legacy_logfilter_textual			PROTO((LogFilter *));
char**			old_legacy_logfilter_textualparam		PROTO((LogFilter*));
void			old_legacy_logfilter_textual_free		PROTO((char **));
int				old_legacy_logfilter_write				PROTO((LogFilter *,  FILE *));
void			old_legacy_logheader_printbyformat		PROTO((FILE *, PrintFormat *));
int				old_legacy_logindex_passesfilter		PROTO((LogIndex, LogFilter *));
LogLabel*		old_legacy_loglabel_alloc 				PROTO((int size));
LogField*		old_legacy_loglabel_getfield			PROTO((LogLabel *, char *));
void			old_legacy_logoperator_add				PROTO((LogOperator **, char *));
void			old_legacy_logoperator_clear			PROTO((LogOperator *));
void			old_legacy_logoperator_free				PROTO((LogOperator *));
void			old_legacy_logoperator_insert			PROTO((LogOperator **, char *, int, char *));
int				old_legacy_logoperator_integer			PROTO((Integer *, OperatorRecord *));
int				old_legacy_logoperator_number			PROTO((Number *, OperatorRecord *));
int				old_legacy_logoperator_read				PROTO((LogOperator **, FILE *));
int				old_legacy_logoperator_text				PROTO((char *, OperatorRecord *));
int				old_legacy_logoperator_time				PROTO((TimeStamp *, OperatorRecord *));
int				old_legacy_logoperator_unknown			PROTO(());
int				old_legacy_logoperator_write			PROTO((FILE *, LogOperator *));
LogSystem*		old_legacy_logsys_alloc   				PROTO((char *));
char*			old_legacy_logsys_buildheader			PROTO((LogSystem *, PrintFormat *));
void			old_legacy_logsys_close					PROTO((LogSystem *));
void			old_legacy_logsys_compability_setup		PROTO(());
int				old_legacy_logsys_compilefilter			PROTO((LogSystem *, LogFilter *));
int				old_legacy_logsys_compileoperator		PROTO((LogSystem *, LogOperator *));
int				old_legacy_logsys_compileprintform		PROTO((LogSystem *, PrintFormat *));
LogHints*		old_legacy_logsys_createdhints			PROTO((LogSystem *, int *));
PrintFormat*	old_legacy_logsys_defaultformat			PROTO((LogSystem *, int));
void			old_legacy_logsys_dumplabelfields		PROTO((LogLabel*));
int 			old_legacy_logsys_entry_count			PROTO((LogSystem*,LogFilter*));
char*			old_legacy_logsys_filepath				PROTO((LogSystem *, char *));
void			old_legacy_logsys_free    				PROTO((LogSystem *));
LogField*		old_legacy_logsys_getfield				PROTO((LogSystem *, char *));
char**			old_legacy_logsys_getfieldnames			PROTO((LogSystem *));
time_t			old_legacy_logsys_lastchange			PROTO((LogSystem *));
LogIndex*		old_legacy_logsys_list_indexed			PROTO((LogSystem *, LogFilter *));
LogEntry**		old_legacy_logsys_makelist				PROTO((LogSystem *, LogFilter *));
LogEntry**		old_legacy_logsys_makelist_indexed		PROTO((LogSystem *, LogFilter *));
LogHints*		old_legacy_logsys_modifiedhints			PROTO((LogSystem *, int *));
PrintFormat*	old_legacy_logsys_namevalueformat		PROTO((LogSystem *, int));
LogSystem*		old_legacy_logsys_open					PROTO((char *, int));
int				old_legacy_logsys_openfile				PROTO((LogSystem *, char *,int ,int));
void			old_legacy_logsys_operatebyfilter		PROTO((LogSystem *, LogFilter *, LogOperator *));
LogLabel*		old_legacy_logsys_readconfig			PROTO((char *));
int				old_legacy_logsys_readlabel				PROTO((LogSystem *));
int				old_legacy_logsys_readlock				PROTO((LogSystem *));
int				old_legacy_logsys_removebyfilter		PROTO((LogSystem *, LogFilter *, char *));
int				old_legacy_logsys_unlock				PROTO((LogSystem *));
int				old_legacy_logsys_writelock				PROTO((LogSystem *));
void			old_legacy_printformat_add				PROTO((PrintFormat **, char *));
void			old_legacy_printformat_clear			PROTO((PrintFormat *));
void			old_legacy_printformat_insert			PROTO((PrintFormat**,char*,char*));
int				old_legacy_printformat_read				PROTO((PrintFormat **, FILE *));
int				old_legacy_printformat_write			PROTO((PrintFormat *,  FILE *));
char*			old_legacy_syserr_string				PROTO((int erno));
char*			tr_timestring							PROTO((char *, time_t));

#undef PROTO
#endif
