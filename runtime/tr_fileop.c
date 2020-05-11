/*===========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_fileop.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992
			Sat Jan  9 09:37:45 EET 1993

	Copyright (c) 1992 Telecom Finland/EDI Operations
===========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_fileop.c 55497 2020-05-06 15:49:05Z jlebiannic $")
/*LIBRARY(libruntime_version)
*/
/*===========================================================================
  Record all changes here and update the above string accordingly.
  3.01 12.03.93/MV	Added O_TRUNC to the opening of the copy destination.
  3.02 28.12.94/JN	Upped bufsize and checked for more errors in copy.
  3.03 08.05.95/JN	directory fixup, notexist/file can now be created
			without explicitly mkdir-ing notexist/ first.
  3.04 08.02.96/JN	O_BINARY for Micro$oft.
  3.05 27.11.96/JN	Warning silenced on NT.
  3.06 05.12.96/JN	Some more NT changes. No link().
  3.07 09.06.98/JN	Rest of fileops for rls files.
  3.08 16.06.98/JN	... and link() too.
  3.09 11.05.99/HT	NT patch for remote files
  3.10 25.05.99/HT	NT patch replaced with new tr_lsCloseFile function
  3.11 04.06.99/HT	tr_lsCloseFile is tested.
  3.12 18.08.99/JR	tr_lsCloseFile needs more testing :)
                     tr_lsCloseFile and tr_fclose have two modes,
							default(0) mode and redirect(1) mode.
  Jira TX-3123 24.09.2019 - Olivier REMAUD - UNICODE adaptation
===========================================================================*/

#define TR_WARN_FILE_WRITE_FAILED "Failed writing to file \"%s\" (%d)."
#define TR_WARN_FILE_READ_FAILED  "Failed reading from file \"%s\" (%d)."

#ifdef MACHINE_HPUX
#define _LARGEFILE64_SOURCE
#else
#define __USE_LARGEFILE64
#endif
#ifdef MACHINE_WNT
#define fopen64 fopen
#define  open64  open
#endif

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#ifdef MACHINE_AIX
#include <errno.h>
#endif
#ifdef MACHINE_LINUX
#include <errno.h>
#endif
#ifdef MACHINE_SOLARIS
#include <sys/errno.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif
#ifdef MACHINE_WNT
#include <io.h>
#include <errno.h>
#else
#include <unistd.h>
#endif
#ifndef MACHINE_LINUX
extern int errno;
#endif
#include "tr_privates.h"
#include "tr_strings.h"
#include "tr_prototypes.h"



FILE *tr_fopen(char *filename, char *mode)
{
	FILE *fp;

	fp = (FILE *)fopen64(filename, mode);

	/*
	 * If we fail creation because of
	 * non-existing parent directory,
	 * try again after fixing the problem.
	 */
	if (fp == NULL
	 && (*mode == 'w' || *mode == 'a')
	 && tr_DirFixup(filename))
		fp = (FILE *)fopen64(filename, mode);

	return (fp);
}

int tr_fclose(FILE *fp, char mode)
{
	/* 3.12/JR two modes...
	 * mode == 0 fclose is done
	 * else fclose is not done
	 */
	return ( tr_lsCloseFile( fp, mode ) );
}

FILE *tr_popen(char *filename, char *mode)
{
#ifdef MACHINE_WNT
	return (_popen(filename, mode));
#else
	return (popen(filename, mode));
#endif
}

int tr_pclose(FILE *fp)
{
#ifdef MACHINE_WNT
	return (_pclose(fp));
#else
	return (pclose(fp));
#endif
}

extern char taFORMSEPS[];

/*============================================================================
============================================================================*/
int tr_FileRemove (char *path)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	int ok;

	if (unlink (path)) {
		tr_Log (TR_WARN_UNLINK_FAILED, path, errno);
		return 0;
	}
	return 1;
}

/*============================================================================
============================================================================*/
int tr_PrintFile (char *filename, FILE *fpOut)
{
	FILE   *fpIn;
	static char buffer[BUFSIZ];
#ifndef MACHINE_LINUX
	extern int errno;
#endif

	if ((fpIn = tr_fopen (filename, "r")) == NULL) {
		tr_Log (TR_WARN_FILE_R_OPEN_FAILED, filename, errno);
		return 1;
	}
	while (tr_mbFgets (buffer, BUFSIZ, fpIn))
		tr_mbFputs (buffer, fpOut);
	tr_fclose (fpIn, 0);

	return (ferror(fpOut));
}

/*============================================================================
============================================================================*/
int tr_CopyFile (char *source, char *destination)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	int ok, nBytes, nWritten, fdS, fdT;
	char *szPtr, szBuf[8192];


	if ((fdS = open64(source, O_RDONLY | O_BINARY)) == -1) {
		tr_Log (TR_WARN_FILE_R_OPEN_FAILED, source, errno);
		return 0;
	}
	fdT = open64(destination,
		O_TRUNC | O_CREAT | O_WRONLY | O_BINARY, 0666);

	if (fdT == -1 && tr_DirFixup(destination))
		fdT = open64(destination,
			O_TRUNC | O_CREAT | O_WRONLY | O_BINARY, 0666);

	if (fdT == -1) {
		tr_Log (TR_WARN_FILE_W_OPEN_FAILED, destination, errno);
		close(fdS);
		return 0;
	}
	while ((nBytes = read(fdS, szBuf, sizeof(szBuf))) > 0) {
		szPtr = szBuf;
		do {
			if ((nWritten = write(fdT, szPtr, nBytes)) <= 0) {
				tr_Log (TR_WARN_FILE_WRITE_FAILED,
					destination, errno);
				if (errno==ENOSPC)
					tr_Fatal("ERROR: There is NO SPACE LEFT on device");
				close(fdS);
				close(fdT);
				return 0;
			}
			szPtr += nWritten;
			nBytes -= nWritten;
		} while (nBytes > 0);
	}
	if (nBytes < 0)
		tr_Log (TR_WARN_FILE_READ_FAILED, source, errno);
	close(fdS);
	close(fdT);
	if (nBytes < 0)
		return 0;
	return 1;
}

/*============================================================================
============================================================================*/
int tr_MoveFile (char *source, char *destination)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	int twice = 1;
	int ok, rv;

#ifdef MACHINE_WNT

	while (rename(source, destination) != 0) {
		if (twice-- && tr_DirFixup(destination))
			continue;
		tr_Log(TR_WARN_FILE_LINK_FAILED, source, destination, errno);
		return (0);
	}
#else /* not WNT */

	rv = link (source, destination);

	if (rv == -1 && tr_DirFixup(destination))
		rv = link (source, destination);

	/* Destination is not on the same file system, we need to copy the file */
	if (rv == -1 && errno == EXDEV) {
		rv = access(destination, R_OK | W_OK);
		if (rv != -1 || errno != ENOENT) {
			tr_Log(TR_WARN_FILE_LINK_FAILED, source, destination, errno);
			return (0);
		}
		rv = tr_CopyFile (source, destination);
		if (rv == 0) {
			tr_Log(TR_WARN_FILE_LINK_FAILED, source, destination, errno);
			return (0);
		}
	}

	if (rv == -1) {
		tr_Log (TR_WARN_FILE_LINK_FAILED, source, destination, errno);
		return 0;
	}
	if (unlink (source)) {
		tr_Log (TR_WARN_UNLINK_FAILED, source, errno);
		return 0; /* oopsie, we "half-succeeded", but 0/1 return */
	}
#endif
	return 1;
}

#ifndef MACHINE_WNT
/*============================================================================
============================================================================*/
int tr_LinkFile (char *source, char *destination)
{
#ifndef MACHINE_LINUX
	extern int errno;
#endif
	int rv, ok;

	rv = link (source, destination);

	if (rv == -1 && tr_DirFixup(destination))
		rv = link (source, destination);

	if (rv == -1) {
		tr_Log (TR_WARN_FILE_LINK_FAILED, source, destination, errno);
		return 0;
	}
	return 1;
}

#endif /* WNT */
