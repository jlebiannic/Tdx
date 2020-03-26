/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_pick.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Wed May  6 16:01:43 EET 1992

	Debugger functions are included by defining DEBUG_CALLS to
	a any value.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_pick.c 55060 2019-07-18 08:59:51Z sodifrance $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	created.
  3.01 29.07.94/JN	unnecessary copying of returnbuffer removed.
			unnecessary clearing of temporary buffer removed.
			Substr modified.
  3.02 28.12.94/JN	Upped the buffer length.
  3.03 06.02.95/JN	Removed excess #includes
  4.01 21.09.12/GBY     tr_Substr: string entry param is not directly manipulated,
			we use a copy to advoid Segmentviolation under Unix
  4.02 29.10.13/CDE     TX-2474 tr_Substr: liberate memory used by the copy of the parameter 
  TX-3123 - 14.05.2019 - Olivier REMAUD - UNICODE adaptation

========================================================================*/

#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <locale.h>
#include <errno.h>

#include "tr_constants.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

#define TR_MAX_WINDOW_LINES	1024
#define TR_NEW_WINDOW           1
#define TR_INSIDE_A_WINDOW      2
#define TR_EATING_UP            3


char *tr_GetNextLine (int usage);
static char *tr_winBuffer[TR_MAX_WINDOW_LINES];

/*==============================================================================
==============================================================================*/
char *tr_Pick (double line, double pos, double len, int pickEOL)
{
	int  i;
	int  iLine = (int)line;
	char *picked;

#ifdef DEBUG_CALLS
	debugpick (0,0,0);	/* Clears the "pick display" */
#endif
	if (iLine < 1) {
		tr_Log (TR_WARN_ILLEGAL_LINENUMBER, iLine);
		return TR_EMPTY;
	}
	/*
	**  Check if the window has a maximum size, and if so then check
	**  whether the picking would be over that.
	*/
	if (tr_winSize && (iLine > tr_winSize)) {
		tr_Log (TR_WARN_PICK_OUTSIDE_WINDOW, iLine, tr_winSize);
		return TR_EMPTY;
	}
	/*
	**  Do we already have the line in the buffer or not?
	**	iLine		runs from 1 to n
	**	tr_curHighLine	runs from 0 to n - 1, where 0 indicates empty buffer.
	**	tr_winBuffer	The first element is never used (tr_winBuffer[0]).
	*/
	if (iLine > tr_curHighLine) {
		for (i = tr_curHighLine + 1; i <= iLine; i++) {
			if (i >= TR_MAX_WINDOW_LINES)
			{
				tr_Fatal (TR_FATAL_TOO_MANY_WINDOW_LINES);
			}
			/*
			**  The tr_GetNextLine() routine places the
			**  line to the tr_winBuffer.
			*/
			if (!tr_GetNextLine (TR_INSIDE_A_WINDOW)) {
				tr_Log (TR_WARN_UNEXPECTED_EOF);
				return TR_EOF;
			}
		}
	}
	picked = tr_Substr (tr_winBuffer[iLine], pos, len, pickEOL);
	if (picked != TR_EMPTY) {
		;
#ifdef DEBUG_CALLS
		int iPos = (int)pos - 1;
		int iLen = (int)len;
		debugpick (iLine, iPos, iLen);
#endif
	}
	return picked;
}

/*==============================================================================
==============================================================================*/
/*
	4.01 - GBY 
	 Modification : string entry param is not directly manipulated, 
 	 we use a copy to advoid Segmentviolation under Unix
	4.02 - CDE
	 free memory used by the copy of the parameter. 
*/
char *tr_mbSubstr (char *p_s, double pos, double len, int pickEOT)
{	
	char *result = NULL; /* 4.02 - result value */
	int iPos = (int)pos - 1; /* substr position in C convention */
	int iLen = (int) len; /* substr length */
	
	/*
	 *  Garbage in, garbage out
	 */
	if (p_s == NULL || p_s == TR_EMPTY) {
		return TR_EMPTY;
	}
	

	if (iPos < 0) {
		tr_Log (TR_WARN_ILLEGAL_POSITION, iPos);
		return TR_EMPTY;
	}

	/* Skip iPos MB chars */
	int toSkip = iPos;
	int atEnd = 0;
	char *start = p_s;
	while ( toSkip > 0 )
	{
		int nextCharLen = mblen(start, MB_CUR_MAX);
		if ( nextCharLen == 0 ) {
			/* At end */
			atEnd = 1;
			break;
		} else if ( nextCharLen < 0 ) {
			/* Error : cannot decode */
			tr_Log ("Illegal encoding");
			return TR_EMPTY;
		} else {
			--toSkip;
			start += nextCharLen;
		}
	}
	
	/*
	 *  This accepts picking the trailing zero,
	 *  like in "kalevi" -> pick(1,7,EOL) -> ""
	 */

	if (atEnd) {
		tr_Log (TR_WARN_POSITION_AFTER_TEXT);
		return TR_EMPTY;
	}
	
	if (pickEOT) {
		return tr_MemPool( start );
	}

	/*
	**  If we are picking a specified length, then check
	**  that it makes sense.
	*/
	if (iLen <= 0) {
		tr_Log (TR_WARN_NEGATIVE_LENGTH, iLen);
		return TR_EMPTY;
	}

	char *end = start;
	toSkip = iLen;
	atEnd = 0;
	
	while (toSkip > 0) 
	{
		int nextCharLen = mblen(end, MB_CUR_MAX);
		if ( nextCharLen == 0 ) {
			/* At end */
			atEnd = 1;
			break;
		} else if ( nextCharLen < 0 ) {
			/* Error : cannot decode */
			tr_Log ("Illegal encoding");
			return TR_EMPTY;
		} else {
			--toSkip;
			end += nextCharLen;
		}
	}
	
	if (atEnd) {
		/*
		*  Pick from startpoint to end of source string.
		*/
		return tr_MemPool( start );
	}
	
	int lenToCopy = end - start;
	char *sub = (char*) tr_malloc(lenToCopy + 1);
	memset(sub, 0, lenToCopy + 1 );
	strncpy(sub, start, lenToCopy );
	result = tr_MemPool( sub );
	
	tr_free( sub );
	
	return result;
}

char *tr_Substr (char *p_s, double pos, double len, int pickEOT)
{
	return tr_mbSubstr( p_s, pos, len, pickEOT );
}

/*======================================================================
======================================================================*/
void tr_EatUp (void)
{
	int  i;
	char *tmp;

	for (i = tr_curHighLine + 1; i <= tr_winSize; i++) {
		if (!(tmp = tr_GetNextLine (TR_EATING_UP))) {
			tr_Log (TR_WARN_UNEXPECTED_EOF);
			return;
		}
	}
}

/*======================================================================
======================================================================*/
char *tr_GetNextLine (int usage)
{
	int  i;
	char buffer[TR_STRING_MAX_SIZE + 1];

	if (usage == TR_NEW_WINDOW) {
		/*
		**  Free the previous contents from the buffer and
		**  initialize the high line pointer to zero.
		*/
		for (i = 1; i <= tr_curHighLine; i++) {
			if (tr_winBuffer[i]) {
				tr_free (tr_winBuffer[i]);
			}
			tr_winBuffer[i] = NULL;
		}
#ifdef DEBUG_CALLS
		debugskiplines (tr_curHighLine);
#endif
		tr_curHighLine = 0;
		/*
		**  See if the return buffer has stuff in it.
		**  If it has, use it first.
		**  Move memory from returnbuffer into winbuffer.
		**  No need to copy, as returnbuffer is already
		**  duplicated.
		*/
		if (tr_returnBuffer != TR_EMPTY) {
			char *p = tr_returnBuffer;

			tr_returnBuffer = TR_EMPTY;
			tr_curHighLine = 1;
#ifdef DEBUG_CALLS
			debugread (0, p);
#endif
			return tr_matchBuf = tr_winBuffer[1] = p;
		}
	}
	/*
	**  Read the next line from standard input.
	*/
	char *text = tr_mbFgets(buffer, sizeof(buffer), stdin );
	if ( text != NULL ) {
		/* Remove RC at end of line */
		tr_stripRC( text );
		/*
		**  Increment the global line count.
		*/
		TR_LINE++;
#ifdef DEBUG_CALLS
		debugread (1, text);
#endif
		switch (usage) {
		case TR_NEW_WINDOW:
			return tr_matchBuf = tr_winBuffer[tr_curHighLine = 1] = tr_strdup (text);
			break;
		case TR_INSIDE_A_WINDOW:
			return tr_winBuffer[++tr_curHighLine] = tr_strdup (text);
			break;
		case TR_EATING_UP:
			/*
			**  For eating up purposes we only indicate
			**  that there was someting (as opposed to
			**  EOF indicated with NULL).
			*/
			return TR_EMPTY;
			break;
		default:
		;
		}
	}
	/*
	**  Return NULL to indicate EOF.
	*/
	return NULL;
}
