/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_loaddef.c
	Programmer:	Mikko Valimaki
	Date:		Thu Dec 10 23:46:35 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_loaddef.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 25.02.93/MV	Default defaults are now loaded at any case: before
			they came in only when nothing else was available.
  3.02 ??.??.??/??	Close now the file in the end.
  3.03 01.04.93/KV	extern double atof(char *) definition added.
  3.04 06.02.95/JN	Removed excess #includes
  3.05 22.09.95/JN	Made packing of elements and/or components an option.
			Added also $EDIHOME/lib/tclrc and incremental loading
			of defaults (EDIHOME, HOME), and removed strtok:s.
			Removed the 'char *' from atof declaration,
			and added stdlib.h.
  3.06 30.04.96/JN	Unknown options cause no warnings.
  3.07 30.03.99/KP	tr_useLocalLogsys
  3.08 13.05.99/HT	fclose replaced with tr_fclose to handle remote files
  4.00 20.12.06/LM  Don't clean trailing spaces option
  4.01 26.09.13/DBT(CG) TX-2437 : binary httpreceive.cgi.exe falls segmentation violation
  TX-3123 - 28.05.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits

============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif
#include <ctype.h>
#include <string.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"
#include "../bottom/tr_edifact.h"

FILE *tr_fopen();

extern double atof();

/* All global data should be gathered here,
 * instead of being scattered throughout the system. */
int tr_doPackComponents;
int tr_doPackElements;
int tr_useLocalLogsys;
int tr_useDao; // Jira TX-3199 DAO: id du DAO à créer

/* Can be set at translator.h
 * If set to 1, unknown options are always warned.
 * If set to -1, only when stderr is to a terminal.
 * Otherwise no complaining. */
int do_complain_about_unknown_tclopt;

static void loadit();

/*======================================================================
======================================================================*/
#define RELAX_BIT	1
#define TEXT_VAR	2
#define DOUBLE_VAR	3
#define INT_VAR		4
#define MULTI_SEP	5

struct defval {
	char *name;
	int  type;
	void *value;
} TCLdefaults[] = {
	{ "INVALID_CONTENT",		RELAX_BIT,	(void *)RELAX_CHARCHECK },
	{ "MISSING_ELEMENTS",		RELAX_BIT,	(void *)RELAX_MISSINGELEMENTS },
	{ "MISSING_COMPONENTS",		RELAX_BIT,	(void *)RELAX_MISSINGCOMPONENTS },
	{ "EXCESS_COMPONENTS",		RELAX_BIT,	(void *)RELAX_EXCESSCOMPONENTS },
	{ "EXCESS_ELEMENTS",		RELAX_BIT,	(void *)RELAX_EXCESSELEMENTS },
	{ "TOO_LONG_ELEMENTS",		RELAX_BIT,	(void *)RELAX_TOOLONGELEMENTS },
	{ "TOO_SHORT_ELEMENTS",		RELAX_BIT,	(void *)RELAX_TOOSHORTELEMENTS },
	{ "MISSING_SEGMENTS",		RELAX_BIT,	(void *)RELAX_MISSINGSEGMENTS },
	{ "MISSING_GROUPS",			RELAX_BIT,	(void *)RELAX_MISSINGGROUPS },
	{ "TOO_MANY_SEGMENTS",		RELAX_BIT,	(void *)RELAX_TOOMANYSEGMENTS },
	{ "TOO_MANY_GROUPS",		RELAX_BIT,	(void *)RELAX_TOOMANYGROUPS },
	{ "GARBAGE_OUTSIDE",		RELAX_BIT,	(void *)RELAX_GARBAGEOUTSIDE },
	{ "GARBAGE_INSIDE",			RELAX_BIT,	(void *)RELAX_GARBAGEINSIDE },
	{ "EMPTY_SEGMENTS",			RELAX_BIT,	(void *)RELAX_EMPTYSEGMENTS },
	{ "FIXED_NUMS",				RELAX_BIT,	(void *)RELAX_FIXEDNUMS },
	{ "XERCES_ERROR",			RELAX_BIT,	(void *)RELAX_XERCES },
	{ "INHOUSE_DECIMAL_SEP",	TEXT_VAR,	(void *)&tr_ADS },
	{ "MESSAGE_DECIMAL_SEP",	TEXT_VAR,	(void *)&tr_MDS },
	{ "LOGGING_LEVEL",			DOUBLE_VAR,	(void *)&tr_errorLevel },
	{ "ARRAY_INDEX_WIDTH",		INT_VAR,	(void *)&tr_bufIndexWidth },
	{ "CODE_CONVERSION_BUFFER",	INT_VAR,	(void *)&tr_codeConversionSize },
	{ "EOF_STRING",				TEXT_VAR,	(void *)&TR_EOF },
	{ "PACK_COMPONENTS",		INT_VAR,	(void *)&tr_doPackComponents },
	{ "PACK_ELEMENTS",			INT_VAR,	(void *)&tr_doPackElements },
	{ "USE_LOCAL_LOGSYS",		INT_VAR,	(void *)&tr_useLocalLogsys },
	{ "USE_SQLITE_LOGSYS",		INT_VAR,	(void *)&tr_useSQLiteLogsys },
	{ "CLEAN_TRAILING_SPACES",	INT_VAR,	(void *)&tr_cleanTrailingSpaces },
	{ "MULTI_SEPARATOR",		MULTI_SEP,	(void *)&tr_multisep },
	{ "USE_DAO",				INT_VAR,	(void *)&tr_useDao },
	{ NULL,						0,			NULL }
};

/*======================================================================
======================================================================*/
static int tr_DefaultDefaults (void)
{
	/* These two TRUE for historic reasons. */
	tr_doPackElements   = 1;
	tr_doPackComponents = 1;

	/* Initialise rte constants and variables 
	   to avoid segfault using tr_Assign variable must not be initialized as constant  by the compiler */
	TR_EMPTY = tr_strdup("\0TradeXpress\0");
	TR_EOF = tr_strdup("__EOF__");
	TR_FILLCHAR = tr_strdup(" ");
	TR_NEWLINE = tr_strdup("\n");
	TR_IRS = tr_strdup("\n");
	tr_multisep = tr_strdup(":");
	tr_ADS = tr_strdup(".");
	tr_MDS = tr_strdup(".");


	tr_errorLevel = 1.0;
	tr_bufIndexWidth = 4;
	tr_codeConversionSize = 1024;

	if ((do_complain_about_unknown_tclopt == -1) && (isatty(2)))
		do_complain_about_unknown_tclopt = 1;

    /* rls mode by default */
	tr_useLocalLogsys = 0;
    tr_cleanTrailingSpaces = 1;
    /* sqlite by default */
    tr_useDao = 0;
    
    return 0;
}

/*======================================================================
======================================================================*/
int tr_LoadDefaults (char *filename)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	FILE *fp;
	char *cp;
	char buffer[1024];

	tr_DefaultDefaults ();

	if (!filename) {
		/*
		**  No explicit defaults-file given.
		**  Read defaults from standard places.
		**  sysad can put global defaults to .../lib/
		**  and users can override those with their own.
		**
		**  Do not complain if opening fails for any reason.
		**
		**  1: $EDIHOME/lib/tclrc
		*/
		if ((cp = getenv("EDIHOME"))) {
			sprintf (buffer, "%s/lib/tclrc", cp);
			if ((fp = tr_fopen (buffer, "r")) != NULL) {
				loadit(fp);
				tr_fclose(fp, 0);
			}
		}
		/*
		**  2: $HOME/.tclrc
		*/
		if ((cp = getenv("HOME"))) {
			sprintf (buffer, "%s/.tclrc", cp);
			if ((fp = tr_fopen (buffer, "r")) != NULL) {
				loadit(fp);
				tr_fclose(fp, 0);
			}
		}
	} else {
		/* BEGIN - 26/09/2013 DBT(CG) - TX-2437 : move code in a function and call it */
		tr_LoadDefaultFile(filename);
		/* END - 26/09/2013 DBT(CG) - TX-2437 */
	}
	return (0);
}

/* BEGIN - 26/09/2013 DBT(CG) - TX-2437 : move code in a function and call it */
int tr_LoadDefaultFile (char *filename)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	FILE *fp;
	
		/*
		 *  Explicit defaults-filename given.
		 *
		 *  Read _only_ that and
		 *  complain if it does not exist.
		 */
		if ((fp = tr_fopen (filename, "r")) != NULL) {
			loadit(fp);
			tr_fclose(fp, 0);
		} else {
			tr_Log (TR_WARN_CANNOT_GET_DEFAULTS, filename, errno);
	}	
		
	return (0);
}
/* END - 26/09/2013 DBT(CG) - TX-2437 */

static void loadit(FILE *fp)
{
	struct defval *def;
	char *name;
	char *value;
	char *p;
#ifdef MACHINE_WNT
	char *p2;
#endif
	char buffer[1024];

	while ((p = tr_mbFgets (buffer, sizeof(buffer), fp))) {

		/*
		 * Lines should be like one of these:
		 * XYZ
		 * FOO=BAR
		 * FOO
		 *
		 * First, skip whitespace.
		 */
#ifdef MACHINE_WNT
		p2 = p + strlen(p) - 2;
		if (p2 >= p && !strcmp(p2, "\r\n"))
			*p2 = 0;
#endif
		while (*p && isspace (*p)) {
			++p;
		}

		/*
		 * Here starts the name.
		 * Name ends in =, # or whitespace.
		 */
		for (name = p; *p; ++p) {
			if (*p == '='
			 || *p == '#'
			 || isspace(*p)) {
				break;
		}
		}
		/*
		 * How did it end ?
		 * = means we have a value,
		 * # means there is no value (boolean by existence).
		 * Otherwise the name ended in whitespace -> skip this line.
		 */
		switch (*p) {
		case '=':
			*p++ = 0;
			value = p;
			break;
		case '#':
		case '\n':
			*p = 0;
			value = NULL;
			break;
		default:
			continue;
		}
		/*
		 * If the name is "" after all,
		 * skip the line.
		 * Otherwise, seek end of value (# or newline).
		 */
		if (*name == 0) {
			continue;
		}

		for ( ; *p; ++p) {
			if (*p == '#'
			 || *p == '\n') {
				break;
		}
		}
		*p = 0;

		/*
		**  Look up the name in the table.
		*/
		for (def = TCLdefaults; def->name; ++def) {
			if (!strcmp (def->name, name)) {
				goto found;
			}
		}

		if (do_complain_about_unknown_tclopt == 1) {
			tr_Log (TR_WARN_UNKNOWN_DEFAULT, name);
		}

		continue;
	found:
		/*
		 * No value, use "" (atox returns 0).
		 */
		if (value == NULL) {
			value = "";
		}

		switch (def->type) {
		case RELAX_BIT:
			if (!strcmp(value, "0")) {
				tr_dorelax((unsigned long) def->value);
			} else {
				if (!strcmp(value, "1")) {
				tr_dounrelax((unsigned long) def->value);
				}
			}
			break;
		case TEXT_VAR:
			*((char **) def->value) = tr_strdup (value);
			break;
		case MULTI_SEP:
			*((char **) def->value) = tr_strdup (value);
			if ( !strcmp(value, "\\007")) {
				sprintf(*((char **) def->value), "%c", '\007');
			} else {
				if (strcmp(value, ":"))
				{
					fprintf (stderr, "Multisep defined in $HOME/.tclrc is an invalid character.\n");
					exit (1);
				}
			}
			break;
		case INT_VAR:
			*((int *) def->value) = atoi(value);
			break;
		case DOUBLE_VAR:
			*((double *) def->value) = atof(value);
			break;
		}
	}
}
