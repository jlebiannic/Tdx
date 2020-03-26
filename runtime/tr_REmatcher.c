/*========================================================================
	E D I S E R V E R

	File:		tr_REmatcher.c
	Programmer:	Juha Nurmela (JN)
	Date:		Fri Jul 29 13:33:52 EET 1994

	Input line matching with RE's

========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_REmatcher.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 21.04.95/JN	created.
========================================================================*/
#include <stdio.h>
#include <string.h>
#include "tr_externals.h"
#include "tr_privates.h"

/* Returns non-zero (true) if the match buffer matches `re'. */
int tr_MatchBufRE(register void *re /* what we are looking for */)
{
	if (tr_regular_execute(re, tr_matchBuf))
		return 1;

	/* No match. */
	return 0;
}
