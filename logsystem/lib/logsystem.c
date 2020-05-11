#include "conf/local_config.h"
/*========================================================================
        TradeXpress

        File:		logsystem.dao.h
        Author:		Frédéric Heulin (FH/Fredd)
        Date:		Fri Feb 17 10:53:15 CET 2006

        Copyright (c) 2006 Illicom groupe Influe

==========================================================================
  @(#)TradeXpress $Id: logsystem.c 55495 2020-05-06 14:41:40Z jlebiannic $ $Name:  $
  Record all changes here and update the above string accordingly.
  3.00 17.02.06/FH	Copied from logsystem.h
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

/* this is the only file where you should those three included */
#include "private.h"
#include "logsystem.h"


/* are we migrating from old legacy to sqlite ? 
 * by default : no */
int logsys_migrate= 0;

char *liblogsystem_version = "@(#)TradeXpress $Id: logsystem.c 55495 2020-05-06 14:41:40Z jlebiannic $";
int liblogsystem_version_n = 3014;

extern double	tr_errorLevel;

void writeLog(int level, char *fmt, ...) {
	va_list ap;
	int dotrace = 0;
	
	if (getenv("LOGSYSTEM_TRACE")) {
		dotrace = 1;
		fprintf(stderr, "[LOGSYSTEM_TRACE (%.f)] ", tr_errorLevel);
	} else if (level <= tr_errorLevel) {
		dotrace = 1;
		switch (level) {
			case LOGLEVEL_NONE:
				fprintf(stderr, "[ ERROR ] ");
				break;
			case LOGLEVEL_NORMAL:
				fprintf(stderr, "[trace] ");
				break;
			case LOGLEVEL_DEBUG:
				fprintf(stderr, "[debug] ");
				break;
			case LOGLEVEL_DEVEL:
				fprintf(stderr, "[devel] ");
				break;
			default:
				fprintf(stderr, "[debug (%d)] ", level);
				break;
		}
	}
	if (dotrace > 0) {

		va_start(ap, fmt);

		vfprintf(stderr, fmt, ap);
		fprintf(stderr, "\n");
		fflush(stderr);

		va_end(ap);
	}
}

