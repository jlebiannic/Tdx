/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_striprc.c
	Programmer:	Olivier Remaud (Sodifrance)
	Date:		12.06.2019	

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_striprc.c $")

/*========================================================================
  Record all changes here and update the above string accordingly.
  TX-3123 - 12.06.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
   
========================================================================*/

#include "tr_prototypes.h"

/*
 * strip '\n' & '\r' from end of line
 * Returns the removed char count
 */

int  tr_stripRC( char *text )
{
	size_t rc_count = 0;
	char *ptr = text;
	while ( *ptr != '\0' ) {
		++ptr;
	}
	ptr--;
	while ( ptr >= text && (*ptr == '\r' || *ptr == '\n') ) {
		*ptr = '\0';
		--ptr;
		++rc_count;
	}
	return rc_count;
}

