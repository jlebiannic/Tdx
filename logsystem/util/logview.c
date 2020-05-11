/*============================================================================
	E D I S E R V E R   L O G S Y S T E M

	File:		logsystem/logview.c
	Programmer:	Mikko Valimaki
	Date:		Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1998 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: logview.c 55499 2020-05-07 16:25:38Z jlebiannic $")
#include "runtime/tr_constants.h"


/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 24.01.95/JN	Adapted from 3.1.1 logview
  3.01 19.02.96/JN	exim stuff.
  3.02 18.02.97/MW	printformat for pseudofield user (in master mode)
			encreased from -9.9 to -17.17
  4.03 16.01.98/JN	rls option.
  4.04 28.10.98/KP	No more separate WNT and Unix parts (WNT works only
			as remote, though. Should be no problem?)
  4.05 16.02.99/KP	Added missing export formats (listfile & absIdx)
  4.06 05.07.02/CD	Added column separator option -C
  4.07 10.07.02/CD	Added -? option	and changed help display
			Added KEYINFO_END before calling exportdump_files
			Added dump in local mode on NT (in exportdump_files)
  4.08 08.11.04/FM      Added management of dump > 2 giga
			Added management of binaries infos into dump files
  4.09 13.03.13/YLI(CG)	TX-460 if fields are wrong in the filters,
								display errors and terminate the program
  4.10 20.01.14/CGL(CG) TX-2495: Remplace LOGLEVEL_DEVEL (level 3)
								 with LOGLEVEL_BIGDEVEL (level 4)
  4.10 23.10.14/PSA(CG) WBA-333: Ajout option -q pour activer la pagination
                                    native SQLite
                                 Optimisation du comptage des entrees pour
                                    les bases SQLite
	   20.11.14/SCH(CG) WBA-333: Plantage de la compilation SOLARIS a cause d'un CRLF
	   
  4.11 06.15/JRE BG-81: implementation du multi-environnements
  4.12 12.06.15/CGL(CG) TX-2732: Modification de l'usage de la commande logview
  4.13 21.07.15/JRE BG-81: multi-environnement: ajout de la possibilite d'utiliser des noms d'environnements avec des caracteres speciaux
  4.14 30.10.15/JRE TX-2767: logview sort en segfault en mode multi-environnement
  4.15 01.12.15/JRE TX-2792: limiter l'utilisation du mode MBA a la plateforme Linux
  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#define __USE_LARGEFILE64
#include <stdio.h>
#ifndef MACHINE_WNT
#include <dirent.h>
#include <unistd.h>
#endif
#include <errno.h>

extern int errno;
extern int logsys_migrate;

#include "translator/translator.h"
#include "logsystem/lib/logsystem.dao.h"
#include "logsystem/lib/logsystem.definitions.h"

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "logview";

static char *cmd;

int exportdump_end();
int exportdump_entry(LogEntry *entry);
int exportdump_entry_env(LogEntryEnv *entry);
int exportdump_files(LogEntry *entry);
int exportdump_files_env(LogEntryEnv *entry);


void bail_out(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", cmd);
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (errno)
	{
		fprintf(stderr, " (%d,%s)\n", errno, dao_syserr_string(errno));
	}
	exit(1);
}

static char *output_name = "(stdout)";

int PrintEntry(LogEntry *entry, PrintFormat *printFormat);
LogEntry *logview_readentry(void *log, int index);
/*JRE 06.15 BG-81*/
int PrintEntryEnv(LogEntryEnv *entry, PrintFormat *printFormat);
LogEntryEnv *logview_readentry_env(void *lg, char* index);
/*End JRE*/

static int nusers = 0;
static char *users[256 +1];

/* 4.04/KP : Now defaults as remote. */
static int header_flag = 0;
static int namevalue_flag = 0;
static int printall_flag = 0;
static int exportdump_flag = 0;
static int dumpverbose_flag = 0;
static int page_number = -1;
static int maxcount = 0;
static int page_header_flag = 0;
static int page_max = 0;
static int listfile = 0;
static int logsys_useTransaction = 0;

static LogSystem *exportLog = NULL;

static void *log;

static void fatal_exit(char *msg, int arg)
{
	fprintf(stderr, "%s: ", cmd);
	fprintf(stderr, msg, arg);
	fprintf(stderr, "\n");
	exit(1);
}

static void perror_exit(char *msg)
{
	int e = errno;

	fprintf(stderr, "%s: ", cmd);
	errno = e;
	perror(msg);
	exit(1);
}

main(int argc, char **argv)
{
	extern char *optarg;
	extern int  optind, opterr, optopt;

	char *        sysname = NULL;
	LogIndex      absIndex = 0;
	PrintFormat * printFormat = NULL;
	LogFilter *   logFilter = NULL;
	LogEntry **   entries;
	FILE *        fp;
	FILE *        listFp = NULL;
	int  	      c;
	int           size_only = 0;
	int           label_fields_only = 0;
    int           no_pagecount = 0;
	int           n, i, page_err=0;
	LogEntry *    entry;
	LogField *    keyfield;
	char **       flt;
	LogIndex *    indexes;
	FilterRecord	*g;
	/*YLI(CG) 13.03.13 TX-460
	nunk stocks the nombre of no exist fields in the filters */
	int nunk = 0;
	char ** vector;
	FilterRecord	*end;
	/*End YLI(CG)*/
	/*JRE 06.15 BG-81*/
	LogEntryEnv * entryEnv;
	LogIndexEnv * indexesEnv;
	LogEnv logEnv = NULL;
	int multBase = 0;
	char * s;
	/*End JRE*/

	tr_UseLocaleUTF8();

	dao_logsys_compability_setup();

	cmd = argv[0];
	opterr = 0;
	while ((c = getopt(argc, argv, "?zwWIOmAS:u:U:s:i:f:F:p:P:n:N:o:c:adhM:D:tTL:rlC:Gg:v:E:q")) != -1) {
		switch (c) {
			default: fprintf(stderr, "%s: Invalid option '%c'.\n", cmd, optopt);
			case '?': /* 4.07 CD */
				fprintf(stderr, "\n%s [options] -s sysname [FIELD1 .. [FIELDn]]\n",cmd);
				/* TX-2732 CGL(CG): Modification of the z's message option */
				fprintf(stderr, "\n\
Valid options are:\n\
	-s sysname		logsystem name in the following format: \n\
				[ediuser@host[#user#password]database \n\
	-h			print header as the first line (on the second line if also print page position)\n\
	-m			(obsolete)\n\
	-i index		absolute index to read\n\
	-f 'name op value'	insert an entry to filter\n\
	-F filterfile		read filter from file\n\
	-p name=format		insert an entry to printformat\n\
	-P formatfile		read printformats from file\n\
	-A			printout all entries in name=value format\n\
	-n name=format		print field as namevalue pair\n\
	-N formatfile		read printformats from file\n\
	-o sortkey		set sorting key field\n\
	-a			sort in ascending order\n\
	-d			sort in descending order\n\
	-c count		set maxcount for list\n\
	-M filterfile		read filter/printformat from file (XXX.f)\n\
	-D dumpfile		export dump into file\n\
	-T			trace dumping\n\
	-L listfile		list of loglines\n\
	-C separator		set column separator\n\
	-w 			return number of entries in the table\n\
	-W 			return informations about the fields used\n\
	-t			use transaction mode on base\n\
	-z 			in legacy mode, do a dump with all fields. Option not needed for SQLite mode, all fields are dumped\n\
	-g 			only output the paGe (size of page based on maxcount) of results\n\
	-G 			print the current page position as the first line\n\
	-v verbose_level	set loglevel, 0: no log, 2: debug log\n\
	-q			Skip pagecount\n");
/*JRE TX-2792*/
#if defined(MACHINE_LINUX)
	fprintf(stderr, "	-E listEnv		read environments of file\n");
#endif
/*Fin TX-2792*/
				exit (2);
			case 's': sysname = optarg; break;
			case 't' :
				logsys_useTransaction = 1;
				break;
			case 'z' :
				/* are we migrating from old legacy to sqlite ?
				 * yes so we need all fields and especially the indexes */
				logsys_migrate = 1;
				break;
			case 'S':
				/* kept for compatibility */
				break;
			case 'w': size_only = 1; break;
			case 'W': label_fields_only = 1; break;
			case 'u':
				/* Add a single user to list. */
				if (nusers + 1 >= SIZEOF(users)){
					fatal_exit("Too many users, %d max", SIZEOF(users));
					}
				users[nusers++] = optarg;
				users[nusers] = NULL;
				break;
			case 'U':
				{
					/* Read in userlist file. */
					char *cp, buf[256];

					if (!strcmp(optarg, "-")){
						fp = stdin;
						}
					else if ((fp = fopen(optarg, "r")) == NULL){
						perror_exit(optarg);
						}

					while (fgets(buf, sizeof(buf), fp))
					{
						if (*buf == '\n' || *buf == '#'){
							continue;
							}
						if ((cp = strchr(buf, '\n')) != NULL){
							*cp = 0;
							}
						if (nusers + 1 >= SIZEOF(users)){
							fatal_exit("Too many users, %d max", SIZEOF(users));
							}
						users[nusers++] = dao_log_strdup(buf);
						users[nusers] = NULL;
					}
					if (fp != stdin){
						fclose(fp);
						}
				}
				break;
			case 'm':
				break;
			case 'h': header_flag = 1; break;
			case 'i': absIndex = atoi(optarg); break; /* Absolute index. */
			case 'f': dao_logfilter_add(&logFilter, optarg); break; /* Add one field into filter. */
			case 'F':
				/* Read filter from file. */
				if (!strcmp(optarg, "-")){
					fp = stdin;
					}
				else if ((fp = fopen(optarg, "r")) == NULL){
					perror_exit(optarg);
					}
				dao_logfilter_read(&logFilter, fp);
				if (fp != stdin){
					fclose(fp);
					}
				break;
			case 'p': dao_printformat_add(&printFormat, optarg); break; /* Printout format field. */
			case 'P':
				/* Printout formats from a file. */
				if (!strcmp(optarg, "-")){
					fp = stdin;
					}
				else if ((fp = fopen(optarg, "r")) == NULL){
					perror_exit(optarg);
					}
				dao_printformat_read(&printFormat, fp);
				if (fp != stdin){
					fclose(fp);
					}
				break;
			case 'A':
				namevalue_flag = 1;
				printall_flag = 1;
				break;
			case 'n':
				namevalue_flag = 1;
				dao_printformat_add(&printFormat, optarg);
				break;
			case 'N':
				namevalue_flag = 1;

				if (!strcmp(optarg, "-")){
					fp = stdin;
					}
				else if ((fp = fopen(optarg, "r")) == NULL){
					perror_exit(optarg);
					}
				dao_printformat_read(&printFormat, fp);
				if (fp != stdin){
					fclose(fp);
					}
				break;
			case 'o': dao_logfilter_setkey(&logFilter, optarg); break;
			case 'a': dao_logfilter_setorder(&logFilter,  1); break;
			case 'd': dao_logfilter_setorder(&logFilter, -1); break;
			case 'c': dao_logfilter_setmaxcount(&logFilter, atoi(optarg)); break;
			case 'M':
				if (!strcmp(optarg, "-")){
					fp = stdin;
					}
				else if ((fp = fopen(optarg, "r")) == NULL){
					perror_exit(optarg);
					}

				dao_logfilter_printformat_read(&logFilter, &printFormat, fp);

				if (fp != stdin){
					fclose(fp);
					}
				break;
			case 'D':
				exportdump_flag = 1;
				printall_flag   = 1;

				if (strcmp(optarg, "-"))
				{
					output_name = optarg;

					/* 4.08/FM
					   Open file in 64 bits mode and in binary mode.
					   Then we can write file > 2 giga.
					*/
					#ifdef MACHINE_WNT
					if (freopen(output_name, "wb", stdout) == NULL){
						perror_exit(output_name);
						}
					#else
					if (!freopen64(output_name, "w", stdout)){
						perror_exit(output_name);
						}
					#endif
				}
				break;
			case 'T': dumpverbose_flag = 1; break;
			case 'L':
				listfile = 1;
				if (!strcmp(optarg, "-")){
					listFp = stdin;
					}
				else if ((listFp = fopen(optarg, "r")) == NULL){
					perror_exit(optarg);
					}
				break;
			case 'C': dao_logfilter_setseparator(&logFilter, optarg); break;
			case 'g': page_number = atoi(optarg); break;
			case 'G': page_header_flag = 1; break;
			case 'v': tr_errorLevel = atof(optarg); break;
			case 'q': no_pagecount = 1; break;
			/*JRE 06.15 BG-81*/
			case 'E': 
				/*JRE TX-2792*/
				#if defined(MACHINE_LINUX)
					/* Read list of environements from file. */
					if (!strcmp(optarg, "-")) {
						fp = stdin;
					}
					else if ((fp = fopen(optarg, "r")) == NULL) {
						perror_exit(optarg);
					}
					logEnv = dao_logenv_read(fp);
					multBase = 1;
				
					/*sysname est egal a la premiere base renseignee dans le fichier de conf*/
					sysname = logEnv->base;
				
					if (fp != stdin){
						fclose(fp);
					}
					break;
				#else
					fatal_exit("Multibase only works on linux", 0);
				#endif
				/*End TX-2792*/
			/*End JRE*/
		}
	}
	while (optind < argc){
		dao_printformat_add(&printFormat, argv[optind++]);
	}

	if (!sysname){
		fatal_exit("No system name given", 0);
		}

	writeLog(LOGLEVEL_NORMAL, "loglevel is %.f", tr_errorLevel);

	writeLog(LOGLEVEL_NORMAL, "logview is in local mode");
	log = dao_logsys_open(sysname, LS_FAILOK | LS_READONLY);
	if ((log)
	&&  (logsys_useTransaction == 1)
	&&  (dao_logsys_begin_transaction(log) == -1)){
			exit(1);
			}
			
	/*JRE 06.15 BG-81*/
	/*affectation de la liste des logsystem names et des environnements*/
	if (logEnv != NULL) {
		dao_logsys_affect_sysname(log,logEnv);
	}
	/*End JRE*/
	

	if (log == NULL){
		perror_exit(sysname);
		}

    /* PSA(CG) : 24/10/2014 - WBA-333 : set flag to use SQL offset */
    if (no_pagecount) {
#if defined(MACHINE_LINUX) || defined(MACHINE_AIX)
        setenv("LOGVIEW_NO_PAGECOUNT", "1", 1);
#else
		putenv("LOGVIEW_NO_PAGECOUNT=1");
#endif
    }
    /* fin WBA-333 */

	/* < 0 to deal with -1 and other (stupid/buggy?) negative page selection */
	if (page_number < 0) {
		writeLog(LOGLEVEL_BIGDEVEL, "page_number < 0 (%d)", page_number);
		if (page_header_flag == 1)
		{
			page_number = 1;
			maxcount = dao_logfilter_getmaxcount(logFilter);
			dao_logfilter_setoffset(&logFilter, 0);
		}
	} else {
		writeLog(LOGLEVEL_BIGDEVEL, "page_number = %d", page_number);
		maxcount = dao_logfilter_getmaxcount(logFilter);
		if ((page_number != 1) && (maxcount == 0)){
			fatal_exit("Without maxcount, only first page can be displayed\n",0);
			}
		dao_logfilter_setoffset(&logFilter, (page_number - 1) * maxcount);
	}
	writeLog(LOGLEVEL_BIGDEVEL, "maxcount=%d", maxcount);

	if  (size_only == 1)
	{
		int entry_count;

		writeLog(LOGLEVEL_DEBUG, "we only want the count of entries");
		if (logFilter != NULL)
		{
			writeLog(LOGLEVEL_BIGDEVEL, "apply filters");

			/*YLI(CG) 13.03.13 TX-460
			when nunk is 0: field exists */
			/* Compile the fields in the filter */
			nunk = dao_logsys_compilefilter(log, logFilter);
			if (nunk != 0)
			{
				end = &logFilter->filters[logFilter->filter_count];
				for (g = logFilter->filters; g < end; ++g)
				{
					g->field = NULL;

					g->field = dao_logsys_getfield(log, g->name);
					if (!g->field){
						fprintf (stderr, "Error: Field %s does not exist\n",g->name);
						}
				}
				exit(0);
			}
		}
		writeLog(LOGLEVEL_BIGDEVEL, "... and count entry\n");

		/*PSA WBA-333
		Utilisation du compteur pour les tables */
		/*JRE 06.15 BG-81*/
		if (multBase == 1) {
			/*multi environnement*/
			entry_count = logsys_dao_entry_count_env((LogSystem *)log, logFilter);
		} else {
			entry_count = logsys_dao_entry_count((LogSystem *)log, logFilter);
		}
		/*End JRE*/
		
		writeLog(LOGLEVEL_BIGDEVEL, "entry_count = %d", entry_count);

		if ((logsys_useTransaction == 1) &&  (dao_logsys_end_transaction(log) == -1))
		{
			exit(1);
		}

		printf("%d entr%s in the base.\n",entry_count,((entry_count <= 1)?"y":"ies"));
		exit(0);
	}

	if  (label_fields_only == 1)
	{
		writeLog(LOGLEVEL_DEBUG, "We only want the labels of fiels in DB");

		dao_logsys_dumplabelfields(((LogSystem *)log)->label);
		if ((logsys_useTransaction == 1)
		&&  (dao_logsys_end_transaction(log) == -1)){
				exit(1);
				}

		exit(0);
		
	}


	if (printall_flag){
		printFormat = dao_logsys_namevalueformat(log);
		}
	else if (!printFormat){
		printFormat = dao_logsys_defaultformat(log);
		}
	dao_logsys_compileprintform(log, printFormat);
	

	/* 4.06/CD copy the column separator from the logfilter to the printformat */
	if ((printFormat) && (logFilter) && logFilter->separator){
		printFormat->separator = dao_log_strdup(logFilter->separator);
	}

	if (absIndex)
	{
		LogEntry *ep;
		/* Just one entry to print */
		writeLog(LOGLEVEL_BIGDEVEL, "Just one entry to print");

		ep = logview_readentry(log, absIndex);

		if (ep == NULL)
		{
			if (logsys_useTransaction == 1){
				dao_logsys_end_transaction(log);
				}
			fatal_exit("Entry %d is unreadable", absIndex);
		}

		if ((page_number >= 0) && (maxcount > 0))
		{
			/* one entry = one page obviously ! */
			page_max = 1;
			switch (page_number)
			{
				case 0:
					break;
				case 1:
					{
						if (page_header_flag){
							fprintf(stderr,"Printing page %d/1.\n",page_number);
							}
						if (header_flag){
							dao_logheader_printbyformat(stdout, printFormat);
							}
						PrintEntry(ep, printFormat);
					}
				break;
				default :
					if (page_header_flag){
						fprintf(stderr,"Page out of range.\n");
						}
				break;
			}
		}
	else
		{
			if (header_flag){
				dao_logheader_printbyformat(stdout, printFormat);
				}
			PrintEntry(ep, printFormat);
		}
	}
	else if (listFp)
	{
		if(multBase == 1){
			char idxEnv[512], envIdx[512];
                        char *temp, *env, *idx;
                        int len;

			log_dao_attach_database_env(log);
			
			if (header_flag){
				dao_logheader_printbyformat_env(stdout, printFormat);
			}
			
			while (fgets(idxEnv, sizeof(idxEnv), listFp))
			{			
				len = strlen(idxEnv);
				if (len > 1) {  /* check for blank line */	
					
					/* remove newline */
					if( idxEnv[len-1] == '\n' )
						idxEnv[len-1] = 0;
				
					/* Change from index|env to env.index*/
					temp = strtok(idxEnv, "|");
					idx = strdup(temp);
					temp = strtok(NULL, "|");
					env = strdup(temp);
			
					strcpy(envIdx, env);
					strcat(envIdx, ".");
					strcat(envIdx, idx);
				
					/* get the entry ... */
					entryEnv = (LogEntryEnv*)malloc(sizeof(struct logentryenv));
					entryEnv = logview_readentry_env(log, envIdx);
					/* ... then print it */
					PrintEntryEnv(entryEnv, printFormat);
				}
			}
		}
		else {
			/* List of entries, most probably from xxxmgr */
			LogIndex idx;
			LogEntry *ep;
			char *cp, buf[512];

			if (header_flag){
				dao_logheader_printbyformat(stdout, printFormat);
			}
			while (fgets(buf, sizeof(buf), listFp))
			{
				if ((idx = atoi(buf)) > 0)
				{
					ep = logview_readentry(log, idx);

					if (ep == NULL)
					{
						if (logsys_useTransaction == 1){
							dao_logsys_end_transaction(log);
							}
						fatal_exit("Entry %d is unreadable", idx);
					}
					PrintEntry(ep, printFormat);
				}
				/*
				 *  Nobody can tell how long the lines are,
				 *  keep reading until the line contains the newline
				 *  or we get eof.
				 */
				while ((cp = strchr(buf, '\n')) == NULL && fgets(buf, sizeof(buf), listFp)){
					;
				}
			}		
		}
	}
	else
	{
		writeLog(LOGLEVEL_BIGDEVEL, "Normal case. We want all entries after apply filter and format");

		/*YLI(CG) 13.03.13 TX-460
		when nunk is 0: field exists */
		nunk = dao_logsys_compilefilter(log, logFilter); /* Compile the fields in the filter */
		if (nunk != 0)
		{	end = &logFilter->filters[logFilter->filter_count];
			for (g = logFilter->filters; g < end; ++g)
			{
				g->field = NULL;

				g->field = dao_logsys_getfield(log, g->name);
				if (!g->field){
					fprintf (stderr, "Error: Field %s does not exist\n",g->name);
					}
			}
			exit(0);
		}
		
		/*End */

		if (header_flag){
			dao_logheader_printbyformat(stdout, printFormat);
		}

		writeLog(LOGLEVEL_BIGDEVEL, "Create indexed list of entries");

		/*JRE 06.15 BG-81*/
		if (multBase == 1) {
			/*multi environnement*/
			indexesEnv = dao_logsys_list_indexed_env(log, logFilter);
			
			writeLog(LOGLEVEL_BIGDEVEL, "Before print of entries");
			n = 0;
			
			while ((s = indexesEnv?indexesEnv[n]:"0") != "0")
			{
				n++;
				
				/* display headers before first entry if needed */
				if (n == 1 && page_header_flag)
				{
					int j;

					j = dao_logsys_entry_count(log,logFilter);

					if (maxcount<1)
					{
						maxcount=1;
						page_max=1;
					}
					else {
						page_max = j / maxcount;
					}
					if (j % maxcount > 0) {
						page_max++;
					}
					fprintf(stderr,"Printing page %d/%d.\n",page_number,page_max);
				}

				if (page_number!=0)
				{

					/* get the entry ... */
					entryEnv = (LogEntryEnv*)malloc(sizeof(struct logentryenv));
					entryEnv = logview_readentry_env(log, s);

					/* ... then print it */
					PrintEntryEnv(entryEnv, printFormat);
				}
			}
		} else {
			indexes = dao_logsys_list_indexed(log, logFilter);
		}
		
		
		if (multBase != 1) {
				
			writeLog(LOGLEVEL_BIGDEVEL, "Before print of entries");
			n = 0;
				
			while ((i = indexes?indexes[n]:0) != 0)
			{
				n++;

				/* display headers before first entry if needed */
				if (n == 1 && page_header_flag)
				{
					int j;

					j = dao_logsys_entry_count(log,logFilter);
						
					if (maxcount<1)
					{
						maxcount=1;
						page_max=1;
					}
					else{
						page_max = j / maxcount;
					}
					if (j % maxcount > 0){
						page_max++;
					}
					fprintf(stderr,"Printing page %d/%d.\n",page_number,page_max);	
				}

				if (page_number!=0)
				{
					/* get the entry ... */
					entry = logview_readentry(log, i);

					/* ... then print it */
					PrintEntry(entry, printFormat);
				}
			}
		}
		/*End JRE*/

		/* say why no entries were displayed */
		if (((n == 0) || (page_number == 0)) && page_header_flag)
		{
			fprintf(stderr,"Page out of range.\n");
		}
	}
	exportdump_end();

	if ((logsys_useTransaction == 1)
	&&  (dao_logsys_end_transaction(log) == -1)){
		exit(1);
		}

	if (fflush(stdout) || ferror(stdout))
    {
		fprintf(stderr, "fflush error \n");
		perror("(stdout)");
		exit(1);
	}
	exit (0);
}

LogEntry *logview_readentry(void *log, int index)
{
	LogEntry *entry;

	entry = dao_logentry_readindex((LogSystem *) log, index);
		
	return entry;
}

/*JRE 06.15 BG-81*/
LogEntryEnv *logview_readentry_env(void *lg, char* index)
{
	LogEntryEnv *entry;

	entry = dao_logentry_readindex_env((LogSystem *) lg, index);


	return entry;
}
/*End JRE*/


int PrintEntry(LogEntry *entry, PrintFormat *printFormat)
{
	if (entry)
    {
		if (exportdump_flag){
			exportdump_entry(entry);
			}

		dao_logentry_printbyformat(stdout, entry, printFormat);
			

		if (exportdump_flag)
		{
			/* 4.07 CD add KEYINFO in all cases */
			printf("---KEYINFO_END---\n");

			exportdump_files(entry);
		}
		dao_logentry_free(entry);
		entry = NULL;
	}
    return 0;
}

/*JRE 06.15 BG-81*/
int PrintEntryEnv(LogEntryEnv *entry, PrintFormat *printFormat)
{
	if (entry)
    {
		if (exportdump_flag) {
			exportdump_entry_env(entry);
		}

		dao_logentry_printbyform_env(stdout, entry, printFormat);

		if (exportdump_flag)
		{
			/* 4.07 CD add KEYINFO in all cases */
			printf("---KEYINFO_END---\n");

			exportdump_files_env(entry);
		}
		dao_logentryenv_free(entry);
		entry = NULL;
	}
    return 0;
}
/*End JRE*/

int exportdump_begin(LogSystem *ls)
{
	printf("---BASE:%s\n", ls->sysname);
}

int exportdump_end()
{
	if (exportLog)
    {
		printf("---END:%s\n", exportLog->sysname);

		if (dumpverbose_flag){
			fprintf(stderr, "\n");
			}
	}
    return 0;
}

int exportdump_entry(LogEntry *entry)
{
	if (exportLog != entry->logsys)
    {
		exportdump_end();
		exportLog = entry->logsys;
		exportdump_begin(exportLog);
	}
    printf("---KEYINFO_BEGIN---\n");

	if (dumpverbose_flag){
		fprintf(stderr, "\nE %d", dao_logentry_getindex(entry));
	}
    return 0;
}

/*JRE 06.15 BG-81*/
int exportdump_entry_env(LogEntryEnv *entry)
{
	if (exportLog != entry->logsys)
    {
		exportdump_end();
		exportLog = entry->logsys;
		exportdump_begin(exportLog);
	}
    printf("---KEYINFO_BEGIN---\n");

	if (dumpverbose_flag){
		fprintf(stderr, "\nE %d", dao_logentry_getindex_env(entry));
	}
    
    return 0;
}
/*End JRE*/

/*
 *  This should remember the subdirs encountered 1st time,
 *  not to read thru .../files each time !
 *  4.07/CD - Now works also on NT
 */
int exportdump_files(LogEntry *entry)
{
#ifdef MACHINE_WNT
	HANDLE files;
	WIN32_FIND_DATA dd;
#define dname dd.cFileName
#else
	DIR *files;
	struct dirent *dd;
#define dname dd->d_name
#endif
	char *filepath, *subdir, *base;
	char *nl;
	FILE *fp;
	struct stat st;
	int namelen;
	char namebuf[64];
	char path[1024];
	char buf[1024];
	unsigned char c;

	sprintf(namebuf, "%d", (dao_logentry_getindex(entry)));
	
	namelen = strlen(namebuf);

	sprintf(path, "%s/", entry->logsys->sysname);
	filepath = path + strlen(path);

	strcpy(filepath, "files");
	subdir = filepath + strlen(filepath);

#ifdef MACHINE_WNT
	strcat(filepath, "\\*");
	files = FindFirstFile(path, &dd);
	/*	*base = 0; En commentaire sinon plantage !!!! */
	if (files == INVALID_HANDLE_VALUE){
		files = NULL;
		}
#else
	files = opendir(path);
#endif

	if (files)
    {
#ifdef MACHINE_WNT
		do
#else
		while (dd = readdir(files))
#endif
		{
			if (dname[0] == '.'){
				continue;
			}
			if (LOGSYS_EXT_IS_DIR){
				sprintf(subdir, "/%s", dname);
				base = subdir + strlen(subdir);
				if (stat(path, &st)){
					perror(path);
					continue;
				}
				if ((st.st_mode & S_IFMT) != S_IFDIR){
					continue;
				 }
				 sprintf(base, "/%d", (dao_logentry_getindex(entry)));
			}
            else
            {
				/* IDX.EXT style.
				 * Note non-descriptive (ha!) usage of subdir & base */
				if (strncmp(dname, namebuf, namelen) != 0
				 || (dname[namelen] != 0
				  && dname[namelen] != '.')){
					continue;
				}
				sprintf(subdir, "/%s", dname);
				base = subdir + 1 + namelen;
			}
			/* 4.08/FM
			   Management of binaries infos in dump file : We read the import file in binary mode
			*/
#ifdef MACHINE_WNT
			fp = fopen (path, "rb");
#else
			fp = fopen (path, "r");
#endif
			if (fp == NULL)
            {
                if (errno != ENOENT)
                {
                    perror(path);
					if (logsys_useTransaction == 1){
						dao_logsys_end_transaction(log);
						}
                    exit(1);
                }
                continue;
			}

			if (LOGSYS_EXT_IS_DIR){
				base[1] = 0;
			}
			else
            {
				/* Print out in new format, anyway.
				 *
				 *  xxx/files/IDX.edifact
				 *           ^subdir
				 *               ^base
				 *  xxx/files/edifact/IDX
				 */
				strcpy(subdir + 1, base + 1);
				strcat(subdir, "/");
			}
			if (dumpverbose_flag){
				fprintf(stderr, " %s", filepath);
				}
			printf("---FILE:%s\n", filepath);
			nl = "\n";
			/* 4.08/FM  : use i/o binary functions
			   the dump file is created with binary functions,
			   then a syslog binary file can be dumped without error
			*/
			c= fgetc(fp);
			while (!feof(fp)){
                fputc(c, stdout);
                c = fgetc(fp);
			}
			fputc('\n', stdout);
			if (ferror(fp)){
				perror(path);
				if (logsys_useTransaction == 1){
					dao_logsys_end_transaction(log);
				}
				exit(1);
			}
			fclose(fp);
			if (ferror(stdout)){
				perror("(stdout)");
				if (logsys_useTransaction == 1){
					dao_logsys_end_transaction(log);
					}
				exit(1);
			}
		}
#ifdef MACHINE_WNT
		while (FindNextFile(files, &dd));
#endif

#ifdef MACHINE_WNT
		FindClose(files);
#else
		closedir(files);
#endif
	} /* end if file */
    return 0;
}

/*JRE 06.15 BG-81*/
int exportdump_files_env(LogEntryEnv *entry)
{
#ifdef MACHINE_WNT
	HANDLE files;
	WIN32_FIND_DATA dd;
#define dname dd.cFileName
#else
	DIR *files;
	struct dirent *dd;
#define dname dd->d_name
#endif
	char *filepath, *subdir, *base;
	char *nl;
	FILE *fp;
	struct stat st;
	int namelen;
	char namebuf[64];
	char path[1024];
	char buf[1024];
	unsigned char c;

	sprintf(namebuf, "%d", (dao_logentry_getindex_env(entry)));

	namelen = strlen(namebuf);

	sprintf(path, "%s/", entry->logsys->sysname);
	filepath = path + strlen(path);

	strcpy(filepath, "files");
	subdir = filepath + strlen(filepath);

#ifdef MACHINE_WNT
	strcat(filepath, "\\*");
	files = FindFirstFile(path, &dd);
	/*	*base = 0; En commentaire sinon plantage !!!! */
	if (files == INVALID_HANDLE_VALUE){
		files = NULL;
		}
#else
	files = opendir(path);
#endif

	if (files)
    {
#ifdef MACHINE_WNT
		do
#else
		while (dd = readdir(files))
#endif
		{
			if (dname[0] != '.'){
				if (LOGSYS_EXT_IS_DIR){
					sprintf(subdir, "/%s", dname);
					base = subdir + strlen(subdir);
					if (stat(path, &st)){
						perror(path);
						continue;
					}
					if ((st.st_mode & S_IFMT) != S_IFDIR){
						continue;
					}
					sprintf(base, "/%d", (dao_logentry_getindex_env(entry)));
				}
        	    else
            	{
					/* IDX.EXT style.
					 * Note non-descriptive (ha!) usage of subdir & base */
					if (strncmp(dname, namebuf, namelen) != 0
					 || (dname[namelen] != 0
					  && dname[namelen] != '.')){
						continue;
					}
					sprintf(subdir, "/%s", dname);
					base = subdir + 1 + namelen;
				}
				/* 4.08/FM
				   Management of binaries infos in dump file : We read the import file in binary mode
				*/
#ifdef MACHINE_WNT
				fp = fopen (path, "rb");
#else
				fp = fopen (path, "r");
#endif
				if (fp == NULL)
    	        {
        	        if (errno != ENOENT)
            	    {
                	    perror(path);
						if (logsys_useTransaction == 1){
							dao_logsys_end_transaction(log);
						}
	                    exit(1);
    	            }
        	        continue;
				}
	
				if (LOGSYS_EXT_IS_DIR){
					base[1] = 0;
				}
				else
	            {
					/* Print out in new format, anyway.
					 *
					 *  xxx/files/IDX.edifact
					 *           ^subdir
					 *               ^base
					 *  xxx/files/edifact/IDX
					 */
					strcpy(subdir + 1, base + 1);
					strcat(subdir, "/");
				}
				if (dumpverbose_flag){
					fprintf(stderr, " %s", filepath);
				}
				printf("---FILE:%s\n", filepath);
				nl = "\n";
				/* 4.08/FM  : use i/o binary functions
				   the dump file is created with binary functions,
				   then a syslog binary file can be dumped without error
				*/
				c= fgetc(fp);
				while (!feof(fp)){
                	fputc(c, stdout);
	                c = fgetc(fp);
				}
				fputc('\n', stdout);
				if (ferror(fp)){
					perror(path);
					if (logsys_useTransaction == 1){
						dao_logsys_end_transaction(log);
					}
					exit(1);
				}
				fclose(fp);
				if (ferror(stdout)){
					perror("(stdout)");
					if (logsys_useTransaction == 1){
						dao_logsys_end_transaction(log);
					}
					exit(1);
				}
			}
		}
#ifdef MACHINE_WNT
		while (FindNextFile(files, &dd));
#endif

#ifdef MACHINE_WNT
		FindClose(files);
#else
		closedir(files);
#endif
	} /* end if file */
    return 0;
}
/*End JRE*/
