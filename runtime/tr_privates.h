/*========================================================================
        TradeXpress

        File:		tr_privates.h
        Author:		Christophe Mars (CMA)
        Date:		Wed Jan 05 2011

	This file defines prototypes for internal use in runtime librairy.
        Adding for BugZ_9980 : OPEN_DB.

        In futur, define all private prototypes in that file.

        Copyright (c) 2010 GenerixGroup
==========================================================================
	Record all changes here and update the above string accordingly.
	05.01.11/CMA	Created.
	05.04.11/JFC	Bug 1219 add new internal functions
========================================================================*/

#ifndef _TR_PRIVATE_H
#define _TR_PRIVATE_H

#include <stdarg.h>
#include "tr_strings.h"                     /* for TR_WARN_OPENDB_INIT_FAILED definition    */
#include <stdio.h>

#ifdef MACHINE_WNT
#ifndef __DEF_WIN_DIRENT_
#define __DEF_WIN_DIRENT_
#include <windows.h>
typedef struct {
	HANDLE handle;
	WIN32_FIND_DATA data;
} DIR;
#endif
#else
#include <dirent.h>
#endif

#ifndef TRUE
#define FALSE 0
#define TRUE  1
#endif

#ifdef MACHINE_WNT
char *getcwd (char *, int);
#endif

/*
 * functions defined into tr_array.c
 */
#include "tr_arrays.h"
char ** tr_am_text(char *table, char *name);
double *tr_am_num(char *table, char *name);
void *tr_am_rewind(char *table);
int tr_amtext_getpair(struct am_node **walkpos, char **idxp, char **valp);
int tr_amnum_getpair(struct am_node **walkpos, char **idxp, double *valp);
int tr_am_remove(char *table);
int tr_am_count(char *table);
int tr_am_walknodes(char *table, int (*fcn)(), void *arg);

/*
 * functions defined into tr_buildtext.c
 */
#define TX_BUFTRACESIZE 1024
void tr_FreeMemPoolArgs(char *, va_list);  /* remove parameters if they were allocated in mempool */
size_t tr_getBufSize(size_t minsize, const char *fmt, va_list ap); /* return the estimate size of the needed buffer */

/*
 * functions defined into tr_comparepaths.c
 */
int tr_comparepaths( char *, char * );

/*
 * functions defined into tr_dirfixup.c
 */
int tr_DirFixup(char *problem_path);

/*
 * functions defined into tr_execlp.c
 */
#ifdef MACHINE_WNT
void tr_wrap_exec_argv(char **v);
void tr_free_exec_argv(char **v);
#endif

/*
 * functions defined into tr_execvp.c
 */
#ifdef MACHINE_WNT
void tr_BackgroundExec(char **argv);
#endif

/*
 * functions defined into tr_fileop.c
 */
FILE *tr_fopen (char *filename, char *mode);
int tr_fclose(FILE *fp, char mode);
FILE *tr_popen (char *command,  char *mode);
int tr_pclose(FILE *fp);

/*
 * functions defined into tr_getfp.c
 */
FILE * tr_LookupFpFromName(char *filename, char *accmode);
void tr_FileCloseAll();

/*
 * functions defined into tr_getopt.c
 */
int getopt(int argc, char *const*argv, const char *optstring);

/*
 * functions defined into tr_load.c
 */
int tr_SetValue (int loadMode, char *arrayname, char *buffer, int multisep, int sep);

/*
 * functions defined into tr_log.c
 */
int tracelevel;       /* Debug level for traces */
int	tr_sourceLine;   /* Corresponding RTE line in relative trace */
void tr_Inittrace();         /* Initialize trace upon environment variables */
void tr_Trace(int , int , const char *, ...);

/*
 * functions defined into tr_logsysrem.c
 */
char *tr_PrettyPrintFileName(char *filename);
 
/*
 * functions defined into tr_memory.c
 */
void	tr_InitMemoryManager();
char *tr_LastMemPoolValue ();

/*
 * functions defined into tr_pack.c
 */
void tr_checkmsgdep ();

/*
 * functions defined into tr_readdir.c
 */
char *tr_ReadDir (DIR *dirp, char *path, char *search);
int tr_abspath(char *relpath, char *buffer, int bufmax);

/*
 * functions defined into tr_regcmppown.c
 */
char *regcmp(char *args, ...);
char *regex(char *re, char *subject, ...);

/*
 * functions defined into tr_regcomp.c
 */
void *tr_regular_compile (char *expr);

/*
 * functions defined into tr_regexp.c
 */
void tr_regular_clear();
int tr_regular_getsubs(char **, int);
char *tr_regular_execute(void *, char *);
char *tr_regular_lastmatch();

/*
 * functions defined into tr_strcsep.c
 */
char *tr_strcpick(char **sp, char c, char d, char e, int *ap);
char *tr_strcsep(char **sp, char c);

/*
 * functions defined into tr_strstr.c
 */
char *tr_strstr (char *s, char *t);

/*
 * Some macros for initialize OPEN_DB
 */
#define OPENDB_INIT if (tr_lsInitLayer() == FALSE) {tr_Fatal (TR_WARN_OPENDB_INIT_FAILED);}

/*
 * functions defined into tr_loaddef.c
 */
int tr_LoadDefaults (char *filename);
void tr_OverwriteDBglobals();

/*
 * functions defined into tr_CprgTools.c
 */

/* process options */
#define DB_LR_FLAG_READ_OPTS          0
/* set local mode */
#define DB_LR_FLAG_FORCE_LOCAL_MODE   1
/* set rls mode */
#define DB_LR_FLAG_FORCE_REMOTE_MODE  2

/*
 * Analyzes standard L(ocal)R(emote) flag
 * Returns TRUE if globals has been set.
 * Else returns FALSE.
 */
int tr_CprgStartupLrFlagAnalyzer(int lrFlag);

/* set local mode */
#define DB_LR_OPT_FORCE_LOCAL_MODE   'l'
/* set rls mode */
#define DB_LR_OPT_FORCE_REMOTE_MODE  'r'

/*
 * Analyzes standard L(ocal)R(emote) option
 * Returns TRUE if globals has been set.
 * Else returns FALSE. 
 */
int tr_CprgStartupLrOptsAnalyzer(int argc, char **argv);

/* process options */
#define DB_IMPL_FLAG_READ_OPTS     NULL 
/* set local mode */
#define DB_IMPL_FLAG_LEGACY        TR_LEGACY_DB
/* set rls mode */
#define DB_IMPL_FLAG_SQLITE        TR_SQLITE_DB

/*
 * Analyzes standard DB Implementation flag
 * Returns TRUE if globals has been set.
 * Else returns FALSE.
 */
int tr_CprgStartupImplFlagAnalyzer(char *implFlag);

/* set legacy db */
#define DB_IMPL_OPT_LEGACY  'O'
/* set rls mode */
#define DB_IMPL_OPT_SQLITE  'I'

/*
 * Analyzes standard DB Implementation options
 * Returns TRUE if globals has been set.
 * Else returns FALSE.
 */
int tr_CprgStartupImplOptsAnalyzer(int argc, char **argv);

/*
 * Use this function to initialize tr global variables and DB layout
 * at start time for C program (not necessary RTE one).
 * Returns TRUE if DB has been initialized. Else returns FALSE
 */
int tr_CprgStartup(
                   int   argc,
                   char *argv[],
                   char *tclrcName,                      /* must be NULL for standard location    */
                   int   lrFlag,                         /* local/remote flag value               */
                   int (*lrFlagAnalyzer)(int),           /* function to use for lrFlag processing */
                                                         /* could be NULL                         */
                   int (*lrOptsAnalyzer)(int,char**),    /* local/remote options analyzer         */
                                                         /* could be NULL                         */
                   char *implFlag,                       /* local DB implementation flag value    */
                   int (*implFlagAnalyzer)(char*),       /* local DB impl flag analyzer           */
                                                         /* could be NULL                         */
                   int (*implOptsAnalyzer)(int,char**)   /* local/remote options analyzer         */
                                                         /* could be NULL                         */
                  );

/*
 * Some macros for starting OPEN_DB
 */
#define OPENDB_START(argc,argv,tclrcName,lrFlag,lrFlagAnalyzer,lrOptsAnalyzer,implFlag,implFlagAnalyzer,implOptsAnalyzer)\
 if (tr_CprgStartup(argc,\
                    argv,\
                    tclrcName,\
                    lrFlag,\
                    lrFlagAnalyzer,\
                    lrOptsAnalyzer,\
                    implFlag,\
                    implFlagAnalyzer,\
                    implOptsAnalyzer) == FALSE) \
 {tr_Fatal (TR_WARN_OPENDB_INIT_FAILED);}







#endif /* TR_PRIVATE_H */
