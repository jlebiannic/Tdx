/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_makeindex.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:46:24 EET 1992
============================================================================*/
#include "conf/config.h"
/*LIBRARY(libruntime_version)
*/
MODULE("@(#)TradeXpress $Id: tr_makeindex.c 47429 2013-10-29 15:27:44Z cdemory $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 21.04.95/JN	Streamlined, integer-argument, version of MakeIndex.
============================================================================*/

#include <stdio.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

#define MAX_IDXLEN 15

/*
 * This can be reset anytime if needed to change index width.
 */
int tr_makeIndexInited = 0;

static char dIndexFormat[64];    /* becomes "%05.0lf" in init */
static char iIndexFormat[64];    /* becomes "%05.0d"  in init */

static double dUpperLimit;
static int    iUpperLimit;

static void init_limit();

/*============================================================================
============================================================================*/
char *tr_MakeIndex (double value)
{
	char tmp[MAX_IDXLEN + 2];

	if (!tr_makeIndexInited)
		init_limit();

	/*
	**  Check that the line number is within our limits.
	**  Variable tr_bufIndexWidth contains the maximum width
	**  for the index.
	*/
	if (value > dUpperLimit)
		tr_Fatal (TR_ERR_ARRAY_INDEX_TOO_LARGE, value);

	sprintf (tmp, dIndexFormat, value);

	return tr_MemPool (tmp);
}

/*============================================================================
  Faster (internal-use only) version of the above.
  Has a couple of static buffers for the index-string.
============================================================================*/
char *tr_IntIndex (int value)
{
	static char tmps[32][MAX_IDXLEN + 1];
	static int  n = 0;

	if (!tr_makeIndexInited)
		init_limit();

	if ((unsigned) value > iUpperLimit)
		tr_Fatal(TR_ERR_ARRAY_INDEX_TOO_LARGE, (double) value);

	if (++n >= 32)
		n = 0;

	sprintf(tmps[n], iIndexFormat, value);
	return (tmps[n]);
}

/*
 *  Calculate the upper limit for the index based on the
 *  maximum allowed width of the index.
 *  This is done only once in one process.
 */
static void init_limit()
{
	int i;

	/*
	**  Make sure it fits to our allocated buffer.
	*/
	if (tr_bufIndexWidth < 1) {
		tr_Log (TR_WARN_NEGATIVE_INDEX_WIDTH, tr_bufIndexWidth);
		tr_bufIndexWidth = 4;
	}
	if (tr_bufIndexWidth > MAX_IDXLEN) {
		tr_Log (TR_WARN_HUGE_INDEX_WIDTH, tr_bufIndexWidth);
		tr_bufIndexWidth = 4;
	}
	dUpperLimit = 1.0;
	iUpperLimit = 1;

	for (i = 0; i < tr_bufIndexWidth; i++) {
		dUpperLimit *= 10;
		iUpperLimit *= 10;
	}
	dUpperLimit--;
	iUpperLimit--;

	sprintf (dIndexFormat, "%%0%d.0lf", tr_bufIndexWidth);
	sprintf (iIndexFormat, "%%0%dd",    tr_bufIndexWidth);

	tr_makeIndexInited = 1;
}

