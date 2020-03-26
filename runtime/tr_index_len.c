/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_index_len.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 10:10:42 EET 1992
==============================================================================*/
#include <string.h>
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_index_len.c 55060 2019-07-18 08:59:51Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
==============================================================================*/

/*==============================================================================
 *   TX-3123 - 10.07.2019 - Denis Brunet - UNICODE adaptation
 *
==============================================================================*/
double tr_Index (char *s, char *t)
{
	int i = 1;
	int tlen;

	if (!s || !t)
		return 0;

	tlen = strlen (t);
	while (*s) {
		int nextcharlen = mblen(s, MB_CUR_MAX);
		if (*s == *t)
			if (!strncmp (s, t, tlen)) {
				return i;
			}
		s+=nextcharlen;
		i++;
	}
	return 0;
}

double tr_IndexAfter (char *s, char *t, double d_offset)
{
	int i = 1;
	int tlen;
	int offset=(int)(d_offset+0.5);

	if (!s || !t)
		return 0;

	tlen = strlen (t);
	while (*s) {
		int nextcharlen = mblen(s, MB_CUR_MAX);
		if (i>=offset)
		{
			if (*s == *t)
				if (!strncmp (s, t, tlen))
					return i;
		}
		s+=nextcharlen;
		i++;
	}

	return 0;
}


/*============================================================================
============================================================================*/

double tr_Length (char *text)
{
	int len = tr_mbLength(text);
	if (len < 0) {
		tr_Log( "Encodage invalide" );
		return -1.0;
	}
	return (double) len;
}

