/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_mbstrchr.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_mbstrchr.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
   
========================================================================*/

#include <stdlib.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*
 * Find the first occurence of the multibyte char uniqueCharToFind
 * Returns NULL if not found
 * Returns a pointer to the first byte of char if found
 */ 

char* tr_mbStrChr( char *string, char *uniqueCharToFind ) {

	char tmp[MB_CUR_MAX + 1];

	int sz = mblen( uniqueCharToFind, MB_CUR_MAX );
	if (sz <= 0) {
		return NULL;
	}
	memset( tmp, 0, sizeof(tmp) );
	memcpy( tmp, uniqueCharToFind, sz );

	return strstr( string, tmp );
}

