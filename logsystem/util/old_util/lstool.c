/*============================================================================*/
/*        TradeXpress 4.3.2                                                   */
/*                                                                            */ 
/*        File:		lstool.c                                                  */
/*        Author:		Juha Nurmela (JN)                                     */
/*        Date:		Mon Oct  3 02:18:43 EET 1994                              */
/*                                                                            */
/*        Copyright (c) 2001-2003 Illicom                                     */ 
/*                                                                            */
/*	Common logsystem manipulation tool.                                       */
/*	Used to build, reconfigure, check, fix and ...                            */
/*	logsystems.                                                               */ 
/*============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: lstool.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*============================================================================*/
/*  Record all changes here and update the above string accordingly.          */
/*  3.00 03.10.94/JN  Created.                                                */
/*  3.01 14.10.96/JN  Silencing one harmless warning from cc.                 */
/*  3.02 19.09.97/JN  Both path separators for NT, \ and /.                   */
/*  3.03 15.06.99/HT  Moved void function out of exit() function. This caused */
/*	                  unexpected return values for -build option.             */
/*  4.01 31.07.02/CD  changed -l option to accept a parameter                 */
/*                    changed the index to the new type of index              */
/*		        	  in the function dump, change and delete                 */
/*  4.02 28.03.03/JT  lsunref command added                                   */
/*  4.03 29.06.05/CD  index now correctly displayed by options "created"      */
/*                    and "modified" commands.                                */
/*                    Before we were printing internal index instead          */
/*                    of user index (internal+version)                        */
/*  4.04 09.11.05/CD  prevent from trying to remove index less than 10        */
/*                    and use logentr_remove when removing an entry           */
/*  4.05 13.04.12/TP(CG) TX-468 filter added to check if its a sqlite database*/
/*                    or a old legacy database                                */
/*  5.1  13.12.12/SCh(CG) TX-2321 : sqlite database detection by TX-468 was   */
/*                    not accurate: use USE_SQLITE_LOGSYS instead as logadd() */
/*   Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits            */
/*============================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>

#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"
#include "translator/translator.h"

extern int do_check(int argc, char **argv);
extern int do_fix(int argc, char **argv);
extern int do_fixhints(int argc, char **argv);
extern int do_salvage(int argc, char **argv);
extern int do_pack(int argc, char **argv);
extern int do_orderfree(int argc, char **argv);
extern int do_lsunref(int argc, char **argv);
extern int do_rmunref(int argc, char **argv);
extern int logsys_reconfsystem(char *sysname, char *cfgfile);
extern int logsys_relabelsystem(char *sysname, char *cfgfile);
extern int logsys_buildsystem(char *sysname, char *cfgfile);
extern int logsys_convertsystem(char *sysname, char *cfgfile);

/* -------------------------------------------------------------------------- */
extern int tr_useSQLiteLogsys;         /* USE_SQLITE_LOGSYSin set in .userenv */

/* dummy variables */
double tr_errorLevel  = 0 ;            /* Not used here : shall be removed ?  */
char*  tr_programName = "";            /* Not used here : shall be removed ?  */

int   Quiet  = 0;                      /* Quiet mode not activated            */
int   Really = 1;                      /* Test mode not activated : real mode */
char *Sysname;                         /* Database name, also directory one   */
char *Cfgfile;                         /* Database definition in *.cfg file   */

static char *cmd;                      /* what command we execute             */
static char *fieldlist;                /* filename for fieldnames             */

static int verbose = 0;                /* verbose mode option set with -v     */
static int lines_between_headers = -1; /* The nimber of empty lines between   */
                                       /* header data (field labels) and      */
									   /* actual data.                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Functions defined in this file :                                           */
/* void:                                                                      */
/*        bail_out()                                                          */
/*        other_options()                                                     */
/* static :                                                                   */
/*        find_last_sep()                                                     */
/*        str4dup()                                                           */
/*        cfg_options()                                                       */
/*        getpair()                                                           */
/*        makelist()                                                          */
/*        printhints()                                                        */
/* static untyped:                                                            */
/*        do_change()                                                         */
/*        do_insert()                                                         */
/*        do_delete()                                                         */
/*        do_listcreated ()                                                   */
/*        do_listmodified()                                                   */
/*        do_list()                                                           */
/*        do_read()                                                           */
/*        do_dump()                                                           */
/*        do_status()                                                         */
/*        do_fields()                                                         */
/* Untyped:                                                                   */
/*        do_reconfig()                                                       */
/*        do_relabel()                                                        */
/*        do_build()                                                          */
/*        do_convert()                                                        */
/*        main()                                                              */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : void bail_out(char * fmt,...)                         NOT USED  */
/*          Send to stderr the command name followed by the parameters given  */
/*          bail_out() formatted according to fmt contents, and ended by the  */
/*          the contents of errno if any                                      */
/* INPUT:                                                                     */
/*        fmt : format to print the undefined list of parameter               */
/*        ... : undefined list of parameters.                                 */
/* OUTPUT:                                                                    */
/*        None                                                                */
/* -------------------------------------------------------------------------- */
void bail_out(char * fmt,...)
{
	va_list ap;

	va_start(ap,fmt);
	fprintf(stderr, "%s: ", cmd);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (errno)
	{
		fprintf(stderr, " (%s)\n", syserr_string(errno));
	}
	exit(1);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static char * find_last_sep(char *path)                         */
/*            Lot like strrchr(path, '/') for finding out the basename.       */
/*            NULL when no pathseparators on string.                          */
/* INPUT:                                                                     */
/*       char *path : string to search for separator / ou \\                  */
/* OUTPUT:                                                                    */
/*       Return 0 if no seperator in the string or the string after the sep   */
/* -------------------------------------------------------------------------- */
static char * find_last_sep(char *path)
{
	char *sep = NULL;

	for ( ; ; ++path) {
		switch (*path) {
		case 0:
			return (sep);
#ifdef MACHINE_WNT
		case '\\':
#endif
		case '/':
			sep = path;
		}
	}
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static char * str4dup(char *a, char *b, char *c, char *d)       */
/*          Create a string and concatenate the input string in it.           */
/*          returns the adresse of the created string                         */
/* INPUT:                                                                     */
/*          char *a, char *b, char *c, char *d : strings to be concatenated   */
/* OUTPUT:                                                                    */
/*          address pf the concatenated string                                */
/* -------------------------------------------------------------------------- */
static char * str4dup(char *a, char *b, char *c, char *d)
{
	char *s = log_malloc(strlen(a) + strlen(b) + strlen(c) + strlen(d) + 1);
	strcpy(s, a);
	strcat(s, b);
	strcat(s, c);
	strcat(s, d);
	return (s);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static void cfg_options(int argc, char **argv)                  */
/*           Command line handling.                                           */
/*           First for the creation/reconfiguration routines: do_reconfig(),  */
/*           do_relabel(), do_build(), do_convert(),                          */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None only globales settup according to found options :           */
/*           h : print possible options                                       */
/*           s : globale Sysname is set to the given database system name     */
/*           f : globale Cfgfile is set to the given configuration file name  */
/*           N : Normal / test mode ==> globale Really is set to 0 (= TEST)   */
/*           q : quiet mode  => globale Quiet is set to 1                     */
/* -------------------------------------------------------------------------- */
static void cfg_options(int argc, char **argv)
{
	char *cp;
	int  c;

	opterr = 0;
    /* ---------------------------------------------------------------------- */
	while ((c = getopt(argc, argv, "h:s:f:Nq")) != -1) {
		switch (c) {
		default:
		case 'h':
			fprintf(stderr, "Possible options:\n\
	-s sysname\n\
	-f cfgfile\n\
	-q (Be quiet)\n\
	-N (Dont actually create/reconfig system)\n");
			exit(2);
		case 's':
			Sysname = optarg;
			break;
		case 'f':
			Cfgfile = optarg;
			break;
		case 'N':
			Really = 0;
			break;
		case 'q':
			Quiet = 1;
			break;
		}
    }
    /* ---------------------------------------------------------------------- */
	
    /* ---------------------------------------------------------------------- */
	while (optind < argc) {
		char *arg = argv[optind];

		if (strlen(arg) > 4 && !strcmp(arg + strlen(arg) - 4, ".cfg")) {
			Cfgfile = argv[optind++];
			continue;
		}
		if (!Sysname) {
			Sysname = argv[optind++];
			continue;
		}
		break;
	}
    /* ---------------------------------------------------------------------- */
	if (!Sysname && Cfgfile) {
		if ((cp = find_last_sep(Cfgfile)) != NULL) {
			++cp;
		} else {
			cp = Cfgfile;
        }
		
		Sysname = strdup(cp);
		if ((cp = strrchr(Sysname, '.')) != NULL) {
			*cp = 0;
		}
	}
    /* ---------------------------------------------------------------------- */
	if (!Cfgfile && Sysname) {
		if ((cp = find_last_sep(Sysname)) != NULL) {
			++cp;
		} else {
			cp = Sysname;
		}

		Cfgfile = str4dup(Sysname, "/", cp, ".cfg");
	}
    /* ---------------------------------------------------------------------- */
	if (!Cfgfile || !Sysname) {
		fprintf(stderr, "Need system name and description.\n");
		exit(2);
	}
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : void other_options(int argc, char **argv)                       */
/*           And next for other misc. routines: do_change(), do_insert(),     */
/*           do_delete(), do_listcreated(), do_listmodified(), do_list(),     */
/*           do_read(), do_dump(), do_status() and do_fields().               */
/*                                                                            */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None only globales settup according to found options :           */
/*           h : print possible options                                       */
/*           v : globale verbose is incremented                               */
/*           s : globale Sysname is set to the given database system name     */
/*           L : globale fieldlist is set to optarg value                     */
/*           l : globale lines_between_headers is set to optarg integer value */
/*           q : quiet mode  => globale Quiet is set to 1                     */
/* -------------------------------------------------------------------------- */
void other_options(int argc, char **argv)
{
	int c;

	opterr = 0;
    /* ---------------------------------------------------------------------- */
	while ((c = getopt(argc, argv, "hvs:L:l:q")) != -1)
	{
		switch (c) {
		default:
		case 'h':
			fprintf(stderr, "Possible options:\n\
	-v \n\
	-s sysname\n\
	-L fieldfile\n\
	-l lines_between_headers\n\
	-q \n");
			exit(2);
		case 'l':
			lines_between_headers = atoi(optarg);
			break;
		case 'v':
			++verbose;
			break;
		case 's':
			Sysname = optarg;
			break;
		case 'L':
			fieldlist = optarg;
			break;
		case 'q':
			Quiet = 1;
			break;
		}
	}
    /* ---------------------------------------------------------------------- */

	if (Sysname == NULL && optind < argc) {
		Sysname = argv[optind++];
	}

	if (Sysname == NULL) {
		fprintf(stderr, "Need system name, -s option\n");
		exit(2);
	}
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static void getpair(char *arg, char **name, char **value)       */
/*           Parse arg for a name=value string, for doinsert() and do_change()*/
/*                                                                            */
/*           name points to the address of arg, and value points to the       */
/*           address after = in arg or the and of arg. = is replace by 0 so   */
/*           name is a string terminated by 0 and value also terminated by    */
/*           arg ended 0.                                                     */
/*           Example arg = [Foo=12\0] gives name=[Foo\0] value=[12\0] and by  */
/*                   the end arg = [Foo\012\0]                                */
/* INPUT:                                                                     */
/*           char *arg : string to be split                                   */
/* OUTPUT:                                                                    */
/*           char **name : pointer on the 1st char of arg                     */
/*           char **value: either pointer on the char after = in arg or ""    */
/* -------------------------------------------------------------------------- */
static void getpair(char *arg, char **name, char **value)
{
	*name = strdup(arg);
	if ((*value = strchr(*name, '=')) != NULL) {
		*(*value)++ = 0;
	} else {
		*value = "";
	}
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static char ** makelist(char *filename, char **argv, int *pos)  */
/*          Creates an array of strings containing either:                    */
/*          - each lines of the file which name is given as a parameter, or   */
/*          - each values folling a - character in argv input string          */
/*          for functions: do_listcreated(), do_listmodified(), do_list(),    */
/*                         do_read()                                          */
/* INPUT:                                                                     */
/*        char *filename : if any, file to read                               */
/*        char **argv    : if no file to read, list of option to parse        */
/*        int  *pos      : walk through index on argv string, start point is  */
/*                         given as an input.                                 */
/* OUTPUT:                                                                    */
/*        Return the address of the created list elaborated in names          */
/* -------------------------------------------------------------------------- */
static char ** makelist(char *filename, char **argv, int *pos)
{
	char **names;
	int i;

	/* if a file name is given, read it and set names[i] values               */
	if (filename) {
		static char *listed[256];
		char *cp, buf[128];
		FILE *fp;

		if ((fp = fopen(filename, "r")) == NULL) {
			perror(filename);
			exit(1);
		}
		names = listed;
		i = 0;
		while (i < 255 && fgets(buf, sizeof(buf), fp)) {
			if (cp = strchr(buf, '\n')) {
				*cp = 0;
			}
			names[i++] = strdup(buf);
		}
		fclose(fp);
		names[i] = NULL;

	/* else if argv is filled, parse it and set names[i] values               */
	} else if (argv && argv[*pos]) {
		names = NULL;
		while (argv[*pos]) {
			if (argv[*pos][0] == '-') {
				argv[*pos] = NULL;
				++*pos;
				break;
			}
			if (!names) {
				names = argv + *pos;
			}
			++*pos;
		}
	/* else set a NULL pointer as names address                               */
	} else {
		names = NULL;
	}
	/* endif                                                                  */
	/* return names address                                                   */
	return (names);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static void printhints(LogSystem *log,                          */
/*                                   LogHints *hints,                         */
/*                                   int count,                               */
/*                                   char **names)                            */
/*                                                                            */
/*          for functions: do_listcreated(), do_listmodified()                */
/* INPUT:                                                                     */
/*        LogSystem *log  :                                                   */
/*        LogHints *hints :                                                   */
/*        int count       : number of entry to be printed                     */
/*        char **names    : list of fields to be printed for each entry       */
/* OUTPUT:                                                                    */
/*                                                                            */
/* -------------------------------------------------------------------------- */
static void printhints(LogSystem *log, LogHints *hints, int count, char **names)
{
	LogEntry *entry;
	int i;

	/* for each index of hints table less than count value                    */
	for (i = 0; i < count; ++i) {
		entry = logentry_readraw(log, hints[i].idx);
		/* if the entry exists then                                           */
		/*    print on stdout the fileds given in names,array of strings      */
		if (entry) {
			old_legacy_logentry_printbynames(stdout, entry, names);
			logentry_free(entry);
		/* else print an error message                                        */
		} else {
			printf("%d Unreadable\n", hints[i].idx);
		}
	}
	/* endfor                                                                 */
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_change(int argc, char **argv)                         */
/*                                                                            */
/*           Changes one record giving its index                              */
/* USAGE:                                                                     */
/*           lstool change-s <database> index field1=value1 [ … fieldn=valuen]*/
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_change(int argc, char **argv)
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx;
	int i;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL);

	if (optind > argc - 2) {
		fprintf(stderr, "Need index and fields\n");
		exit(2);
	}
	idx = atoi(argv[optind++]);

	/* 4.01/CD uses readindex instead of readraw 
		   so we can use the actual index for the records
         */
	entry = logentry_readindex(log, idx);
	if (entry == NULL) {
		fprintf(stderr, "Cannot read entry\n");
		exit(3);
	}
	for (i = optind; i < argc; ++i) {
		char *name, *value;

		getpair(argv[i], &name, &value);
		old_legacy_logentry_setbyname(entry, name, value);
		free(name);
	}
	if (logentry_write(entry)) {
		fprintf(stderr, "Cannot write entry\n");
		exit(4);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_insert(int argc, char **argv)                         */
/*                                                                            */
/*           Inserts one record with the specified values                     */
/* USAGE:                                                                     */
/*           lstool insert -s <database> [field1=value1 … fieldn=valuen]      */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_insert(int argc, char **argv)
{
	LogSystem *log;
	LogEntry  *entry;
	LogIndex   idx;
	int i;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL);

	if (optind > argc - 1) {
		fprintf(stderr, "Need fields\n");
		exit(2);
	}
	entry = logentry_new(log);
	if (entry == NULL) {
		fprintf(stderr, "Cannot create new entry\n");
		exit(3);
	}
	for (i = optind; i < argc; ++i) {
		char *name, *value;

		getpair(argv[i], &name, &value);
		old_legacy_logentry_setbyname(entry, name, value);
		free(name);
	}
	if (logentry_write(entry)) {
		fprintf(stderr, "Cannot write entry\n");
		exit(4);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_delete(int argc, char **argv)                         */
/*                                                                            */
/*           Deletes one or more record giving its index                      */
/* USAGE:                                                                     */
/*           lstool delete -s <database> index1 .. indexn                     */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_delete(int argc, char **argv)
{
	LogSystem *log;
	LogEntry *entry;
	LogIndex idx;
	int i;
    int nerr=0;
    int nRet;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL);

	if (optind > argc - 1) {
		fprintf(stderr, "Need index\n");
		exit(2);
	}
	for (i = optind; i < argc; ++i) {
		idx = atoi(argv[i]);

        /* 4.04/CD  check that the given index is less than the generation divider */
        if (idx<LS_GENERATION_MOD) {
			fprintf(stderr, "Cannot remove index %d: invalid index\n",idx);
            nerr++;
            continue;
        }

        /* 4.04/CD remove data like done in logremove, using logentry_remove instead of
           logentry_readindex + logentry_destroy 
        */
        if (nRet=logentry_remove(log, idx, NULL)) {
			fprintf(stderr, "Cannot remove index %d: %s\n",
				idx, syserr_string(errno));
            nerr++;
        }
	}
    if (nerr) { 
        /* 4.04/CD goes into error if at least one remove failed */
        exit(3); 
    }
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_listcreated(int argc, char **argv)                    */
/*                                                                            */
/*           Displays the last created records                                */
/* USAGE:                                                                     */
/*           lstool created -s <database> [-v]                                */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
/* 4.03/CD 
   default fields to display in a  "lstool created" command 
*/
static char *created_fields[3]={"INDEX","CREATED",NULL};

static do_listcreated(int argc, char **argv)
{
	LogSystem *log;
	LogHints  *hints;
	int count;
	char **names = NULL;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	if (verbose) {
		names = makelist(fieldlist, argv, &optind);
		if (!names) {
			names = old_legacy_logsys_getfieldnames(log);
		}
    } else {
        names = created_fields;
	}

	hints = old_legacy_logsys_createdhints(log, &count);
	if (hints) {
		printhints(log, hints, count, names);
		old_legacy_logsys_freehints(hints);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_listmodified(int argc, char **argv)                   */
/*                                                                            */
/*           Displays the last modified records                               */
/* USAGE:                                                                     */
/*           lstool modified -s <database> [-v]                               */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
/* 4.03/CD
   default fields to display in a  "lstool modified" command 
*/
static char *modified_fields[3]={"INDEX","MODIFIED",NULL};

static do_listmodified(int argc, char **argv)
{
	LogSystem *log;
	LogHints  *hints;
	int count;
	char **names = NULL;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	if (verbose) {
		names = makelist(fieldlist, argv, &optind);
		if (!names) {
			names = old_legacy_logsys_getfieldnames(log);
		}
    } else {
        names = modified_fields;
	}
	hints = old_legacy_logsys_modifiedhints(log, &count);
	if (hints) {
		printhints(log, hints, count, names);
		old_legacy_logsys_freehints(hints);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_list(int argc, char **argv)                           */
/*                                                                            */
/*           Lists the records for the specified fields                       */
/* USAGE:                                                                     */
/*           lstool list -s <database> [-l headerlines] [-L fieldlist] field1 */
/*           [ … fieldn]                                                      */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_list(int argc, char **argv)
{
	LogSystem *log;
	LogEntry *entry;
	char **names;
	int n = 0;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	names = makelist(fieldlist, argv, &optind);
	if (!names) {
		names = old_legacy_logsys_getfieldnames(log);
	}

	if (lines_between_headers < 0) {
		old_legacy_logsys_printheaderbynames(stdout, log, names);
	}
	for (;;) {
		if (lines_between_headers > 0 && (n++ % lines_between_headers) == 0) {
			old_legacy_logsys_printheaderbynames(stdout, log, names);
		}
		entry = old_legacy_logentry_get(log);
		if (entry == NULL) {
			break;
		}
		old_legacy_logentry_printbynames(stdout, entry, names);
		logentry_free(entry);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_read(int argc, char **argv)                           */
/*                                                                            */
/* USAGE:                                                                     */
/*           lstool read  ???                                                 */
/* INPUT:                                                                     */
/*        argc : number or arguments given                                    */
/*        argv : the command line                                             */
/* OUTPUT:                                                                    */
/*        return 0 if the command succeed, 1 otherwise.                       */
/* -------------------------------------------------------------------------- */
static do_read(int argc, char **argv)
{
	LogSystem *log;
	LogEntry  *entry;
	LogIndex   idx;
	char     **names;
	int n = 0;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	names = makelist(fieldlist, argv, &optind);
	if (!names) {
		names = old_legacy_logsys_getfieldnames(log);
	}

	if (lines_between_headers < 0) {
		old_legacy_logsys_printheaderbynames(stdout, log, names);
	}
	while (optind < argc) {
		if (lines_between_headers > 0 && (n++ % lines_between_headers) == 0) 
		{
			old_legacy_logsys_printheaderbynames(stdout, log, names);
		}
		idx = atoi(argv[optind++]);
		/* 4.01/CD uses readindex instead of readraw 
			   so we can use the actual index for the records
         	*/
		entry = logentry_readindex(log, idx);
		if (entry == NULL) {
			fprintf(stderr, "Cannot read entry %d\n", idx);
		} else {
			old_legacy_logentry_printbynames(stdout, entry, names);
			logentry_free(entry);
		}
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_dump(int argc, char **argv)                           */
/*                                                                            */
/*           Lists the records for the specified indexes                      */
/* USAGE:                                                                     */
/*           lstool dump -s <database> [-l headerlines] index1 [ … indexn]    */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_dump(int argc, char **argv)
{
	LogSystem *log;
	LogEntry  *entry;
	LogIndex   idx;
	int i, n = 0;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	if (optind > argc - 1) {
		fprintf(stderr, "Need index\n");
		exit(2);
	}
	if (lines_between_headers < 0) {
		old_legacy_logsys_dumpheader(log);
	}
	for (i = optind; i < argc; ++i) {
		if (lines_between_headers > 0 && (n++ % lines_between_headers) == 0) {
			old_legacy_logsys_dumpheader(log);
		}
		idx = atoi(argv[i]);
		entry = logentry_readindex(log, idx);
		if (entry == NULL) {
			fprintf(stderr, "Cannot read %d: %s\n", idx, syserr_string(errno));
			exit(3);
		}
		old_legacy_logentry_dump(entry);
		logentry_free(entry);
	}
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_status(int argc, char **argv)                         */
/*                                                                            */
/*           Displays the status of the database                              */
/* USAGE:                                                                     */
/*           lstool status -s <database>                                      */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_status(int argc, char **argv)
{
	LogSystem *log;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	old_legacy_logsys_refreshmap(log);

	old_legacy_logsys_dumplabel(log->label);
	old_legacy_logsys_dumpmap(log->map);

	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : static do_fields(int argc, char **argv)                         */
/*                                                                            */
/*           Displays the fields configuration                                */
/* USAGE:                                                                     */
/*           lstool fields -s <database>                                      */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
static do_fields(int argc, char **argv)
{
	LogSystem *log;

    /* Process authorised options : hvs:L:l:q to configure processing         */
	other_options(argc, argv);
	log = logsys_open(Sysname, LS_DONTFAIL | LS_READONLY);

	old_legacy_logsys_dumplabeltypes (log->label);
	old_legacy_logsys_dumplabelfields(log->label);

	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : do_reconfig(int argc, char **argv)                              */
/*                                                                            */
/*           Reconfigures the existing database system (adds or removes fields*/
/*           ) based on the database system configuration file                */
/* USAGE:                                                                     */
/*           lstool reconfig -s <database> -f <database>.cfg [-q][-N]         */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
do_reconfig(int argc, char **argv)
{
    /* Process authorised options : h:s:f:Nq to configure processing          */
	cfg_options(argc, argv);

	exit(logsys_reconfsystem(Sysname, Cfgfile));
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : do_relabel(int argc, char **argv)                               */
/*                                                                            */
/*           Changes the labels of the database                               */
/* USAGE:                                                                     */
/*           lstool relabel -s <database>                                     */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
do_relabel(int argc, char **argv)
{
    /* Process authorised options : h:s:f:Nq to configure processing          */
	cfg_options(argc, argv);

	exit(logsys_relabelsystem(Sysname, Cfgfile));
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : do_build(int argc, char **argv)                                 */
/*                                                                            */
/*           Builds a new database based on the database system configuration */
/*           file. This command creates the directory database system and the */
/*           necessary files and subdirectories.                              */
/* USAGE:                                                                     */
/*           lstool build -s <database> -f <database>.cfg [-q][-N]            */
/* INPUT:                                                                     */
/*           argc : number of arguments                                       */
/*           argv : arguments value                                           */
/* OUTPUT:                                                                    */
/*           None                                                             */
/* -------------------------------------------------------------------------- */
do_build(int argc, char **argv)
{
    /* Process authorised options : h:s:f:Nq to configure processing          */
	cfg_options(argc, argv);

	/*
	**  3.03/HT
	**  exit(logsys_buildsystem(Sysname, Cfgfile));
	*/
	logsys_buildsystem(Sysname, Cfgfile);
	
	/* remove unreferenced, dangling files, that have no record               */
    do_rmunref(argc, argv);
	exit(0);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : do_convert(int argc, char **argv)                               */
/*                                                                            */
/*           Changes the database format                                      */
/* USAGE:                                                                     */
/*           lstool convert -s sysname -f sysname.cfg                         */
/* INPUT:                                                                     */
/*        argc : number or arguments given                                    */
/*        argv : the command line                                             */
/* OUTPUT:                                                                    */
/*        return 0 if the command succeed, 1 otherwise.                       */
/* -------------------------------------------------------------------------- */
do_convert(int argc, char **argv)
{
	char buffer[1024];
	char *cp;
	char *getenv();

	if (argc == 2) {
		/*
		 * Just convert sysname
		 */
		Sysname = argv[1];
		Cfgfile = buffer;
		
		sprintf(buffer, "%s/%s.cfg", Sysname, Sysname);
		if (!access(buffer, 0)) {
			goto ok;
		}

		if (cp = getenv("HOME")) {
			sprintf(buffer, "%s/database/%s.cfg", cp, Sysname);
			if (!access(buffer, 0)) {
				goto ok;
			}
		}
		if (cp = getenv("EDIHOME")) {
			sprintf(buffer, "%s/database/%s.cfg", cp, Sysname);
			if (!access(buffer, 0)) {
				goto ok;
			}
		}
		Cfgfile = NULL;
	}
    /* Process authorised options : h:s:f:Nq to configure processing          */
	cfg_options(argc, argv);
ok:
	exit(logsys_convertsystem(Sysname, Cfgfile));
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function : main(int argc, char **argv)                                     */
/*            Entry point of lstool.exe                                       */
/* USAGE:                                                                     */
/*           lstool function -s sysname [-f sysname.cfg] [-L fieldlist]       */
/*           [-l headerlines] [-q] [-v] [-N]                                  */
/* with                                                                       */
/*     -s sysname: the name of the database system                            */
/*     -f sysname.cfg: the path to the database system configuration file     */
/*     -L fieldlist: the list of displayed fields, one field name per line    */
/*     -l headerlines: the number of empty lines between header data          */
/*                     (field labels) and actual data.                        */
/*     -q: quiet mode                                                         */
/*     -v: verbose mode                                                       */
/*     -N: this mode makes it possible to test the other actions. The actions */
/*         are not taken, but their results are displayed as if they had been */
/*         taken.                                                             */
/* INPUT:                                                                     */
/*        argc : number or arguments given                                    */
/*        argv : the command line                                             */
/* OUTPUT:                                                                    */
/*        return 0 if the command succeed, 1 otherwise.                       */
/* -------------------------------------------------------------------------- */
main(int argc, char **argv)
{

	char *v[256];
	int n = 0;

	/* SCh (CG) 13/12/2012 :                                                  */
	/*          TX-2321 avoid use of this command on SQLITE database          */
    /* ---------------------------------------------------------------------- */
    /* only revelant it local mode in rls, this is looked at server side      */
    if (getenv("USE_SQLITE_LOGSYS") != NULL) {
        tr_useSQLiteLogsys = atoi(getenv("USE_SQLITE_LOGSYS"));
    } else {
        tr_useSQLiteLogsys = 0;
	}
    /* ---------------------------------------------------------------------- */

    /* ---------------------------------------------------------------------- */
	/* if we are in a old legacy database                                     */
	if ( tr_useSQLiteLogsys == 0)
	{
		logsys_compability_setup();

	    /* end SCh (CG) 13/12/2012 */
		/*
		 * If we are not invoked as lstool,
		 * use basename of argv[0] as the command to execute.
		 */
		if ((cmd = find_last_sep(argv[0])) != NULL) {
			++cmd;
		} else {
			cmd = argv[0];
		}

		if (!strcmp(cmd, "lstool")) {
			--argc;
			++argv;
			cmd = argv[0];
		}
		Sysname = NULL;

        /* Build usage information as we search for the specified command     */
    	#define IF_CMD(x, fcn) v[n++]=x; if(cmd&&!strcmp(cmd,x)){exit(fcn(argc,argv));}

		IF_CMD("build",		do_build	)           /* Defined in this file   */
		IF_CMD("reconfig",	do_reconfig	)           /* Defined in this file   */
		IF_CMD("relabel",	do_relabel	)           /* Defined in this file   */
		IF_CMD("convert",	do_convert	)           /* Defined in this file   */
		v[n++] = "\n";

		IF_CMD("check",		do_check	)           /* Defined in check.c     */    
		IF_CMD("salvage",	do_salvage	)           /* Defined in fix.c       */
		IF_CMD("fix",		do_fix		)           /* Defined in fix.c       */
		IF_CMD("pack",		do_pack		)           /* Defined in fix.c       */
		IF_CMD("fixhints",	do_fixhints	)           /* Defined in fixhints.c  */
		IF_CMD("orderfree",	do_orderfree)           /* Defined in orderfree.c */
		IF_CMD("lsunref",	do_lsunref	)           /* Defined in rmunref.c   */
		IF_CMD("rmunref",	do_rmunref	)           /* Defined in rmunref.c   */
		v[n++] = "\n";

		IF_CMD("dump",		do_dump		)           /* Defined in this file   */
		IF_CMD("modified",	do_listmodified)        /* Defined in this file   */
		IF_CMD("created",	do_listcreated)         /* Defined in this file   */
		IF_CMD("fields",	do_fields	)           /* Defined in this file   */
		IF_CMD("status",	do_status	)           /* Defined in this file   */
		v[n++] = "\n";

		IF_CMD("list",		do_list		)           /* Defined in this file   */
		IF_CMD("insert",	do_insert	)           /* Defined in this file   */
		IF_CMD("delete",	do_delete	)           /* Defined in this file   */
		IF_CMD("change",	do_change	)           /* Defined in this file   */
		IF_CMD("read",		do_read		)           /* Defined in this file   */
		v[n++] = "\n";

		v[n] = NULL;

        /* if this point is reached, no command has been executed             */
		/* Check if a command has been provided or not and report to the user */
		if (cmd) {
			fprintf(stderr, "Unknown command %s.\n", cmd);
		} else {
			fprintf(stderr, "No command given.\n", cmd);
		}

        /* Print the usage information as the command was wrong or absent     */
		fprintf(stderr, "Available:\n");
		for (n = 0; v[n]; ++n) {
			fprintf(stderr, " %s", v[n]);
        }
        /* Exit with an error code                                            */
		exit(2);
	}
	/* TP (CG) le 05/04/2012 :                                                */
	/*      TX-468 else added to print an error if its a sqlite database      */
	else
	{
		fprintf(stderr, "lstool is not allowed for sqlite databases: check USE_SLITE_LOGSYS env variable \n");
        /* Exit with an error code                                            */
		exit(2);
	}
	/* end TP (CG) le 05/04/2012 */
}
/* -------------------------------------------------------------------------- */
