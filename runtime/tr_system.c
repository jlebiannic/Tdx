/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_system.c
	Programmer:	Damien BLANCHET (CG)
	Date:		Wen Jun 04 13:37:47 EET 2014

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_system.c 48426 2014-07-03 15:33:58Z tcellier $")
/*========================================================================
  5.20 04.06.2014/DBL(CG)	TX-2537 Created.
========================================================================*/

double tr_System (char *command) {

	int result;

	result = system(command);
	
	return (double)((result > 255) ? (int)(result / 256) : result);
}