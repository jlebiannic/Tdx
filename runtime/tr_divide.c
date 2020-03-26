/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_divide.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_divide.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 10:10:42 EET 1992
==============================================================================*/
#include "tr_strings.h"

/*==============================================================================
  Record all changes here and update the above string accordingly.
==============================================================================*/

#include "tr_prototypes.h"

/*============================================================================
============================================================================*/
double tr_Divide (double value, double divisor)
{
	if (divisor)
		return (value / divisor);
	tr_Fatal (TR_ERR_DIVISION_BY_ZERO);
    return 0.0;
}

