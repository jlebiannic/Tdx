/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_save.c
	Programmer:	Mikko Valimaki
	Date:		Fri Oct  9 14:41:11 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
===========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_save.c 55062 2019-07-22 12:35:02Z sodifrance $")
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
#if 0
		/*
		 * 3.03/KP
		 * This stat-guess-procedure is no longer required, as the param-save
		 * mechanism is used if the path points to an rls-location anyway.
		 */
		if ( tr_lsTryStatFile( tr_parameterFile, NULL ) == -1 ) {
			RLSPATH       rlspath;
			char *formseps, multisep[2];
			int pcnt;

			formseps = tr_am_textget (taFORMSEPS, taPARAMETERS);

			if (formseps == TR_EMPTY || *formseps == 0)
				formseps = "=\0";

			multisep[0] = formseps[1];
			multisep[1] = 0x00;

			memset( &rlspath, 0x00, sizeof(RLSPATH) );
			if (tr_rlsDecodeLoc( tr_parameterFile, &rlspath )) {
				if ( tr_lsOpen( &paramArrayLog, rlspath.location )) {
					if ( !tr_lsReadEntry( &paramArrayLog, (double)rlspath.idx ) || 
								((pcnt = (int)tr_CopyArr( 0, NULL, taPARAMETERS, 
								&paramArrayLog, rlspath.ext, multisep, "=" )) == 0 )) {
						tr_parameterFile = NULL;
					/* tr_lsFreeEntryList(&paramArrayLog);
					** if ( paramArrayLog.logsys )
					** 	rls_close(paramArrayLog.logsys);
					** if ( paramArrayLog.path )
					** 	tr_free(paramArrayLog.path);
					*/
					}
#if 0
		else {
			struct am_node  *walker;
			char *idx, *val;
			fprintf( stderr, "Parameters to DB\n");
			walker = tr_am_rewind( taPARAMETERS );
			while ( tr_amtext_getpair( &walker, &idx, &val ) )
				fprintf(stderr,"%s=%s\n", idx,val );
			fprintf(stderr, "Parameters [%s] in DB\n", rlspath.ext);
			tr_PrintParms( &paramArrayLog,  rlspath.ext, stderr );
			fprintf( stderr, "------------------\n");
		}
#endif
				}
			}
		}
		else
		/*
		**   Old Fashion local file
		*/
		{
#endif
			/*
			 * 3.03/KP : Try to tr_rlsDecodeLoc() to see if the paramfile points
			 * into the database parameter file. If that is the case, use
			 * tr_CopyArr() instead of stdio.
			 */
			RLSPATH       rlspath;
			memset( &rlspath, 0x00, sizeof(RLSPATH) );
			if (tr_rlsDecodeLoc(tr_parameterFile, &rlspath) && 
					(rlspath.idx > 0) &&
					(rlspath.ext)) {
				char *formseps, multisep[2];

				formseps = tr_am_textget (taFORMSEPS, taPARAMETERS);

				if (formseps == TR_EMPTY || *formseps == 0)
					formseps = "=\0";

				multisep[0] = formseps[1];
				multisep[1] = 0x00;

				if ( tr_lsOpen( &paramArrayLog, rlspath.location )) {
					if ( tr_lsReadEntry( &paramArrayLog, (double)rlspath.idx )) {
						tr_CopyArr( 0, NULL, taPARAMETERS, &paramArrayLog, rlspath.ext, multisep, "=" );
						return 0;
					}
				}
			}
			/*
			 * In case we have not returned yet, tr_parameterfile probably points to
			 * a normal file. So, write params into it, then.
			 */
			tr_Save (taPARAMETERS,tr_GetFp(tr_parameterFile, "w"));
			tr_FileClose (tr_parameterFile);
		}
#if 0
	}
#endif
	return 0;
}
