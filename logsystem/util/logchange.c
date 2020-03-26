/*============================================================================
	E D I S E R V E R   L O G S Y S T E M

	File:		logsystem/logchange.c
	Programmer:	Mikko Valimaki
	Date:		Wed Aug 12 09:29:29 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
#include "conf/copyright.h"

/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 24.01.95/JN	Adapted from 3.1.1 logchange
  3.01 21.07.97/JN	errno.h included for multithread definitions.
  3.02 10.07.02/CD	added -? option.
  3.03 13.03.13/YLI(CG)	TX-460 if fields are wrong in the filters,
								display errors and terminate the program  
  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

extern int  errno;
extern int tr_useSQLiteLogsys;

#include "logsystem/lib/logsystem.h"
#include "translator/translator.h"

static char *cmd;

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "";

void bail_out(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", cmd);
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (errno){
		fprintf(stderr, " (%d,%s)\n", errno, syserr_string(errno));
	}
	exit(1);
}

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

	char        *sysname = NULL;
	LogIndex    absIndex = 0;
	LogFilter   *logFilter = NULL;
	LogOperator *logOperator = NULL;
	LogSystem   *log		 = NULL;
	LogEntry    *entry	     = NULL;
	FILE        *fp			 = NULL;
	int  	    c			 = 0;
	/*YLI(CG) 13.03.13 TX-460 
	 nunk stocks the nombre of no exist fields in the filters */
	int  nunk = 0;
	FilterRecord	*g			= NULL;
	FilterRecord	*end		= NULL;

	int logsys_useTransaction = 0;

    tr_UseLocaleUTF8();

    logsys_compability_setup();

    /* only revelant it local mode 
     * in rls, this is looked at server side */
    if (getenv("USE_SQLITE_LOGSYS") != NULL){
        tr_useSQLiteLogsys = atoi(getenv("USE_SQLITE_LOGSYS"));
		}
    else{
        tr_useSQLiteLogsys = 0;
		}

	cmd = argv[0];
	opterr = 0;
	while ((c = getopt(argc, argv, "?OItms:i:f:F:v:V:")) != -1)
    {
		switch (c)
        {
		default:
			fprintf(stderr, "%s: Invalid option '%c'.\n",
				cmd, optopt);
		case '?':
			fprintf(stderr, "\
Valid options are:\n\
	-s sysname		logsystem name\n\
	-m			    (obsolete)\n\
	-i index		absolute index to use\n\
	-f 'name op value'	insert an entry to filter\n\
	-F filterfile		read filter from file\n\
	-v 'name op value'	insert an entry to values\n\
	-V valuefile		read values from file\n\
	-O 			the base is an old legacy one\n\
	-I 			the base is a  sqlite one\n\
	-t			use transaction mode on sqlite base\n\
");
			exit (2);
		case 's':
			sysname = optarg;
			break;
        case 'O' :
            tr_useSQLiteLogsys = 0;
			break;
        case 'I' :
            tr_useSQLiteLogsys = 1;
			break;
        case 't' :
			logsys_useTransaction = 1;
			break;
		case 'm':
			break;
		case 'i':
			absIndex = atoi(optarg);
			break;

		case 'f':
			/* Add one field into filter. */
			logfilter_add(&logFilter, optarg);
			break;
		case 'F':
			/* Read filter from file. */
			if (!strcmp(optarg, "-")){
				fp = stdin;
				}
			else if ((fp = fopen(optarg, "r")) == NULL){
				perror_exit(optarg);
				}

			logfilter_read(&logFilter, fp);
			if (fp != stdin){
				fclose(fp);
				}
			break;

		case 'v':
			/* Add one operator. */
			logoperator_add(&logOperator, optarg);
			break;
		case 'V':
			/* Read ops from file. */
			if (!strcmp(optarg, "-"))
            {
				fp = stdin;
            }
			else
            {
                if ((fp = fopen(optarg, "r")) == NULL)
                {
				    perror_exit(optarg);
                }
            }

			logoperator_read(&logOperator, fp);
			if (fp != stdin){
				fclose(fp);
				}
			break;
		}
	}
	if (optind < argc){
		fatal_exit("Excess arguments", 0);
		}

	if (!sysname){
		fatal_exit("No system name given", 0);
		}

	if ((log = logsys_open(sysname, LS_FAILOK)) == NULL){
		perror_exit(sysname);
		}

	if ((logsys_useTransaction == 1)
	&&  (logsys_begin_transaction(log) == -1)){
		exit(1);
		}

	logsys_compileoperator(log, logOperator);

	if (absIndex)
    {
		if ((entry = logentry_readindex(log, absIndex)) == NULL)
		{
			if (logsys_useTransaction == 1){
				logsys_begin_transaction(log);
				}
			fatal_exit("Entry %d is unreadable", absIndex);
		}

		logentry_operate(entry, logOperator);

		if (logentry_write(entry))
		{
			if (logsys_useTransaction == 1){
				logsys_begin_transaction(log);
				}
			fatal_exit("Cannot rewrite entry %d", absIndex);
		}
	}
    else
    {
		/*YLI(CG) 13.03.13 TX-460 
		nunk = 0: field exists */
		nunk = logsys_compilefilter(log, logFilter);
		if (nunk != 0)
        {
			end = &logFilter->filters[logFilter->filter_count];
			for (g = logFilter->filters; g < end; ++g)
            {
				g->field = NULL;
				g->field = logsys_getfield(log, g->name);
				if (!g->field){
					fprintf (stderr, "Error: Field %s does not exist\n",g->name);
					}
			}
			exit(0);
		}
		/*End*/
		logsys_operatebyfilter(log, logFilter, logOperator);
	}

	if ((logsys_useTransaction == 1)
	&&  (logsys_end_transaction(log) == -1)){
		exit(1);
		}

	exit (0);
}

