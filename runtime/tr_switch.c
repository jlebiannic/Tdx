/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_switch.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:40:04 EET 1992
============================================================================*/
/* #include <varargs.h> */
#include "conf/local_config.h"
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_switch.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	First version
  3.01 30.03.99/JR	varargs->stdargs
  Bug 1219 08.04.11/JFC ensure parameters given to tr_strcmp are not in the pool
					if they need to be used after the call.
==============================================================================*/

/*============================================================================
	The translator maker generates code that assumes that the
	default return value is -1.
	see mt_grammar.y
============================================================================*/
int tr_Switch ( char *args, ... )
{
	va_list ap;
	char    *base;
	char    *choice;
	int	choiceNumber = 0;

	va_start(ap, args);
	base = tr_strdup(args);
	/*
	**  Since we call tr_strcmp() there is no need
	**  to check for NULL-pointer.
	*/
	while ((choice = va_arg(ap, char *))) {
		if (!tr_strcmp (base, choice)) {
			va_end (ap);
			tr_free(base);
			return choiceNumber;
		}
		choiceNumber++;
	}
	va_end (ap);
	tr_free(base);
	return -1;
}

