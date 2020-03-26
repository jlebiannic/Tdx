/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_split.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 09:56:38 EET 1992
============================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_split.c 55062 2019-07-22 12:35:02Z sodifrance $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 29.06.93/MV	tr_MakeIndex received an integer as argument instead of
			the double it was expecting.  This caused some havoc
			and it was found when the receiving end was unable to
			handle the content correctly!!!
  3.02 06.02.95/JN	Removed excess #includes
  3.03 21.04.95/JN	Int-makeindex. Split leaked when no match was found.
  3.04 14.02.96/JN	strtok replaced with strsep.
			Array now always cleared, even when input-text empty.
  3.05 05.03.07/ADV	Pointer to gestion of text's position in case of error
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation

============================================================================*/

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*======================================================================
  Split given text into array[1] ... array[n] and return n.

  XXX Uses only the first character in separator !
======================================================================*/
double tr_Split (char *text, char *array, char *separator, char *delimitor, char *escape)
{
	char *seppos;
	char *p;
	char *workCopy;
	int  n;
	int abspos;

	if ( separator && mblen(separator, MB_CUR_MAX) > 1 ) {
		/* multibytes seps not allowed */
		/* TODO : warning ? Error ? Use default sep ? */
	}
	if (  delimitor && mblen(delimitor, MB_CUR_MAX) > 1 ) {
		/* multibytes seps not allowed */
		/* TODO : warning ? Error ? Use default sep ? */
	}
	if (  escape && mblen(escape, MB_CUR_MAX) > 1 ) {
		/* multibytes not allowed */
		/* TODO : warning ? Error ? Use default sep ? */
	}

	/*
	**  Clean up possible previous array.
	**  XXX Done always, even when returning zero
	*/
	tr_am_textremove (array);

	if (text == NULL || text == TR_EMPTY || *text == 0)
		return (0);

	/*
	**  Make a copy from the original string (and remember to free).
	*/
	n = 0;
	/* 3.05 AVD Pointer to count text's position in case of lack of 
	 *          delimitor error before a separator.
	 */
	abspos = 0;
	workCopy = tr_strdup(text);
	seppos = workCopy;

	if (delimitor && escape)
	{
		while (p = tr_strcpick(&seppos, *separator, *delimitor, *escape, &abspos))
		{
			tr_am_textset (array, tr_IntIndex(++n), p);
			free(p);
		}
	}
	else
	{
		while (p = tr_strcsep(&seppos, *separator))
			tr_am_textset (array, tr_IntIndex(++n), p);
	}

	tr_free (workCopy);

	return (n);
}

