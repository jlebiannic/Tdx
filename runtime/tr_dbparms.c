/*============================================================================
	V E R S I O N   C O N T R O L
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_dbparms.c 55167 2019-09-24 13:48:37Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  4.00 25.03.98/HT	Created
  4.01 28.10.98/HT	Modified set_that_value
  4.02 25.03.99/KP	Do not use rls_* functions directly anymore.
  4.03 24.11.99/KP	Do not warn about mismatching separators when using
					the default ones.
  4.04 28.03.2017/SCH(CG) TX-2963  Allocate dynamicaly memory for line in tr_CopyArr() 
  TX-3123 - 19.07.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3143 24.09.2019 - Olivier REMAUD - Passage au 64 bits
============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"
#include "tr_logsystem.h"
#include "rlslib/rls.h"

enum {
	LOAD_FRESH	= 0,   /* Create a new table        */
	LOAD_OVERRIDE   = 1,   /* Overwrite existing values */
	LOAD_APPEND     = 2,   /* Append to existing values */
	LOAD_ADDNEW     = 3,   /* Add new entries           */
	LOAD_4          = 4,   /* Overwrite existing and 
				  remove empty values */
	};

void tr_am_dbparmset( LogSysHandle	*pLog, char *sExt, char *sName, char *sValue )
{
	tr_lsSetTextParm(pLog, sExt, sName, sValue);
	/*
	 * char	*buf;
	 *
	 * buf = tr_malloc( strlen(sName)+strlen(sExt)+2 );
	 * sprintf(buf,"%s.%s",sExt,sName);
	 * rls_settextvalue(pLog->logsys, buf, sValue ? sValue : "");
	 * tr_free(buf);
	 */
}

char *tr_am_dbparmget( LogSysHandle	*pLog, char *sExt, char *sName )
{
	return (tr_lsGetTextParm(pLog, sExt, sName));
}

void tr_PrintParms( LogSysHandle *pLog, char *sExt,  FILE *fOut )
{
	register char	**v;
	register int	i;

	if (pLog && ( fOut != (FILE *) NULL )) { 
        /* From is a db-param-array */
		if ((v = tr_lsFetchParms(pLog, sExt))) {
			for (i = 0; v[i]; ++i) {
				tr_mbFprintf( fOut, "%s\n", v[i] );
			}
			fflush( fOut );
			tr_lsFreeStringArray(v);
		}
	}
}


void tr_amdbparmcopy( LogSysHandle *pTo, char *sTo, LogSysHandle *pFrom, char *sFrom )
{
	tr_CopyArr( 0, pFrom, sFrom, pTo, sTo, NULL, NULL);
	return;
}

void tr_am_dbremove( LogSysHandle *pTo, char *sTo )
{
	tr_lsClearParms(pTo, sTo);
	return;
}

int tr_am_dbvalid( LogSysHandle *pTo, char *sTo )
{
	register char **v;
	register int	i = 0;

	if ( !pTo  || !sTo)
		return 0;

	if (( v = tr_lsFetchParms(pTo, sTo))) {
		for (i = 0; v[i]; ++i);
		tr_lsFreeStringArray(v);
	}

	return i;
}

