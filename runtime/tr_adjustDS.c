/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_adjustDS.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_adjustDS.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.01 06.02.95/JN	adjust DS moved into separate file (this).
==============================================================================*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

#include "../bottom/tr_edifact.h"

/*============================================================================
============================================================================*/

char *tr_AdjustDS (char *value)
{
	char *p;

	for (p = value; *p; p++) {
		if (TR_ISDECIMALPOINT(*p)) {
			if (*tr_ADS)
				*p = *tr_ADS;
			else
				tr_Log (TR_WARN_NO_ADS_SPECIFIED);
			/* Break the loop after the first occurence, since the second
			 * decimal separator would be an  error. */
			break;
		}
	}
	return value;
}

