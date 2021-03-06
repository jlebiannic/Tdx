/*==============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_isempty.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
==============================================================================*/

/*==============================================================================
	V E R S I O N   C O N T R O L
==============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_isempty.c 55433 2020-03-16 12:37:08Z sodifrance $")
/*==============================================================================
  Record all changes here and update the above string accordingly.

  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
==============================================================================*/
#include <ctype.h>

#include "tr_externals.h"
#include "tr_privates.h"

/*==============================================================================
==============================================================================*/
int tr_isempty (char *text)
{
	if (!text || !*text || text == TR_EMPTY)
		return 1;
	while (*text) {
		if (!isspace(*text))
			return 0;
		text++;
	}
	return 1;
}

