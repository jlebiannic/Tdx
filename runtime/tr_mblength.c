/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_mblength.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_mblength.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
   
========================================================================*/

#include <stdlib.h>

#include "tr_prototypes.h"

/*
 * Computes the logical length of string mbText
 *
 * Assumes that the input string is UTF-8 encoded and that the curent locale is UTF-8
 * Returns the logical length (in UNICODE MultiBytes chars)
 * Returns 0 if the string parameter is NULL or if the string is empty
 * returns -1 if the UTF-8 sequence was invalid
 */ 
int tr_mbLength (char *mbText)
{
	if ( mbText == NULL ) {
		return 0;
	}
	size_t res = mbstowcs( NULL, mbText, 0 );
        if (res == (size_t) -1) {
                /* invalid encoding */
                return -1;
        }
        return (int) res;
}
