/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_REsplit.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 09:29:09 EET 1992

	Splitting text based on regular expressions.
===========================================================================*/
#include "conf/local_config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_REsplit.c 47429 2013-10-29 15:27:44Z cdemory $")
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 21.04.95/JN	Compile-time creation of RE's. Big memset removed.
===========================================================================*/

#include <stdio.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*===========================================================================
  regex the `text' against `re' and put subexpressions into `array'.
  Number of subexpressions is returned (flaky).
===========================================================================*/

double tr_REsplit (char *text, char *array, void *re)
{
	char *rs[10];
	int  count = 0;
	int  i;

	/*
	**  Clean up possible previous array.
	*/
	tr_am_textremove (array);

	tr_regular_clear();
	if (tr_regular_execute ((char *)re, text) == NULL) {
		/*
		**  No match at all.
		*/
		return (0);
	}
	/*
	**  Something matched.
	*/
	tr_regular_getsubs(rs, 10);

	for (i = 0; i < 10; i++) {
		if (*rs[i]) {
			tr_am_textset (array, tr_IntIndex(i), rs[i]);
			count++;
		}
	}
	return (count);
}

