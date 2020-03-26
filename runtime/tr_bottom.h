#ifndef __TR_BOTTOM_H__
#define __TR_BOTTOM_H__
/*========================================================================
        T R A D E X P R E S S

        File:		bottom/tr_bottom.h
        Author:		Kari Poutiainen (KP)
        Date:		Fri Jan 13 13:13:13 EET 1994

		Copyright Generix-Group 1990-2011

		This includes prototypes for functions in libedi that are used
		outside this library.

		An interface definition, that is.

		All memory-handling related functions (from mem.c) are 
		intentionally left out of this.

		This header is not (yet) included anywhere but serves a purpose
		within the XML-project, the libXML should provide similar
		interface.

========================================================================*/
#include "conf/config.h"
HEADERFILE(tr_bottom,"@(#)TradeXpress $Id: tr_bottom.h 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  4.00 23.09.99/KP	Created.
========================================================================*/
#include "../bottom/tr_edifact.h"
#endif
