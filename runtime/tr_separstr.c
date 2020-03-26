/*==========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_separstr.c
	Programmer:	Juha Nurmela (JN)
	Date:		Wed Feb 14 15:09:11 EET 1996
==========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_separstr.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*==========================================================================
  Record all changes here and update the above string accordingly.
  3.00 14.02.96/JN	Created.
==========================================================================*/

#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_prototypes.h"

/*==========================================================================
  Pick next token from string.
  Looks lot like tr_strcsep but can be used directly from TCL

  Uses tr_Assign() to keep leaks at the minimum.
  Does not touch input data,
  returned *token is okay with regard to TCL and memory-pool.

  State is kept in *sp, which should be a modifiable and trashable pointer.

  "" and ":" as sep gives "" once.
==========================================================================*/

int tr_SeparateString(char **sp, char *formseps, char **token)
{
	char *s = *sp;
	char c;

	/*
	 *  Use the default formseparator,
	 *  if none was specifically given,
	 *  or the value was empty,
	 *  or it contained only the first character
	 *  (FORMSEPS==: is the implied default)
	 */

	if (formseps == NULL || formseps == TR_EMPTY
	 || formseps[0] == 0 || (c = formseps[1]) == 0)
		c = ':';

	if (s && s != TR_EMPTY) {
		/*
		 *  Nonnull input.
		 *  Seek to terminating character
		 *  and set next location.
		 *  If we encounter end-of-string,
		 *  use simpler way to bld the return value.
		 */
		char *p;

		for (p = s; ; ++p) {
			if (*p == 0) {
				tr_Assign(token, s);
				*sp = NULL;
				break;
			}
			if (*p == c) {
				tr_Assign(token, tr_BuildText("%.*s", p - s, s));
				*sp = p + 1;
				break;
			}
		}
		/*
		 *  Can be empty string.
		 */
		return (1);
	}
	/*
	 *  No more.
	 *  If the loop exhausts,
	 *  the index-variable is set to EMPTY.
	 */
	tr_AssignEmpty(token);

	return (0);
}

