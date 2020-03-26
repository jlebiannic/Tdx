/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_texttonum.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_texttonum.c 52804 2017-01-26 14:29:02Z tcellier $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 06.02.95/JN	Removed excess #includes
  3.02 12.01.17/SCH(CG) TX-2936	if text is nan or contains inf considere these 
                                values as not valid
============================================================================*/

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*============================================================================
============================================================================*/
double tr_TextToNum (char *text)
{
	double value;
	char   *p = NULL;
	char   sep;
	int    fields;
	int    point;

	if (tr_isempty (text)) {
		tr_Log (TR_WARN_EMPTY_TEXT_TO_NUM);
		return 0.0;
	}
	/*
	**  Be prepared for varying decimal separators!
	*/
	if (*tr_ADS && *tr_ADS != '.' && (p = strchr (text, *tr_ADS))) {
		*p = '.';
		sep = *tr_ADS;
	} else if (p = strchr (text, ',')) {
		*p = '.';
		sep = ',';
	}

	/*
	**  Try to extract a double out of the text.  If there
	**  are non numeric values in the string before any
	**  numerics, then fields will be assigned a zero.
	**  The consumption pointer (point) is used check
	**  that there are no extra characters after the
	**  possible double.
	*/
	fields = sscanf (text, "%lf%n", &value, &point);
	
	/* TX-2936 12/01/2017 SCH(CG) : if text is nan or contains inf considere these values as not valid */
	if (!strcmp(text, "nan") || !strcmp(text, "NAN") || strstr(text, "inf") || strstr(text, "INF")) { 
		fields = 0;
	}
	/* end TX-2936 */
	/*
	**  If we changed the separator, put it back before
	**  returning the control.
	*/
	if (p)
		*p = sep;

 	if (fields != 1) {
		tr_Log (TR_WARN_NON_NUMERIC_CONV, text);
		return 0.0;
	}
	if (!tr_isempty (text + point)) {
		tr_Log (TR_WARN_EXTRA_CHARS_IN_CONV, text);
		return 0.0;
	}
	return value;
}

