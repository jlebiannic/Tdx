/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_strcmp.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:35:28 EET 1992
============================================================================*/
#include <string.h>
#include "tr_prototypes.h"

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_strcmp.c 47429 2013-10-29 15:27:44Z cdemory $")
/*==============================================================================
  Record all changes here and update the above string accordingly.
==============================================================================*/

#include "tr_externals.h"
#include "tr_privates.h"

/*============================================================================
============================================================================*/
int tr_strcmp (char *s, char *t)
{
	int result = -1;

	if (!s) {
		if (!t || t == TR_EMPTY)
			result = 0;
		else
			result = 1;
	}
	else {
		if (!t) {
			if (s == TR_EMPTY)
				result = 0;
			else
				result = 1;
		}
	}
	if (result == -1) {
		if (s == TR_EMPTY) {
			if (tr_isempty (t))
				result = 0;
		}
	}
	if (result == -1) {
		if (t == TR_EMPTY) {
			if (tr_isempty (s))
				result = 0;
		}
	}
	if (result == -1) {
		result = strcmp (s, t);
	}
	/* When parameters are allocated by tr_mempool, we need to free them */
	if (s) 
		tr_MemPoolFree(s);
	if (t)
		tr_MemPoolFree(t);
	return result;
}

