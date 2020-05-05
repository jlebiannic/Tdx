/*============================================================================
	V E R S I O N   C O N T R O L
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_dbparms.c 55062 2019-07-22 12:35:02Z sodifrance $")
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
  Jira TX-3143 16.03.2020 - Olivier REMAUD - Passage au 64 bits
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

int	tr_dbparms_trace = 0;

enum {
	LOAD_FRESH	= 0,   /* Create a new table        */
	LOAD_OVERRIDE   = 1,   /* Overwrite existing values */
	LOAD_APPEND     = 2,   /* Append to existing values */
	LOAD_ADDNEW     = 3,   /* Add new entries           */
	LOAD_4          = 4,   /* Overwrite existing and 
				  remove empty values */
	};

static double set_that_value(int mode, LogSysHandle *pTo, char *sTo, char *name, char *value, char multisepc, char sepc, char *prevValue )
{
	int	bDoSet = 1;
	double count = 0.0;
	char *cp;
	int _localmemptr;

	if ( !name || !*name || !value ) return count;

	_localmemptr = tr_GetFunctionMemPtr();

	/* strip trailing cr/lf characters */
	cp =  value + strlen(value);
	while (--cp >= value && (*cp == '\r' || *cp == '\n'))
		*cp = 0;

	if ( value && *value ) {
		switch(mode) {
			case LOAD_ADDNEW:
				if (prevValue && *prevValue) 
					return 0.0;
				break;
						
			case LOAD_4: 	
			case LOAD_OVERRIDE:
				if (prevValue)
					*prevValue = 0x00;
				break; 
			case LOAD_APPEND:   
				break;
			default:		break;
		}

		if ( bDoSet) {
			if ( multisepc && *value && prevValue && *prevValue ) {
				value = tr_BuildText( "%s%c%s", prevValue, multisepc, value );
			}
			/* TX-2199 : value is in the memory pool. Copy value here as tr_am_textset will free it */
			strcpy( prevValue, value );
			if (pTo)
				 tr_am_dbparmset( pTo, sTo, name, value );
			else {
				 tr_am_textset( sTo, name, value );
			}
			count = 1.0;
		}
	}
	else {
		if ( mode == LOAD_4 ) {
			if (prevValue && *prevValue) {
				if (pTo)
					tr_am_dbparmset( pTo, sTo, name, "" );
				else
					tr_am_textset( sTo, name, TR_EMPTY );
				*prevValue = 0x00;
			}
		}
	}

	tr_CollectFunctionGarbage(_localmemptr);  /* Remove unwanted mempool allocation */
	return count;
}

double tr_CopyArr( int mode, LogSysHandle *pFrom, char *sFrom, LogSysHandle *pTo, char *sTo, char *multiseps, char *seps )
{
	double 			count = 0.0;
	register char	**v;
	register int	i;
	char			sepc = (seps ? *seps : '=') ,
					multisepc = ( multiseps ? *multiseps : 0x00 ),
					*oldSeps,
					*prevName,
					prevValue[4096],
					*name,
					*value;
	struct am_node	*p;

	/*  To is not a db-param-array  */
	if (mode == LOAD_FRESH) { 
		if (pTo)
			tr_lsClearParms(pTo, sTo);
		else 
			tr_am_textremove( sTo );
	}

	if (!pTo) {
		/*
		**  For other options check the correspondence
		**  between the old and the new separators.
		*/
		oldSeps = tr_am_textget (taFORMSEPS, sTo);

		if (oldSeps != NULL && oldSeps != TR_EMPTY && (oldSeps[0] != sepc || oldSeps[1] != multisepc)) {
			/*
			 * 4.03/KP : If no seps (or default ones) are given for load() command,
			 * do not issue a warning but use old seps silently.
			 */
			if ((sepc != '=') || (multisepc != ':'))
				tr_Log (TR_WARN_LOAD_SEP_MISMATCH);

			sepc      = oldSeps[0];
			multisepc = oldSeps[1];
		}
	}

	prevName = NULL;
	prevValue[0] = 0x00;
	if (pFrom) {  /* From is a db-param-array */

		if ((v = tr_lsFetchParms(pFrom, sFrom))) {
			/*
			 * KP
			 * Should be just "if (pTo && (mode == LOAD_FRESH)) {" but some problems...
			 * if (pTo && (mode == LOAD_FRESH) && (tr_localMode() || getenv("RLS_SAVE_PARMS"))) {
			 */
			if (pTo && (mode == LOAD_FRESH)) {
				/*
				 * 4.02/KP (25.03.99)
				 * A special case; if destination is a db-param array,
				 * and mode is FRESH (0),
				 * perform a simply 1-to-1 copy instead of setting parameters one by one.
				 */
				tr_lsSaveParms(pTo, sTo, v);
			}
			else {
				for (i = 0; v[i]; ++i) {
					name = strdup( v[i] );
					value = strchr(name, sepc);
					if (value) {
						*value = 0;
						value++;
					}
					if ( !prevName || strcmp( prevName, name ) ) {
						if ((mode == LOAD_APPEND) || 
							(mode == LOAD_ADDNEW) ||
							(mode == LOAD_4)) {
							if ( pTo )
								strcpy( prevValue, tr_am_dbparmget( pTo, sTo, name ));
							else
								strcpy( prevValue, tr_am_textget( sTo, name ) );
						}
						else
							prevValue[0] = 0x00;
						if ( prevName ) 
							free( prevName );
						prevName = strdup( name );	
					}
					if ( tr_dbparms_trace ) {
						fprintf( stderr, "DB [%s=%s] -> Array ",name,value);
					}
					count = count + set_that_value( mode, pTo, sTo, name, value, 
							multisepc, sepc, prevValue );
					if ( tr_dbparms_trace ) {
						fprintf( stderr, " Count=%d \n",(int)count);
					}
					free( name ); name = NULL;
				}
				if ( tr_dbparms_trace ) {
					fprintf( stderr, "Found [%d] from DB \n", i);
				}
			}

			/*
			 * rls_free_stringarray(v);
			 */
			tr_lsFreeStringArray(v);
		}
		else {
			if ( tr_dbparms_trace ) {
				fprintf( stderr, "Nothing found from DB \n");
			}
		}
	}
	else {  /* From is a text-array */
		p = tr_am_rewind( sFrom );
		/*
		 * KP
		 * Should be just "if (pTo && (mode == LOAD_FRESH)) {" but some problems...
		 * if (pTo && (mode == LOAD_FRESH) && (tr_localMode() || getenv("RLS_SAVE_PARMS"))) {
		 */
		if (pTo && (mode == LOAD_FRESH)) {
			/*
			 * 4.02/KP (25.03.1999)
			 * A special case if the pTo is a db-parm array and
			 * the mode is FRESH (0). See above for details.
			 */
			 /* TX-2963 28/03/2017 SCH(CG) Allocate dynamicaly memory for line */
			char * line;
			char **v;
			int i = 0, size = 256, inc = 256;

			v = calloc(size, sizeof(char *));
			while (tr_amtext_getpair(&p, &name, &value)) {
				if (i >= size) {
					v = realloc(v, ((size+inc)*sizeof(char *)));
					if (!v)
						break;
					size += inc;
				}
				line = malloc((strlen(name) + strlen(value) + 2)* sizeof(char));
				sprintf(line, "%s%c%s", name, sepc, value);
				v[i++] = strdup(line);
				free(line);
			}
			/* End TX-2963 */
			v[i] = NULL;
			tr_lsSaveParms(pTo, sTo, v);
			tr_lsFreeStringArray(v);
		}
		else {
			prevName = NULL;
			while ( tr_amtext_getpair( &p, &name, &value ) ) {
				if ( !prevName || strcmp( prevName, name ) ) {
					if ((mode == LOAD_APPEND) || 
						(mode == LOAD_ADDNEW) ||
						(mode == LOAD_4)) {
						if ( pTo )
							strcpy( prevValue, tr_am_dbparmget( pTo, sTo, name ));
						else
							strcpy( prevValue, tr_am_textget( sTo, name ) );
					}
					else
						prevValue[0] = 0x00;
					if ( prevName ) free( prevName );
						prevName = strdup( name );	
				}
				count = count + set_that_value( mode, pTo, sTo, name, value, 
						multisepc, sepc, prevValue );
			}
		}
	}
	if ( prevName ) { 
		free( prevName );
		prevName = NULL;
	}

	if (!pTo) {
		char buffer[3];
		buffer[0] = sepc;
		buffer[1] = multisepc;
		buffer[2] = 0x00;
		tr_am_textset( taFORMSEPS, sTo, buffer );
	}

	if ( tr_dbparms_trace ) {
		fprintf( stderr, "tr_CopyArr() : returning %lf\n", count);
	}
	return count;

}

