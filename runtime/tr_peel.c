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
MODULE("@(#)TradeXpress $Id: tr_peel.c 55484 2020-05-05 09:18:01Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 15.07.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 05.05.2020 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/


char *tr_peel_simple(char *s, char garbage) {
	char *p = s;
	while (p != '\0' && *p == garbage) {
		++p;
	}
	s = tr_MemPool(s);
	p = s;
	p += strlen(s); /* at \0 */
	--p; /* at last char */
	while ( p >= s && *p == garbage ) {
		*p = '\0'; /* strip */
		--p; /* at new last char */
	}
	return s;
}

char *tr_peel(char *s, char *garbage)
{
	char *p;
	char *end;
	int inGarbage;

	/* if one char to strip && this is an ASCII char just use a simplified vresion */
	if (garbage != NULL && garbage[0] != '\0' && garbage[1] == '\0' && garbage[0] >= 32 && garbage[0] <= 127) {
		return tr_peel_simple( s, garbage[0] );
	}

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
		int sz = mblen(p, MB_CUR_MAX);
		if ( sz > 0) {
			p += mblen(p, MB_CUR_MAX);
		} else {
			/* bad encoding */
			/* best way to fail ? */
			/* for now just ignore */
			return s;			
		}
	}

	if (end != NULL) {
		*end = '\0';
	}

	return s;
}



