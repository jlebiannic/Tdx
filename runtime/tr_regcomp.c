/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_regcomp.c
	Programmer:	Juha Nurmela (JN)
	Date:		Mon Mar 22 18:19:31 EET 1993
===========================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)*/
MODULE("@(#)TradeXpress $Id: tr_regcomp.c 54894 2019-03-25 09:50:38Z cdemory $")
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 29.03.93/JN	Temporary fix, HP regcomp is from perse.
  3.02 08.02.95/JN	Removed casts and added prototypes, (alpha...).
  3.03 21.04.95/JN	Split compile and execute.
  3.04 26.01.99/JR  POSIX regular expressions...
  3.05 22.02.19/CD  Jira TX-31115 regexp in line were generating not always same code for tr_RE 
===========================================================================*/

#include <stdio.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#ifdef MACHINE_LINUX
#include <regex.h>
#define POSIX_REGCOMP
#endif

/* NT4 STX4.2.2 doesn't have regex
#ifdef MACHINE_WNT
#include "regex.h"
#define POSIX_REGCOMP
#endif
*/
/*
 * Pre-compile regular expression.
 * Return pointer to opaque re-structure,
 * or 0 on failure.
 */
void *tr_regular_compile(char *expr)
{
	void *re = 0;
#ifdef POSIX_REGCOMP
/* This is Ugly!
 * regcomp will be done in tr_regexec.c/tr_regular_execute
 * expr will be remapped to re.
 */
	re = expr;
#else
	re = regcmp(expr, (char *) 0);
#endif
	return (re);
}

/*
 * Free compiled re-structure
 */
void tr_regular_free(void *re)
{
/* Nothing here to free with POSIX */
#ifndef POSIX_REGCOMP
	if (re)
		free(re);
#endif
}

/*
 * Print the re as C-code into file.
 */
void tr_regular_print(FILE *fp, void *re)
{
	int n = 256;
	int col = 0;
	unsigned char *ucp = (unsigned char *) re;

#ifdef POSIX_REGCOMP
	/*
	 * TX-3115/CD For Linux, we need only to print the content of re as a string, stoping at the first 0 found.
	 *            as before we were printing 256 bytes from re, resulting in not always printing the same content
	 *            as we were printing outside the content of the string pointed to by re.
	 */
	int expfinished = 0;

	while ((--n >= 0) && (!expfinished) ) {
		if (col >= 70) {
			col = 0;
			fprintf(fp, "\n");
		}
		if (*ucp == (char) 0) {
			expfinished = 1;
			col += fprintf(fp, "0");
		} else {
			col += fprintf(fp, "%d,", *ucp++);
		}
	}
#else
	while (--n >= 0) {
		if (col >= 70) {
			col = 0;
			fprintf(fp, "\n");
		}
		col += fprintf(fp, "%d,", *ucp++);
	}
#endif
}

