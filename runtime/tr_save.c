/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_save.c
	Programmer:	Mikko Valimaki
	Date:		Fri Oct  9 14:41:11 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
===========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_save.c 55493 2020-05-06 13:38:41Z jlebiannic $")
/*LIBRARY(libruntime_version)
*/
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 06.02.95/JN	Removed excess #includes
  3.02 14.02.96/JN	strtok replaced with strsep
  3.03 28.11.98/KP	If paramfile to be saved is an rls-database
			paramfile (it has idx and ext), use tr_CopyArr
			instead of normal stdio.
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation

===========================================================================*/

#include <stdio.h>
#include <string.h>

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"

/*==========================================================================
==========================================================================*/

int tr_Save (char *arrayname, FILE *fp)
{
	struct am_node *walkpos;
	char *formseps;
	char *name;
	char namesep;
	char *value;
	char multisep;

	/*
	**  Fetch the separators that were used in loading this array.
	**  If there is none, then this array has never been
	**  loaded by our routine and we stop worrying about it.
	*/
	formseps = tr_am_textget (taFORMSEPS, arrayname);

	if (formseps == TR_EMPTY || *formseps == 0) {
		formseps = "=\0";
	} else {
	    if ( &formseps[0] && mblen(&formseps[0], MB_CUR_MAX) > 1 ) {
	        /* multibytes seps not allowed */
	        /* TODO : warning ? Error ? Use default sep ? */
	    }
	    if ( &formseps[1] && mblen(&formseps[1], MB_CUR_MAX) > 1 ) {
	        /* multibytes seps not allowed */
 	       /* TODO : warning ? Error ? Use default sep ? */
	   }
    }

	namesep  = formseps[0];
	multisep = formseps[1];

	walkpos = tr_am_rewind(arrayname);

	while (tr_amtext_getpair(&walkpos, &name, &value)) {
		/*
		 * Zero-length values are skipped.
		 */
		if (value == NULL || *value == 0)
			continue;

		if (multisep && strchr (value, multisep)) {
			char *seppos;
			char *partial;
			char *workCopy = tr_strdup (value);
			/*
			**  Split the line by the separator character
			*/
			seppos = workCopy;
			while (partial = tr_strcsep(&seppos, multisep))
				tr_mbFprintf (fp, "%s%c%s%s",
					name, namesep, partial, TR_NEWLINE);
			tr_free (workCopy);
		} else {
			tr_mbFprintf (fp, "%s%c%s%s",
				name, namesep, value, TR_NEWLINE);
		}
	}
	return 0;
}

/*============================================================================
============================================================================*/

int tr_SaveParameters ()
{

	if (tr_updateParams && tr_parameterFile) {
			/*
			 * In case we have not returned yet, tr_parameterfile probably points to
			 * a normal file. So, write params into it, then.
			 */
			tr_Save (taPARAMETERS,tr_GetFp(tr_parameterFile, "w"));
			tr_FileClose (tr_parameterFile);
		}

	return 0;
}
