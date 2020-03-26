/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strtoupr.c
	Programmer:	Mikko Valimaki
	Date:		Mon Nov 16 07:59:19 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_strtoupr.c 55060 2019-07-18 08:59:51Z sodifrance $")
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.01 06.02.95/JN	Removed excess #includes.
  TX-3123 - 17.07.2019 - Olivier REMAUD - UNICODE adaptation

============================================================================*/

#include "../bottom/tr_edifact.h"

/*============================================================================
============================================================================*/
char *tr_strtoupr (char *string)
{
	return tr_mbStrToUpper(string);
}

/*============================================================================
============================================================================*/
char *tr_strtolwr (char *string)
{
	return tr_mbStrToLower(string);
}
