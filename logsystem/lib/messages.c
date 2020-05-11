/*========================================================================
        E D I S E R V E R

        File:		logsystem/messages.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Warning and fatal messages.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: messages.c 55487 2020-05-06 08:56:27Z jlebiannic $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>

#include "logsystem.dao.h"

static char * dao_lsname(LogSystem *ls)
{
	char *name = "Logsys";
	char *cp, *slash;

	if (ls)
    {
		name = ls->sysname;
		cp = name + 1;
		while ((slash = strchr(cp, '/')) != NULL)
        {
			name = cp;
			cp = slash + 1;
		}
	}
	return (name);
}

void dao_logsys_warning(LogSystem *ls, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);

	fprintf(stderr, "%s: ", dao_lsname(ls));
	vfprintf(stderr, fmt, ap);
	fprintf(stderr, "\n");
	fflush(stderr);

	va_end(ap);
}

void dao_debug_logsys_warning(LogSystem *ls, char *fmt, ...)
{
	va_list ap;

    if (getenv("DAO_DEBUG") != NULL)
    {
        va_start(ap, fmt);

        fprintf(stderr, "%s: ", dao_lsname(ls));
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, "\n");
        fflush(stderr);

        va_end(ap);
    }
}

void dao_logsys_trace(LogSystem *ls, char *fmt, ...)
{
	va_list ap;
	static int dotrace = -1;

	if (dotrace == -1)
    {
		if (getenv("LOGSYSTEM_TRACE"))
			dotrace = 1;
		else
			dotrace = 0;
	}
	if (dotrace)
    {
		va_start(ap, fmt);

		fprintf(stderr, "%s trace: ", dao_lsname(ls));
		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		fflush(stderr);

		va_end(ap);
	}
}

char * dao_syserr_string(int error)
{
    char *error_desc;

    if(!(error_desc=(char*)strerror(error)))
    {
        /* should not happen */
        fprintf(stderr,"Fatal error : Unknown error code : %d\n",error);
        exit(1);
    }
    return (error_desc);
}
