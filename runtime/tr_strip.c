/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strip.c
	Programmer:	Juha Nurmela (JN)
	Date:		Fri Oct  9 11:01:41 EET 1992
==============================================================================*/
#include "conf/local_config.h"
#include <stdio.h>
#include <string.h>

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
MODULE("@(#)TradeXpress $Id: tr_strip.c 55060 2019-07-18 08:59:51Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

char *tr_strip(char *s, char *garbage)
{
	char *p, *q;

	while( *s != '\0' && tr_mbStrChr(garbage, s) != NULL)
		s += mblen( s, MB_CUR_MAX);

	p = q = s = tr_MemPool(s);

	while( *p != '\0' ) {
		if( tr_mbStrChr(garbage, p) == NULL ) {
			int count = mblen( p, MB_CUR_MAX);
			char * cp = p;
			while (count > 0) {
				*q++ = *cp++;
				--count;
			}
		}
		p += mblen( p, MB_CUR_MAX );
	}
	*q = 0;
	return s;
}

