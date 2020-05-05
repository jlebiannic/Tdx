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
MODULE("@(#)TradeXpress $Id: tr_logsysstub.c 47429 2013-10-29 15:27:44Z cdemory $")
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


#if 0
#define TR_WARN_FILE_WRITE_FAILED "Failed writing to file \"%s\" (%d)."
#define TR_WARN_FILE_READ_FAILED  "Failed reading from file \"%s\" (%d)."
#define TR_WARN_FILE_COPY_FAILED  "Failed copying \"%s\" to \"%s\" (%d)."

/*
 *  Only for filter-comparison
 */
#define OPERATOR_STRINGS

#include "../logsystem/remote/rls.h"

extern double atof();
#ifndef MACHINE_LINUX
extern int errno;
#endif

/*========================================================================
========================================================================*/

static LogSysHandle *ActiveLogSysHandles = (LogSysHandle *)NULL;
#endif

/*========================================================================
  Nobody is supposed to know the pathnames, and it would not
  even be usefull with remote bases !!!
========================================================================*/

char *tfBuild__LogSystemPath(LogSysHandle *log, char *ext)
{
	return (tr_lsPath(log, ext));
}

/*
 *  Return 0 if fs-file, 1 if remote & exists and -1 if not.
 *  If result is NULL, this is only 'access()' call,
 *  and no error is logged.
 */
int tr_lsTryStatFile(char *pseudoname, struct stat *st_result)
{
	return 0;
}

int tr_lsTryOpenFile(char *pseudoname, char *mode, FILE **fp_result)
{
	return 0;
}

int tr_lsTryRemoveFile(char *uglypath, int *ok_result)
{
	return 0;
}

int tr_lsTryCopyFile(char *source, char *destination, int *ok_result)
{
	return 0;
}

void tr_lsFreeStringArray(char **ta)
{
	char **vp;

	for(vp = ta; ta && *vp; ++vp)
		free(*vp);

	free (ta);
}


void tr_lsResetFile( FILE *fp )
{
	return;
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

