/*========================================================================
  Dummy function to be used in cases where the user does not bother to
  write this kind of function.

  This is called when a dependency violation is detected.
  If this function returns true, an error message will be printed
  and other (yet unspecified) action might be taken.

  By returning false (zero, that is) this function will inhibit
  any other action from runtime library.

  name:       the name of the offending segment.
  dependency: the rule from message database.

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_deperror.c 47429 2013-10-29 15:27:44Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 05.12.94/JN	Created (copied tr_incerror.c).
========================================================================*/

/* ARGSUSED */
int bfDependencyError(char *name, char *dependency)
{
	return (1);
}

