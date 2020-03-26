/*========================================================================
        E D I S E R V E R

        File:		tr_getopt.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Dec  2 12:22:03 EET 1996

        Copyright (c) 1996 Telecom Finland/EDI Operations

	NT misses getopt and as libruntime is used by almost everything,
	so here it is.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_getopt.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 02.12.96/JN	Created.
========================================================================*/

#include <stdio.h>
#ifdef MACHINE_NT
#include <io.h>
#endif

int   optind = 1;
int   opterr = 1;
int   optopt = 0;
char *optarg = NULL;

static int optoff = 1;

int getopt(int argc, char *const*argv, const char *optstring)
{
	char *cp;

	optarg = 0;

	/*
	 *  End of strings ?
	 *  Not starting with - ?
	 *  Just - ?
	 */
	if (optind >= argc || argv[optind][0] != '-' || argv[optind][1] == 0)
		return (-1);
		
	/*
	 *  -- ?
	 */
	if (argv[optind][1] == '-' && argv[optind][2] == 0) {
		optind++;
		return (-1);
	}

	if (optoff == 0)
		optoff = 1;

	optopt = argv[optind][optoff++];
	if (argv[optind][optoff] == 0) {
		optoff = 0;
		optind++;
	}
	for (cp = (char *) optstring; *cp && *cp != optopt; ++cp)
		;
	if (*cp == 0) {
		if (opterr)
			fprintf(stderr, "%s: Illegal option -- %c\n",
				argv[0], optopt);
		return ('?');
	}
	if (cp[1] == ':') {
		if (optind >= argc) {
			if (opterr)
				fprintf(stderr, "%s: Option requires an argument -- %c\n",
					argv[0], optopt);
			return ('?');
		}
		optarg = &argv[optind][optoff];
		optind++;
		optoff = 1;
	}
	if (cp[1] == '=') {
		if ( optoff > 0 ) {
		    optarg = argv[optind]+2;
		    optind++;
		    optoff = 0;
		}
	}

	return (optopt);
}

#if 0

main(int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "s:x")) != -1)
		printf("- %c '%s' '%s'\n",
			optopt, optarg ? optarg : "(null)",
			argv[optind] ? argv[optind] : "(null)");
	while (optind < argc)
		printf("'%s'\n", argv[optind++]);
}
#endif

