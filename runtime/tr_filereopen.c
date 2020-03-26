#ifdef MACHINE_WNT_NOT_IN_USE
#include "tr_filereopen.c.w.nt"
#else
/*============================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_filereopen.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992 Telecom Finland/EDI Operations
============================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_filereopen.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.01 30.01.97/JN	Making sure we get what we want (0, 1 or 2).
  3.02 13.02.97/JN	close&open just does not work on NT.
  3.03 18.08.99/JR	Linux returns dynamic addres from fopen(stdout..)
                     Fixed with dup2. (Allmost complete rewrite of FileReopen)
  3.04 21.10.99/JR	Flushing problem fixed. This problem came out while
                     USE_LOCAL_LOGSYS=1 was used.
  3.05 07.12.99/HT,KP	Added Null-filename check.
============================================================================*/

/* To do:
 * popen() function for NT, so that we can wait()
 * processes.
 */

#include <stdio.h>
#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include <fcntl.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#ifdef MACHINE_WNT
#define NULLFILE "NUL"
#else
#include <unistd.h>
#define NULLFILE "/dev/null"
#include <sys/wait.h>
#endif

#define isprocess(x) ((x)&&*(x)=='|')

#ifndef MACHINE_LINUX
extern int errno;
#endif

/*============================================================================
============================================================================*/
#define TYPE_FILE	1
#define TYPE_PROCESS	2

int tr_FileReopen (char *filename, FILE *fp)
{
	static int fileType[3] = { TYPE_FILE, TYPE_FILE, TYPE_FILE };
	int fileTypeNew;
	int error, rv;
	FILE *fp2;
	int file = fileno (fp);
	char *what =
		fp == stdin  ? "INPUT" :
		fp == stdout ? "OUTPUT" :
		fp == stderr ? "LOGGING" : "UNKNOWN";

	/* Accept only stdin, stdout or stderr */
	if (file < 0 || file > 2)
		tr_Fatal (TR_FATAL_ILLEGAL_SYS_FILE, file);


	/* 3.05/HT,KP */
	if (!filename
	 || !filename[0]
#ifdef MACHINE_WNT
	 || !strcmp(filename, "/dev/null")
#endif
	 )
		filename = NULLFILE;

	/* 3.04/JR Flush output, so that we don't miss anything */
	fflush(fp);

	/* First close the file descriptor. */
	if (fileType[file] == TYPE_PROCESS){
		/* Closing descriptor closes also popen():ed process */
		close(file);
		/* Later we need valid descriptor */
		open(NULLFILE, O_WRONLY);
		/* Eat zombies... */
#ifndef MACHINE_WNT
		alarm(10);
		wait(&rv);
		alarm(0);
#endif
	}
	else {
		/* Fake close (we need that darn FILE structure) */
		tr_fclose (fp, 1);
	}

	/* Open a new descriptor. (file/process) */
	if (isprocess(filename)) {
		/* This is a process. */
		if (file == 0)
			fp2 = tr_popen (filename + 1, "r");
		else
			fp2 = tr_popen (filename + 1, "w");
		fileTypeNew = TYPE_PROCESS;
	} else {
		/* This is a regular file. */
		if (file == 0)
			fp2 = tr_fopen (filename, "r");
		else
			fp2 = tr_fopen (filename, "a");
		fileTypeNew = TYPE_FILE;
	}

	error = errno;
	if (!fp2) {
		if (file == 0)
			fp2 = tr_fopen (NULLFILE, "r");
		else
			fp2 = tr_fopen (NULLFILE, "w");
		fileType[file] = TYPE_FILE;

		tr_Log (TR_WARN_REOPEN_FAILED, fileno(fp2), filename, error);

		return 1;
	}

	/* Duplicate our fp2 to stdin, stdout or stderr
	 * then close unneeded pointer/descriptor.
	 */
#ifdef MACHINE_WNT
	if( (rv = dup2(fileno(fp2), file)) != 0 ){
#else
	if( (rv = dup2(fileno(fp2), file)) != file ){
#endif
		tr_Log("Redirect %s got %d, expected %d\n", what, rv, file);
	}
	if (fileTypeNew == TYPE_PROCESS)
		/* We can't close anything else than descriptor! */
		close( fileno(fp2) );
	else
		fclose ( fp2 );

	/* Bookkeeping... */
	fileType[file] = fileTypeNew;

	/* If this is output and we want no buffering
	 * then set the buffer to NULL.
	 */
	if (tr_unBufferedOutput && file > 0)
		setbuf (fp, NULL);
	
	return 0;
}

#endif 
