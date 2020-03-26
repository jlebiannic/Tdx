/*========================================================================
        E D I S E R V E R

        File:		logsystem/messages.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Warning and fatal messages.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: messages.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/types.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

extern double	tr_errorLevel;

static char *
old_legacy_lsname(ls)
	LogSystem *ls;
{
	char *name = "Logsys";
	char *cp, *slash;

	if (ls) {
		name = ls->sysname;
		cp = name + 1;
		while ((slash = strchr(cp, '/')) != NULL) {
			name = cp;
			cp = slash + 1;
		}
	}
	return (name);
}

void old_legacy_logsys_warning(LogSystem *ls, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", old_legacy_lsname(ls));
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fflush(stderr);

	va_end(ap);
}

void old_legacy_logsys_trace(LogSystem *ls, char *fmt, ...)
{
	va_list ap;
	static int dotrace = -1;

	if (dotrace == -1) {
		/* add test of LOGLEVEL to trace.
		Choice of LOGLEVEL of 2 or more to avoid a lot of trace in normal and published way */
		if (getenv("LOGSYSTEM_TRACE") || (tr_errorLevel > 2)) { dotrace = 1; }
	}
	if (dotrace > 0) {
		va_start(ap, fmt);

		fprintf(stderr, "%s trace (%.lf): ", old_legacy_lsname(ls), tr_errorLevel);
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		fflush(stderr);

		va_end(ap);
	}
}

char *
old_legacy_syserr_string(error)
	int error;
{
    char *error_desc;

    if(!(error_desc=(char*)strerror(error))) {
        /* should not append */
        fprintf(stderr,"Fatal error : Unknown error code : %d\n",error);
        exit(1);
    }
    return (error_desc);
}

