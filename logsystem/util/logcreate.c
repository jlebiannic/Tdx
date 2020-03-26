#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: logcreate.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
 * Record all changes here and update the above string accordingly.
 * TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
 * Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
 * ========================================================================*/
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <stdarg.h>
#include <errno.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "translator/translator.h"

#include "build.h"

extern int tr_useSQLiteLogsys;

/* dummy variables */
double tr_errorLevel = 0;
char*  tr_programName = "";

int Quiet = 0;
int Really = 1;
char *Sysname;
char *Cfgfile;

static char *cmd;

void bail_out(char *fmt, ...)
{
	va_list ap;

	fprintf(stderr, "%s: ", cmd);
	va_start(ap,fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	if (errno)
		fprintf(stderr, " (%d,%s)\n", errno, syserr_string(errno));
	exit(1);
}

/* Lot like strrchr(path, '/') for finding out the basename. NULL when no pathseparators on string. */
static char * find_last_sep(char *path)
{
	char *sep = NULL;
	for ( ; ; ++path)
    {
		switch (*path)
        {
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

static char * str4dup(char *a,char *b,char *c,char *d)
{
	char *s = log_malloc(strlen(a) + strlen(b) + strlen(c) + strlen(d) + 1);
	strcpy(s, a);
	strcat(s, b);
	strcat(s, c);
	strcat(s, d);
	return (s);
}

main(int argc, char **argv)
{
	extern char *optarg;
	extern int  optind, opterr, optopt;

    int resetBase = 0;
	struct opt options;
	char *cp;
	int c;
    int rc = 0;

	options.rereadCfg = 0;
	options.checkAffinities = 0;

	tr_UseLocaleUTF8();

    /* sqlite mode only */
    tr_useSQLiteLogsys = 1;
    cmd = strdup(argv[0]);
	opterr = 0;
	while ((c = getopt(argc, argv, "h:s:f:NqrRC")) != -1)
    {
		switch (c)
        {
		default:
		case 'h':
			fprintf(stderr, "Possible options:\n\
	-s sysname\n\
	-f cfgfile\n\
	-q (Be quiet)\n\
	-N (Dont actually create/reconfig system)\n\
	-r In the RARE case you have exhausted all the unique indexes, reset the base counter (consider deep cleaning the base before this)\n\
	-R reread cfgfile for a base (allow ONLY resizing or adding fields, need 5.0.5+ install)\n\
	-C check consistency (of sqlite affinities with cfgfile)\n\
");
			exit(2);
		case 's': Sysname = optarg; break;
		case 'f': Cfgfile = optarg; break;
		case 'N': Really = 0;       break;
		case 'q': Quiet = 1;        break;
		case 'r': resetBase = 1;    break;
		case 'R': options.rereadCfg = 1;       break;
		case 'C': options.checkAffinities = 1; break;
		}
    }

	while (optind < argc)
    {
		char *arg = argv[optind];

		if (strlen(arg) > 4 && !strcmp(arg + strlen(arg) - 4, ".cfg"))
        {
			Cfgfile = argv[optind++];
			continue;
		}
		if (!Sysname)
        {
			Sysname = argv[optind++];
			continue;
		}
		break;
	}
	if (!Sysname && Cfgfile)
    {
		if ((cp = find_last_sep(Cfgfile)) != NULL)
			++cp;
		else
			cp = Cfgfile;

		Sysname = strdup(cp);
		if ((cp = strrchr(Sysname, '.')) != NULL)
			*cp = 0;
	}
	if (!Cfgfile && Sysname)
    {
		if ((cp = find_last_sep(Sysname)) != NULL)
			++cp;
		else
			cp = Sysname;

		Cfgfile = str4dup(Sysname, "/", cp, ".cfg");
	}
	if (!Cfgfile || !Sysname)
    {
		fprintf(stderr, "Need system name and description.\n");
		exit(2);
	}

    if (resetBase == 1)
    {
        logsys_resetindex(Sysname);
    }
    else
    {
        rc = logsys_buildsystem(Sysname, Cfgfile, options);
    }

    free(cmd);
    exit(rc);
}

