/*========================================================================
	E D I S E R V E R

	File:		tr_linematcher.c
	Programmer:	Juha Nurmela (JN)
	Date:		Fri Jul 29 13:33:52 EET 1994

	Input line matching, either
	from given index (1..length of line) or
	at end of line.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_linematcher.c 55166 2019-09-24 13:46:39Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 29.07.94/JN	created.
			Inputline matching was inefficient and
			handling of short lines after longer ones
			had undefined behavior, as length of current
			line was not checked before indexing into the line.
  TX-3123 - 06.06.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
========================================================================*/
#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*
 * Returns non-zero (true) if 'text' is in the match buffer at index 'indx'.
 * Empty strings are always found.
 * However, tcl does NOT call this function for empty strings.
 * indx : index in TCL, from 1 upwards
 * text : what we are looking for
 */
int tr_MatchBufHit(register int indx, register char *text)
{
	register char *matchp;
	/*
	 *  Seek into proper position in the match buffer.
	 *  This is done this way to prevent indexing over end of line.
	 *  If we hit EOL,
	 *  return true only if searched string was empty.
	 *  empty strings are found always.
	 *
	 *  indx starts from 1.
	 */
	if ( *text == '\0'  || indx < 1) { 
		return 1;
	}


	size_t n = indx - 1; /* skip indx - 1 mb chars */
	int skipped = tr_mbSkipChars( &n, tr_matchBuf );
	if ( skipped < 0 ) {
		tr_Log( "Bad encoding\n" ); /* TODO : externalize message */
		return 0;
	}
	if ( n > 0 ) {
		return 0;
	}
	matchp = tr_matchBuf + skipped;

	for (;;) {
		if (*text == 0) {
			return 1;
		}
		if (*text++ != *matchp++) {
			return 0;
		}
	}
}

/*
 * Returns non-zero (true) if 'text' is at the end of match buffer.
 * text : what we are looking for
 */
int tr_MatchBufHitEOL(char *text)
{
	register char *matchp;
	register char *textp;
	register char *match0 = tr_matchBuf;	/* start of match buffer */

	/*
	 * Find the end of both strings.
	 */
	for (matchp = match0; *matchp; ++matchp);
	for (textp = text; *textp; ++textp);

	/*
	 * Getting to start of text means we found the wanted text.
	 * Getting to start of matchbuffer means
	 *  there was not enough stuff in match buffer.
	 * Repeat until strings differ.
	 */
	for (;;) {
		if (textp == text)
			return (1);
		if (matchp == match0)
			return (0);
		if (*--textp != *--matchp)
			return (0);
	}
}

