/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_logsysstub.c
	Programmer:	Kari Poutiainen (KP)
	Date:		Fri Mar 19 08:05:36 EET 1999

	Copyright Generix-Group 1990-2011


	Stub routines for logsystem handling. These call the
	actual functions in tr_logsystem.c or tr_logsysrem.c
	depending on the current operation mode.


	
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_logsysstub.c 55498 2020-05-07 07:40:18Z jlebiannic $")
/*LIBRARY(libruntime_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  4.00 19.03.99/KP	Created
  4.01 25.05.99/HT	tr_lsResetFile and tr_lsCloseFile added.
  4.02 18.08.99/JR	tr_lsCloseFile with two modes
  4.03 04.12.99/KP	Use tr_localTryGetUniq(), changed params for 
			tr_lsTryGetUniq()
========================================================================*/

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"


/*========================================================================
  Nobody is supposed to know the pathnames, and it would not
  even be usefull with remote bases !!!
========================================================================*/

char *tfBuild__LogSystemPath(LogSysHandle *log, char *ext)
{
	return (tr_lsPath(log, ext));
}


void tr_lsFreeStringArray(char **ta)
{
	char **vp;

	for(vp = ta; ta && *vp; ++vp)
		free(*vp);

	free (ta);
}

int tr_lsCloseFile( FILE *fp, char mode )
{
	int rv = 0;

	/* 4.02/JR two modes (0 == fclose is done, else not) */
	if(mode == 0){
		/* default (fclose is done) */
		rv = fclose(fp);
	}
	return( rv );
}

