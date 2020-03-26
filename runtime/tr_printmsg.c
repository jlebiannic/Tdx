/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_printmsg.c
	Programmer:	Mikko Valimaki
	Date:		Fri Oct  9 14:41:11 EET 1992

	Copyright (c) 1997 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_printmsg.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.01 01.03.93/MV	Changed the "ERROR" text to be "*** CONTROL ELEM" or
			"*** CONTROL COMP" depending on the level.
  3.02 16.06.94/JN	Cosmetic change to align the ^ with EDIFACT errors.
  3.03 19.07.94/JN	Update to above.
  3.04 13.10.94/JN	tr_SegmentDiag reorganized. Previously component
			numbers in CONTRL-message lists were one too high.
  3.05 06.02.95/JN	Removed excess #includes
  3.06 24.02.95/JN	Grouping globals, tr_latestXXX now superseded
			by tr_segAddress.
  3.07 05.08.97/JN	Extending "control-data" for "segment-errors".
  4.00 29.08.02/JEM     Segment name in error messages instead of first component
  4.01 15.01.03/JEM     Involved by 4.00
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <stdio.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"
#include "bottom/tr_edifact.h"

extern MessageNode tr_msgNodes[];
extern MessageNode *tr_segAddress;
extern EdifactError *tr_segErrs;

#define ERROR_LISTING 1
#define ERROR_POSITIONS 2

/*============================================================================
============================================================================*/
int tr_PrintMessage (FILE *fp)
{
#if 0
	if (!trb_messagefile) {
		tr_Log (TR_WARN_MESSAGE_NOT_SAVED);
		return 0;
	}
	tr_PrintFile (trb_messagefile, fp, 0);
#endif
	tr_MessageDiag (2, fp);
    return 0;
}

/*============================================================================
============================================================================*/
#if 0
int
tr_WriteSegmentDiag (char *elems[], MessageNode *info, int mode, EdifactError *err, FILE *fp)
{
	static char *tmp[256];
	int i;

	tmp[0] = info->name;
	for (i = 1; i < info->nels; i++)
		tmp[i] = elems[i - 1];

	tr_SegmentDiag (tmp, info->nels, -1, info, mode, err, fp);
}
#endif

int tr_SegmentDiagnostic(int mode, FILE *fp)
{
	MessageNode *info = tr_segAddress;

	if (info)
	tr_SegmentDiag(info->array, tr_elemCount,
		tr_segCounter, info,
		mode, tr_segErrs, fp);
    return 0;
}

int tr_WriteSegmentDiagnostic(int mode, FILE *fp)
{
	MessageNode *info = tr_segAddress;

	if (info)
	tr_SegmentDiag(info->array, info->nels,
		tr_segCounter, info,
		mode, tr_segErrs, fp);
    return 0;
}

