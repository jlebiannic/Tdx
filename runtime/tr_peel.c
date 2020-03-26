/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_peel.c
	Programmer:	Juha Nurmela (JN)
	Date:		Fri Oct  9 11:01:41 EET 1992
==============================================================================*/
#include "conf/local_config.h"
#include <stdio.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_peel.c 55060 2019-07-18 08:59:51Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/

char *tr_peel(char *s, char *garbage)
{
	char *p;
	char *end;
	int inGarbage;

	while(*s != '\0' && tr_mbStrChr(garbage, s) != NULL) {
		s += mblen(s, MB_CUR_MAX);
	}
	s = tr_MemPool(s);
	p = s;
	
	end = NULL;
	inGarbage = 0;
	while(*p != '\0') {
		if ( tr_mbStrChr(garbage, p) != NULL ) {
			if ( !inGarbage ) {
				inGarbage = 1;
				end = p;
			}
		} else {
			inGarbage = 0;
			end = NULL;
		}
		p += mblen(p, MB_CUR_MAX);
	}

	if (end != NULL) {
		*end = '\0';
	}

	return s;
}

