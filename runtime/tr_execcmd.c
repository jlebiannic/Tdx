/*========================================================================
        E D I S E R V E R

        File:		tr_execcmd.c
        Author:		Juha Nurmela (JN)
        Date:		Wed Apr 26 10:29:22 EET 1995

        Copyright (c) 1995 Telecom Finland/EDI Operations

	execute commandline with /bin/sh.
	_XXX_ are expanded before commandline is given to shell.

	exec is NOT prepended to line,
	a process can be saved by doing that before calling this.

	Commandline is given as a one string, thus it must already
	include any "" escapes for unusual single arguments.
	Our substitutions never create such, only integers
	and "easy" pathnames replace _INDEX_, _FILE_ etc.

========================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_execcmd.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 26.04.95/JN	Created.
========================================================================*/

#include <stdio.h>
#include <string.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

#ifndef MACHINE_LINUX
extern int errno;
#endif

double nfExecCommand(
	char  *cmdline,
	double index,    /* _INDEX_ */
	char  *filename, /* _FILE_ */
	char  *parfile)  /* _PARAMFILE_ */
{
	char *cmd, *new, *pos, *add;
	int addlen, rv;

#if 0
	/*
	 * This would prevent using ; in commands.
	 */
	add = "exec "; addlen = strlen(add);
	cmd = tr_malloc(strlen(cmdline) + addlen + 1);
	strcpy(cmd, add);
	strcpy(cmd + addlen, cmdline);
#else
	cmd = tr_strdup(cmdline);
#endif

	/*
	 * Find a fresh substr (overlaps get ignored) in cmd
	 * and make a copy expanding the place of the substr.
	 * Ugly as hell but works.
	 */
#define SUBSTITUTE(x, y)                                        \
    add = y; addlen = strlen(add); pos = cmd;                   \
    while (pos = strstr(pos, x)) {                              \
	new = tr_malloc(strlen(cmd) + addlen + 1);              \
	strcpy(new, cmd);                                       \
	strcpy(new + (pos - cmd), add);                         \
	strcpy(new + (pos - cmd + addlen), pos + sizeof(x) - 1);\
	pos = new + (pos - cmd) + 1;                            \
	tr_free(cmd);                                           \
	cmd = new;                                              \
    }

	SUBSTITUTE("_INDEX_",      tr_IntIndex((int) index))
	SUBSTITUTE("_FILE_",       filename)
	SUBSTITUTE("_PARAMFILE_",  parfile)

	rv = system(cmd);

	if (rv == -1) {
		/*
		 * Fork failed ?
		 */
		tr_Log(TR_ERR_CANNOT_CREATE_PROCESS, errno);

	} else if (rv & 0xFF) {
		/*
		 * Abnormal termination.
		 */
		tr_Log(TR_MSG_TERMINATED_BY_SIGNAL, cmd, rv & 0x7F);
		if (rv & 0x80)
			tr_Log(TR_MSG_CORE_DUMPED);
		rv = -1;
	} else {
		/*
		 * Return child status in the second byte.
		 */
		rv >>= 8;
		rv &= 0xFF;
	}
	tr_free(cmd);

	return (rv);
}

