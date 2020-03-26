/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_bufwrite.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:01:41 EET 1992
===========================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_bufwrite.c 55100 2019-08-21 13:30:39Z sodifrance $")
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 10.06.94/JN	fillString did not end in zero-byte. purify!
  3.02 06.02.95/JN	Removed excess #includes.
  3.03 08.03.95/JN	Plugged a memory leak when array was not flushable.
			Also reorganized put() and flush() a little.
			flush() now prints the first line even when it has
			not been initialized.
  3.04 21.04.95/JN	MakeIndex to IntIndex.
       13.02.96/JN	tr_am_ names and argument to tr_amtext_getpair().
  3.05 12.02.04/LM	error checked in tr_Flush, function nfGetFlushErr() used
			to retrieve these errors.
  Bug 1219 08.04.11/JFC free given parameters if they are in the memory pool
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
===========================================================================*/

#include <stdio.h>
#include <string.h>
#ifndef MACHINE_WNT
#include <strings.h>
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#ifdef MACHINE_LINUX 
#include <errno.h> 
#else
extern int errno;
#endif


/* 3.05 */
int flusherr;
double nfGetFlushErr(void)
{
 return flusherr;
}

/*===========================================================================
===========================================================================*/

void tr_Put (char *arrayname, double line, double pos, char *value)
{
	int iLine = (int) line;
	int iPos  = (int) pos;
	int len, valueLen;
	int logicalLen, valueLogicalLen; /* logical lengths according to MB encoding (UTF-8) */
	char **theLine; /* Robs the pointer from the array */
	char *curLine;  /* manipulator to data in above */
	char *index;

	/*
	**  The tradition is to make a few important looking checks:
	*/
	if (!value)
		return;
	if (iLine < 1)
		tr_Log (TR_WARN_PLACE_ILLEGAL_LINE, iLine);
	if (iPos < 1) {
		tr_Log (TR_WARN_PLACE_ILLEGAL_POS, iPos);
		iPos = 1;
	}
	/*
	**  The positions run from 1 onward, but in C from zero
	**  so lets adjust.
	**  tr_IntIndex reuses a few static buffers, do not free index.
	*/
	--iPos;
	index = tr_IntIndex (iLine);

	/*
	**  Read in the line.
	**
	**  tr_am_text invents the line as "",
	**  if it did not exist already.
	*/
	theLine = tr_am_text (arrayname, index);
	curLine = *theLine;

	valueLen = (int) strlen (value);
	logicalLen = (int) tr_mbLength( curLine );

	/*
	**  If this is a new or empty line or the resulting
	**  string would exceed the current, we do some
	**  memory management for starters.
	**  As we robbed the pointer from the array, we can
	**  manipulate it freely, as long as we put something back...
	*/
	
	/* check if there is enough size to store value */
	/* this is not trivial because we can have MB chars in both replaced part, remaining part and value */
	
	/* compute physical size of remaining chars after the copy (in case of overwrite */
	/* |<-skipped part->|<-value->|<-remaining chars->| */ 
	
	int neededSize = 0;
	len = strlen (curLine);

	/* check if there is enough place until index */
	size_t skippedCount = iPos;
	int skippedPos = tr_mbSkipChars( &skippedCount, curLine );
	if (skippedPos < 0 ) {
		/* TODO externalize message */
		tr_Log( "Bad UTF-8 encoding" );
		return;
	}
	size_t replacedCount = 0;
	int replacedPos = 0;
	int eraseAll = 0; /* Do we write up to the actual physical length of string or not */
	if ( skippedCount > 0 ) {
		/* index is after physical end of string */
		skippedPos += skippedCount; /* padding char is one byte wide */
		neededSize = skippedPos; /* add skipped physical chars */
		neededSize += valueLen; /* add physical size of value */
		eraseAll = 1;
	} else {
		neededSize = skippedPos; /* physical size of existing string before index */

		/* now see if we erase all of remaining string or not */
		valueLogicalLen = tr_mbLength( value ); /* logical length of value */
		replacedCount = valueLogicalLen; /* skip as much logical chars as there are in value */
		replacedPos = tr_mbSkipChars( &replacedCount, curLine + skippedPos );
		if ( replacedCount > 0 ) {
			/* we erase all of remaining string */
			neededSize += valueLen; /* add physical size of value */
			eraseAll = 1;
		} else {
			neededSize += valueLen; /* add physical size of value */
			neededSize += len - replacedPos - skippedPos; /* remaining size */
		}
	}
	if ( neededSize > len )  {
		curLine = tr_realloc (curLine, neededSize + 1);
		curLine[neededSize] = '\0';
		if (!curLine) {
			tr_OutOfMemory();  /* exit on error */
		}
		*theLine = curLine;
	}
	if (iPos > logicalLen) {
		/*
		**  Writing starts beyond existing text.
		**  Stuff fill characters as many as needed...
		*/
		memset( curLine + len, *TR_FILLCHAR, neededSize - len );
	}

	/* now copy remaining in correct position if needed */
	if ( !eraseAll ) {
		int physicalOldPos = skippedPos + replacedPos;
		int physicalNewPos = skippedPos + valueLen;
		if ( physicalOldPos > physicalNewPos ) {
			/* copy from left to right */
			int oldIdx = skippedPos + replacedPos;
			int newIdx = skippedPos + valueLen;
			while (oldIdx <= len ) { /* <= : also copy final 0 */
				curLine[newIdx] = curLine[oldIdx];
				++oldIdx;
				++newIdx;
			}
		} else if ( physicalOldPos < physicalNewPos ) {
			/* copy from right to left */
			int oldIdx = len - 1;
			int newIdx = neededSize - 1;
			while ( oldIdx >= replacedPos + skippedPos ) {
				curLine[newIdx] = curLine[oldIdx];
				--oldIdx;
				--newIdx;
			}
		} /* else same size so nothing to do */
	}

	/*
	**  ...and then the text itself.
	**
	**  Exact length is used to avoid the terminating zero.
	**  If the added text continues past the old text,
	**  we must remember to terminate the line.
	*/
	memcpy (curLine + skippedPos, value, valueLen);

	/* free parameters if they are in the pool */
	tr_MemPoolFree(arrayname);
	tr_MemPoolFree(value);
	return;
}

static char *makefiller (char, int);

/*======================================================================
======================================================================*/

void tr_Flush (char *arrayname, double minCol, double maxCol, char *separator, FILE *fp, int keep)
{
	void *tr_am_rewind(char *);
	struct am_node *arrayhandle;
	char *index;
	char *curLine;
	char *fillString;
	int  previousLine;
	int  trailer;
	int  iLine;
	int  iMinCol = (int)minCol;
	int  iMaxCol = (int)maxCol;
	/*
	**
	*/
	
/* 3.05 */
	flusherr = 0;
	if (iMinCol < 0) {
		tr_Log (TR_WARN_FLUSH_ILLEGAL_MIN, iMinCol);
		iMinCol = 0;
	}
	if (iMaxCol < 0) {
		tr_Log (TR_WARN_FLUSH_ILLEGAL_MAX, iMaxCol);
		iMaxCol = 0;
	}
	if (iMinCol > iMaxCol)
		tr_Log (TR_WARN_FLUSH_MIN_GT_MAX, iMinCol, iMaxCol);

	if (iMinCol)
		fillString = makefiller (*TR_FILLCHAR, iMinCol);

	previousLine = 0;
	arrayhandle = tr_am_rewind(arrayname);

	while (tr_amtext_getpair (&arrayhandle, &index, &curLine)) {
		if (!(iLine = atoi (index)) || (iLine < previousLine)) {
			tr_Log (TR_WARN_NOT_FLUSHABLE, arrayname);
			if (iMinCol)
				tr_free (fillString);
			return;
		}
		while (++previousLine < iLine) {
			/*
			**  Print the empty lines...
			*/
			if (iMinCol)
				/* 3.05 */
				if (tr_mbFputs(fillString, fp) == EOF)
				{
					if (errno != 0)
						flusherr = errno; 
					else
						flusherr = 1;
				}
			/* 3.05 */
			if (tr_mbFputs(separator, fp) == EOF)
			{
				if (errno != 0)
					flusherr = errno; 
				else
					flusherr = 1;
			}
		}
		previousLine = iLine;
		size_t logicalLen = tr_mbLength( curLine );
		if (iMaxCol && (logicalLen > iMaxCol)) {
			tr_Log (TR_WARN_LONG_LINE_TRUNCATED, iLine, curLine);
			/* 3.05 */
			if (tr_mbFnputs(curLine, fp,iMaxCol) < 0)
			{
				if (errno != 0)
					flusherr = errno; 
				else
					flusherr = 1;
			}

		} else if (iMinCol && ((trailer = iMinCol - logicalLen) > 0)) {
			/* 3.05 */
			if (tr_mbFputs (curLine, fp) == EOF)
			{
				if (errno != 0)
					flusherr = errno; 
				else
					flusherr = 1;
			}
			/* 3.05 */
			if (tr_mbFnputs( fillString, fp, trailer ) < 0)
			{
				if (errno != 0)
					flusherr = errno; 
				else
					flusherr = 1;
			}

		} else
			/* 3.05 */
			if (tr_mbFputs (curLine, fp)== EOF)
			{
				if (errno != 0)
					flusherr = errno; 
				else
					flusherr = 1;
			}

		/* 3.05 */
		if (tr_mbFputs (separator, fp)== EOF)
		{
			if (errno != 0)
				flusherr = errno; 
			else
				flusherr = 1;
		}

	}
	if (!keep)
		tr_am_textremove(arrayname);
	if (iMinCol)
		tr_free (fillString);

	return;
}

/*======================================================================
======================================================================*/
static char *makefiller (char fill, int width)
{
	char *tmp;

	if (width < 0)
		width = 0;
	tmp = tr_malloc (width + 1);
	if (!tmp) {
		tr_OutOfMemory();  /* exit on error */
	}
	memset (tmp, fill, width);
	tmp[width] = '\0';		/* [3.01] */
	return tmp;
}

