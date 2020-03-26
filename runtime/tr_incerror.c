/*===========================================================================
	V E R S I O N   C O N T R O L

	Dummy function to be used in cases
	where the user does not bother to
	write this kind of function.
===========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_incerror.c 47429 2013-10-29 15:27:44Z cdemory $")
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 21.04.95/JN	Now this spits out a warning message.
===========================================================================*/

#include "tr_prototypes.h"

int bfIncomingError()
{
	tr_Log("*** Error in message.\n");
	return 1;
}

