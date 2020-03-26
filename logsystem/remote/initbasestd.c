/*========================================================================

	E D I S E R V E R

	File:		logsystem/initbasestd.c
	Author:		Juha Nurmela (JN)
	Date:		Fri Jan 13 13:13:13 EET 1994

	Copyright (c) 1998 Telecom Finland/EDI Operations

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: initbasestd.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  4.00 27.03.98/JN	Created

  This file is for "static" peruser.
  Target system can link peruser binary from selected xxxops.o and
  liblogd.a with relink_peruser program. Generated _space.c replaces
  this file then.

  It might be a good idea to doublecheck this with Imakefile,
  to keep initializations in the same order. It _should_ not
  matter in which order each basetype is tried when OPEN_BASE request
  is processed, but anyway...
========================================================================*/

#include "bases.h"

void
init_bases()
{
	init_base(init_legacyops); /* Old EDISERVER bases. */
}

