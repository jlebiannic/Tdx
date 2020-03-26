/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_filewrite.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
#include <stdio.h>
/* #include <varargs.h> */
#include <stdarg.h>
#include "tr_prototypes.h"
#include "tr_privates.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_filewrite.c 55062 2019-07-22 12:35:02Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	First version
  3.01 30.03.99/JR	varargs->stdargs
  Bug 1219 08.04.11	free given parameters if they are in the memory pool
  TX-3123 - 10.07.2019 - Olivier REMAUD - UNICODE adaptation
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation

==============================================================================*/

void tr_FileWrite ( FILE *fp, char *args, ... )
{
	va_list	ap;

	va_start( ap, args );
	tr_mbVfprintf( fp, args, ap );
	va_end(ap);

	/* check in the list of arguments if they are in mempool 
	 * i.e. by use of call to build inside the build function */
	va_start( ap, args );
	tr_FreeMemPoolArgs(args, ap);
	va_end (ap);
}
