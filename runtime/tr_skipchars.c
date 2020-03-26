/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_encoding.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_encoding.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
   
========================================================================*/

#include <stdlib.h>
/*
#include "tr_externals.h"
#include "tr_privates.h"
*/
#include "tr_prototypes.h"

/*
 * skips N multi-bytes logical chars in string
 * returns the physical length of skipped chars
 * returns -1 if string is not a valid Multi-byte string (ie is not a valid UTF-8 string)
 * After execution, N contains the numbers of chars that have not been skipped
 * 
 * tr_mbSkipChars( 10, "1234567é" ) should return 9 (9 physical chars skipped) and N should have a value of 1
 */
int tr_mbSkipChars( size_t *n, char *ptr ) {
	/* reset shift state */
	mblen( NULL, MB_CUR_MAX );

	int physicalLength = 0;
	char *p = ptr;

        int clen = 0;
	while ( *n > 0 ) {
        	clen = mblen(p, MB_CUR_MAX);
		if ( clen <= 0 ) 
			break;
                p += clen; /* advance to next MB char */
		--*n;
		physicalLength += clen;
        }
	if (clen < 0) {
		/* bad encoding */
		return -1;
	}
	return physicalLength;
}


