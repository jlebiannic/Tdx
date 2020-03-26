/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_textopt.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:40:04 EET 1992
============================================================================*/
/* #include <varargs.h> */
#include "conf/local_config.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_textopt.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	First version
  3.01 30.03.99/JR	varargs->stdargs
==============================================================================*/
char *tr_TextOpt ( char *args, ... )
{
	va_list ap;
	char    *base;
	char    *choice;
	char    *result;

	va_start(ap, args);
	base = args;
	/*
	**  The list consists of choice-result pairs and is terminated
	**  with a 0-0 pair.
	*/
	while ((choice = va_arg(ap, char *)) && (result = va_arg(ap, char *))) {
		/*
		**  TR_EMPTY is used to mark the default-option
		**  see mt_grammar.y
		*/
		if (choice == (char *)-1 || !tr_strcmp (base, choice)) {
			va_end (ap);
			return result;
		}
	}
	va_end (ap);
	return TR_EMPTY;
}

