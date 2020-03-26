/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strstr.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:31:09 EET 1992
==============================================================================*/
#include <string.h>
/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_strstr.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
==============================================================================*/

/*==============================================================================
	This function is called from the line matching.
	(It is here only because Risc/OS strstr() has a bug...)
==============================================================================*/
char *tr_strstr (char *s, char *t)
{
	int tlen;

	if (!s || !t)
		return (char *)0;
	tlen = strlen (t);
	while (*s) {
		if (*s == *t)
			if (!strncmp (s, t, tlen))
				return s;
		s++;
	}
	return (char *)0;
}

