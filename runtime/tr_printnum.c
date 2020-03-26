/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_printnum.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_printnum.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 10.10.92/MV	Created.
  3.01 27.02.95/JN	Simplified and corrected the problem with
			default decimal points (-177.1 gave -177.100).
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "tr_prototypes.h"

/*========================================================================
========================================================================*/

char *tr_PrintNum(double num, char *format, int starcount, int w1, int w2)
{
	char tmp[256];
	char tmp2[64];
	char *p, *p2;
	char *fmt2;

	if (format == NULL) {
		if (num == 0.0)
			return tr_MemPool ("0");

		/*
		**  printf the default, which gives 6 decimal digits,
		**  then clear zeroes from end. (update: can be changed by setlocale)
		**  p2 points to the . and
		**  p scans the end backwards.
		**
		**  This mess for finding out how many
		**  decimal digits the user probably wanted...
		*/
		sprintf (tmp, "%.6lf", num);
		p2 = strchr(tmp, '.');
		p = p2 + strlen(p2);
		while (*--p == '0')
			;
		sprintf (tmp, "%.*lf", (int)(p - p2), num);

		return tr_MemPool (tmp);
	}
	if (strchr (format, '.')) {
		switch (starcount) {
		case 0:
			sprintf (tmp, format, num);
			break;
		case 1:
			sprintf (tmp, format, w1, num);
			break;
		case 2:
			sprintf (tmp, format, w1, w2, num);
			break;
		}
		return tr_MemPool (tmp);
	}
	/*
	**  User gave only the width (possibly with padding, alignment etc).
	**  Again find out how many decimal digits to print...
	*/
	strcpy (tmp2, format);
	fmt2 = strchr (tmp2, 'l');
	if (num == 0.0) {
		sprintf (fmt2, ".0lf");
		if (starcount)
			sprintf (tmp, tmp2, w1, num);
		else
			sprintf (tmp, tmp2, num);
		return (tr_MemPool (tmp));
	}
	/*
	**  See comment in NULL-format case.
	*/
	sprintf (tmp, "%.6lf", num);
	p2 = strchr(tmp, '.');
	p = p2 + strlen(p2);
	while (*--p == '0')
		;

	sprintf (fmt2, ".%dlf", (int) (p - p2));
	if (starcount)
		sprintf (tmp, tmp2, w1, num);
	else
		sprintf (tmp, tmp2, num);

	return tr_MemPool (tmp);
}

