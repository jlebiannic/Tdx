/*============================================================================
	E D I S E R V E R   L O G S Y S T E M

	File:		logsystem/logadd.c
	Programmer:	Mikko Valimaki
	Date:		Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1990-2013 Generix Group - All rights reserved.
============================================================================*/
#include "conf/local_config.h"
#include "conf/copyright.h"
COPYRIGHT()
MODULE("@(#)TradeXpress $Id: logadd.c 55499 2020-05-07 16:25:38Z jlebiannic $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 25.01.95/JN	Adapted from 3.1.1 logadd
  3.01 25.10.95/RV	Changed parameter order of dao_logoperator_read
  3.02 01.08.96/JN	INDEX/GENERATION are ignored when importing.
  4.00 13.10.98/KP	rls option
  4.01 10.07.02/CD	added -? option
  4.02 08.11.04/FM      Added management of dump > 2 giga
			Added management of binaries infos into dump files
  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "translator/translator.h"
#include "logsystem/lib/logsystem.definitions.h"

static int import_it(LogSystem *log);

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "";

static char *cmd;

static char *input_name = "(stdin)";

static int dispindex_flag = 0;
static int dumpimport_flag = 0;
static int importverbose_flag = 0;
static int import_rawmode = 0;
static int logsys_useTransaction = 0;

/* -------------------------------------------------------------------------- */
/* Function bail_out()                                                        */
/* -------------------------------------------------------------------------- */
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
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function fatal_exit()                                                      */
/* -------------------------------------------------------------------------- */
static void fatal_exit(char *msg)
{
	fprintf(stderr, "%s: %s\n", cmd, msg);
	exit(1);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function perror_exit()                                                     */
/* -------------------------------------------------------------------------- */
static void perror_exit(char *msg)
{
	int e = errno;

	fprintf(stderr, "%s: ", cmd);
	errno = e;
	perror(msg);
	exit(1);
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function main()                                                            */
/* -------------------------------------------------------------------------- */
int main(int argc, char **argv)
{
	extern char *optarg;
	extern int  optind, opterr, optopt;

	char        *sysname = NULL;
	LogOperator *logOperator = NULL;
	LogSystem   *log;
	LogEntry    *entry;
	FILE        *fp;
	int  	    c;


    tr_UseLocaleUTF8();

    dao_logsys_compability_setup();

	cmd = argv[0];
	opterr = 0;
    /* ---------------------------------------------------------------------- */
	/* Process command line options                                           */
	while ((c = getopt(argc, argv, "?zIOds:v:V:D:tTrl")) != -1)
    {
		switch (c)
        {
		/* Unknown option : invalid                                           */
		default:
			fprintf(stderr, "%s: Invalid option '%c'.\n",
				cmd, optopt);
			break;
		/* Help message                                                       */
		case '?':
			fprintf(stderr, "\
Valid options are:\n\
	-s sysname		logsystem name\n\
	-d			print created index to stdout\n\
	-v 'name op value'	insert an entry to values\n\
	-V valuefile		read values from file\n\
	-D dumpfile		import a dump\n\
	-T			trace import\n\
	-t			use transaction mode on base\n\
	-z 			raw for dump importing : write ALL informations as provided (including INDEX, CREATED ...)\n\
");
			return 2;
			break;
		/* logsystem name                                                     */
		case 's':
			sysname = optarg;
			break;
		/* undocumented option                                                */
        case 't' :
			logsys_useTransaction = 1;
			break;
		/* Print created index to stdout                                      */
		case 'd':
			dispindex_flag = 1;
			break;
		/* Insert an entry to values                                          */
		case 'v':
			dao_logoperator_add(&logOperator, optarg);
			break;
		/* Read values from file                                              */
		case 'V':
			if (!strcmp(optarg, "-")) {
				fp = stdin;
			} else {		
				 if ((fp = fopen(optarg, "r")) == NULL){
				perror_exit(optarg);
			}
		    }
			dao_logoperator_read(&logOperator,fp);
			if (fp != stdin){
				fclose(fp);
			}
			break;
		
		/* Import a dump                                                      */
		case 'D':
			dumpimport_flag = 1;

			if (strcmp(optarg, "-"))
            {
				input_name = optarg;

				/* 4.02/FM 
				   Open file in 64 bits mode and in binarie mode.
				   Then we can read file > 2 giga.
				*/
				#ifdef MACHINE_WNT
				if (freopen(input_name, "rb", stdin) == NULL) {
					perror_exit(input_name);
				}
				#else
/*				if (!freopen64(input_name, "r", stdin)) { */
				if (!freopen(input_name, "r", stdin)) {
					perror_exit(input_name);
				}
				#endif
			}
			break;
		/* States that the specified indexes should be consifered when        */
		/* creating imported entries in the new database                      */
        case 'z': import_rawmode = 1;     break;
		/* Trace import                                                       */
		case 'T': importverbose_flag = 1; break;
		}
	}
    /* ---------------------------------------------------------------------- */

	if (optind < argc) {
		fatal_exit("Excess arguments");
	}
	if (!sysname) {
		fatal_exit("No system name given");
	}

	if ((log = dao_logsys_open(sysname, LS_FAILOK)) == NULL) {
		perror_exit(sysname);
	}

	if ((logsys_useTransaction == 1)
	&&  (dao_logsys_begin_transaction(log) == -1)) {
		return 1;
	}
	

	if (dumpimport_flag) {
		import_it(log);
	} else {
		dao_logsys_compileoperator(log, logOperator);

		if ((entry = dao_logentry_new(log)) == NULL) {
			perror_exit("Cannot write entry");
		}
		dao_logentry_operate(entry, logOperator);

		if (dao_logentry_write(entry)) {
			perror_exit("Cannot write entry");
		}
		if (dispindex_flag) {
			printf("%d\n", dao_logentry_getindex(entry));
		}
	}
	

	if ((logsys_useTransaction == 1)
	&&  (dao_logsys_end_transaction(log) == -1)) {
		return 1;
	}
	
	return 0;
}
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Function import_it()                                                       */
/* -------------------------------------------------------------------------- */
static int import_it(LogSystem *log)
{
	LogOperator *logop = NULL;
	LogEntry *entry = NULL;
	int curidx =  0;
	int state  = -1;
	FILE *fp   = NULL;
	char *cp, *dirname;
	int lineno = 0;
	char buf[1024];
	char *fgetbuf;
	char path[1024];
	char c;
	int tempo;
			
#define EQ(x) (buf[0] == '-' && !strncmp(buf, x, sizeof(x) - 1))

    /* Loop reading the file and process each lines                            */
	fgetbuf = fgets(buf, sizeof(buf), stdin);
	while (fgetbuf != NULL)
    {
		lineno++;

		/* if the line begins with ---BASE: it gives the path of the export DB */
		if (EQ("---BASE:"))
        {
			state = 0;                        /* initial state : nothing to do */
			fgetbuf = fgets(buf, sizeof(buf), stdin);
			continue;
		}
		/* if the line begins with ---KEYINFO_BEGIN--- the following lines give field=value data */
		if (EQ("---KEYINFO_BEGIN---"))
        {
			entry = dao_logentry_new(log);
			if (entry == NULL)
            {
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				perror(log->sysname);
				exit(4);
			}
			curidx = dao_logentry_getindex(entry);
			state = 1;
			if (importverbose_flag) {
				fprintf(stderr, "\nI %d", curidx);
			}
			fgetbuf = fgets(buf, sizeof(buf), stdin);
			continue;
		}
		/* if the line begins with ---KEYINFO_END--- end of KEYINFO bloc of data */
		if (EQ("---KEYINFO_END---"))
        {
			dao_logentry_operate(entry, logop);
			if ( ( import_rawmode && dao_logentry_writeraw(entry) )
              || dao_logentry_write(entry)
               )
            {
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				perror(log->sysname);
				exit(5);
			}
			dao_logentry_free(entry);
			dao_logoperator_free(logop);
			entry = NULL;
			logop = NULL;
			fgetbuf = fgets(buf, sizeof(buf), stdin);
			continue;
		}
		/* if the line begins with ---FILE: it gives the extention file to fill with the following lines */
		/* the bloc ends at the next ---FILE: or at ---END: tags                                         */
		if (EQ("---FILE:"))
        {
			if (fp && fclose(fp))
			{
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				perror(path);
				exit(6);
			}
			dirname = buf + sizeof("---FILE:") - 1;

			cp = dirname + strlen(dirname);
			while ((--cp >= dirname) && (*cp == ' ' || *cp == '\n' || *cp == '\r')) {
				*cp = 0;
			}
			if (*dirname == 0)
			{
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				fprintf(stderr, "Missing file on line %d\n", lineno);
				exit(5);
			}
			if (LOGSYS_EXT_IS_DIR)
			{
				/*
				 *  Change
				 *  ---FILE:files/,ovt
				 *  into
				 *  ---FILE:files/ovt/
				 */
				if ( (cp = strrchr(dirname, ',')) )
				{
					strcpy(cp, cp + 1);
					strcat(cp, "/");
				}
				sprintf(path, "%s/%s", log->sysname, dirname);
				mkdir(path, 0755);
				sprintf(path + strlen(path), "%d", curidx);
			}
			else
			{
				/*
				 *  Separate
				 *  ---FILE:files/,ovt
				 *  "files/" and "ovt"
				 */
				if ( (cp = strrchr(dirname, ',')) ) {
					*cp++ = 0;
				}
				sprintf(path, "%s/%s", log->sysname, dirname);
				mkdir(path, 0755);
				sprintf(path + strlen(path), cp ? "%d.%s" : "%d", curidx, cp);
			}

			/* 4.02/FM 
			   Management of binaries infos in dump file :
			   We read the import file in binarie mode,
			   and write the result files (in syslog) in binarie mode.
			*/

#ifdef MACHINE_WNT
			fp = fopen(path, "wb");
#else
			fp = fopen(path, "w");
#endif
			
			if (fp == NULL)
			{
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				perror(path);
				if (logsys_useTransaction == 1) {
					dao_logsys_end_transaction(log);
				}
				exit(1);
			}

			if (importverbose_flag) {
				fprintf(stderr, " %s", dirname);
			}
			/* while reading extension (---FILE) 
			 * we should not include the last \n before next line
			 * so include in the search for the tag line */
#define END_TAG_SIZE 8
#define LEQ(x) (buf[0] == '\n' && !strncmp(buf, x, sizeof(x) - 1))

			/* read until we meet a tag line
			 * not to break the flow, rotate a small buffer 
			 * in buf for further use */
			tempo = 0;
			buf[0] = '\0';
			while (!feof(stdin) && !LEQ("\n---FILE") && !LEQ("\n---KEYI") && !LEQ("\n---END:"))
			{
				if (tempo > END_TAG_SIZE) {
					fputc(buf[0], fp);
				} else {
					tempo++;
				}
				c = fgetc(stdin);
				memmove(buf, buf+1,END_TAG_SIZE);
				buf[END_TAG_SIZE] = c;
			}
			
			/* finish reading the line to further proceed */
			cp = &buf[END_TAG_SIZE];
			while (!feof(stdin) && (*cp != '\n') && (*cp != '\r'))
			{
				++cp;
				*cp = fgetc(stdin);
			}
			*cp = '\0';

			/* back to normal, remove \n at start */
			memmove(buf, buf+1,strlen(buf)-1);

			continue;
		}
		if (EQ("---END:")) {
	
			break;
		}
		/* Process the lines following the tag                                     */
		switch (state)
        {
        /* bloc beginning with ---KEYINFO_BEGIN--- encountered                     */
		case 1:
			cp = buf + strlen(buf);
			while ((--cp >= buf) && (*cp == ' ' || *cp == '\n' || *cp == '\r')) {
				*cp = 0;
			}
			
			/* in raw mode, take the index given */
			if (import_rawmode && strncmp(buf, "INDEX=",sizeof("INDEX")) == 0) {
				curidx = atoi((const char *)(buf + sizeof("INDEX")));
			}
			dao_logoperator_add(&logop, buf);
			break;
		/*                                                                    */
        /* dead code : unreachable state  I guess                             */
		case 2:
			if (fputs(buf, fp) == EOF)
            {
				if (importverbose_flag) {
					fprintf(stderr, "\n");
				}
				perror(path);
				exit(7);
			}
			break;
		default:
		    /* nothing to do */
			break;
		}
		/* end_switch                                                         */
		fgetbuf = fgets(buf, sizeof(buf), stdin);
	}
    /* End_Loop reading the file and process each lines                       */

	if (importverbose_flag) {
		fprintf(stderr, "\n");
	}

	if (fp && fclose(fp)) {
		if (logsys_useTransaction == 1) {
			dao_logsys_end_transaction(log);
		}

		perror(path);
		exit(1);
	}
    return 0;
}
/* -------------------------------------------------------------------------- */
