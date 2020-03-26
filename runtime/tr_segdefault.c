/*========================================================================
  Dummy function to be used in cases where the user does not bother to
  write this kind of function.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_segdefault.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 24.02.95/JN	Created (copied tr_incerror.c).
========================================================================*/

#include "tr_prototypes.h"

/* ARGSUSED */
int tr_SegRoutineDefault(char *segname, char **segdata)
{
	return 0;
}


