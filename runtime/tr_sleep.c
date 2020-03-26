/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_sleep.c
	Programmer:	Juha Nurmela (JN)
	Date:		Tue Jan 28 11:13:57 EET 1997

	sleep() for NT (uses Sleep())
	and convenience bfSleep(nSec) for all.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_sleep.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  3.00 28.01.97/JN	Created.
========================================================================*/

#ifdef MACHINE_WNT

int sleep(int nsec)
{
	Sleep(nsec * 1000);
	return(nsec);
}

#endif

int bfSleep(double nsec)
{
	return (sleep((int) nsec));
}

