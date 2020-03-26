/*========================================================================

	File:		tfTempnam.c
	Author:		Juha Nurmela (JN)
	Date:		Thu Dec 11 12:10:02 EET 1997

	TCL callable tempnam() imitation.
==========================================================================
  11.12.97/JN	Created
  12.05.97/JN	temp_nam_ has the unavoidable problem that
		the function is _not_ supposed to create the file,
		just return a pathname that did not exist.
		basename now contains the pid (whatever on NT)
		and lsbits of time() in separate positions
		(it used to add them, which is less exclusive).
  31.07.98/VA   added NULL as a parameter for time()
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tfTempnam.c 47429 2013-10-29 15:27:44Z cdemory $")
#include <stdio.h>
extern char *TR_EMPTY;
extern char *tr_MemPoolPass();
#ifdef MACHINE_WNT
#include <windows.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#else
#include <unistd.h>
#endif

#include <string.h>
#include "tr_prototypes.h"

#ifdef MACHINE_WNT
static void convertPath2Windows(char * path) {
	char *p;

	while (p = strchr(path, '/')) 
		p[0] = DIRSEP;
}
#endif
/* tfTempnam : generate a temporary file name in a given directory
  psDir : path to the directory
  psPfx : filename prefix
*/
char *tfTempnam(char *psDir, char *psPfx)
{
	char fullname[1024];
#ifdef MACHINE_WNT
	char *psep = "\\";
#else
	char *psep = "/";
#endif
	char *path;
	char lastc;
	
	if (psDir == NULL || strlen(psDir) == 0 || psPfx == NULL || strlen(psPfx) == 0)
		tr_Fatal("(tfTempnam) parameters must not be NULL");
		
	strcpy(fullname,psDir);
	
	lastc=psDir[strlen(psDir)-1];
	if (strchr(psDir, '/'))
		psep = "/";
	/* Add a separator to the path if the prefix does not start with a separator */
	if (lastc != '\\' && lastc != '/' && lastc != ':' && psPfx[0] != '\\' && psPfx[0] != '/')
		strcat(fullname,psep);
	strcat(fullname,psPfx);
	strcat(fullname,"XXXXXX");
#ifdef MACHINE_WNT
	convertPath2Windows(fullname);  /* Change / to \ */
#endif

	path=strdup(fullname);
	if (!path)
		return TR_EMPTY;

	tr_MemPoolPass(path);
	bfMktemp(path);
	return path;
}

int bfMktemp(char *data_file)
{
#ifndef MACHINE_WNT
	int fd;
#endif
	int len_datafile;
	
	if (data_file==NULL)
		tr_Fatal("(bfMktemp) parameter must not be NULL");
		
	len_datafile=strlen(data_file);
	if (len_datafile<6)
		tr_Fatal("(bfMktemp) parameter must finish by XXXXXX");
		
	if (strcmp(data_file+len_datafile-6,"XXXXXX")!=0)
		tr_Fatal("(bfMktemp) parameter must finish by XXXXXX");
		
#ifdef MACHINE_WNT
	if (_mktemp(data_file))
		return 1;
	return 0;
#else
	fd = mkstemp(data_file);
	if (fd == -1)
		return 0;
	close(fd);  /* close the file as we do not return its descriptor */
	return 1;
#endif
}


