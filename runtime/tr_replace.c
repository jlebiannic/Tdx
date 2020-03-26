/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_replace.c
	Programmer:	Juha Nurmela (JN)
	Date:		Fri Oct  9 11:01:41 EET 1992
==============================================================================*/
#include "conf/local_config.h"
#include <stdio.h>
#include <string.h>

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_replace.c 55061 2019-07-18 14:53:36Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  
  TX-3123 - 18.07.2019 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

char *tr_replace(char *s, char *was, char *tobe)
{
	if (tobe == NULL || *tobe == '\0') {
		return tr_strip(s, was);
	}
	size_t wsz = mbstowcs( NULL, s, 0 ) + 1;
	if (wsz <= 0) {
		/* invalid MB sequence */
		return TR_EMPTY;
	}
	wchar_t *wcstring = (wchar_t *) tr_malloc( wsz * sizeof(wchar_t) );
	mbstowcs( wcstring, s, wsz );
	wchar_t wcwas, wctobe;
	if (mbtowc( &wcwas, was, MB_CUR_MAX ) < 0
		|| mbtowc( &wctobe, tobe, MB_CUR_MAX ) < 0 ) {
		/* conversion failed */
		return TR_EMPTY; /* right thing to do ? */
	}

	wchar_t *wp = wcstring;
	while ( *wp != L'\0' ) {
		if ( *wp == wcwas ) {
			*wp = wctobe;
		}
		++wp;
	}

	size_t sz = wcstombs( NULL, wcstring, 0 ) + 1;
	char *string = (char *) tr_malloc( sz * sizeof(char) );
	wcstombs( string, wcstring, sz );

	tr_free( wcstring );

	/*
	 * Do not free, because garbage collection
	 * is gonna take care of that.
	 * tr_free(s);
	 **/
      return tr_MemPoolPass( string );
}
