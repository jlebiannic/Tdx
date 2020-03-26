/*========================================================================

	File:		nfcp.c
	Author:		Mikko Wecksten (MW)
	Date:		Mon Sep 21 09:43:52 EET 1992

	Notes:		
 28.12.94/JN	Added O_TRUNC, upped buffer size, check for e2big, mode.
 08.02.96/JN	O_BINARY for Micro$oft.
========================================================================*/
#include <stdio.h>
#include <fcntl.h>
#ifdef MACHINE_LINUX
#include <errno.h>
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 *  The O_BINARY should be okay even for text files,
 *  they were text before and copy is text too then.
 */
/*----------------------------------------------------------------------
	double nfCp
	Description:	Copies the file to another name.
	Returns:	0 if OK.
	Notice:		
----------------------------------------------------------------------*/
double nfCp(char *psTarget, char *psSource)
{
	int	iSaveErrno;
	int	iBytes, iWritten;
#ifndef MACHINE_LINUX
	extern	int errno;
#endif
	int	fdS, fdT;
	char	*szPtr, szBuf[8192];

	if ((fdS = open(psSource, O_RDONLY | O_BINARY)) == -1)
		return 1;

	if ((fdT = open(psTarget,
			O_CREAT | O_WRONLY | O_TRUNC | O_BINARY,
			0666)) == -1)
	{
		iSaveErrno = errno;
		close(fdS);
		errno = iSaveErrno;
		return 1;
	}
	while ((iBytes = read(fdS, szBuf, sizeof(szBuf))) > 0)
	{
		szPtr = szBuf;
		do {
			if ((iWritten = write(fdT, szPtr, iBytes)) <= 0)
			{
				iSaveErrno = errno;
				close(fdS);
				close(fdT);
				errno = iSaveErrno;
				return 1;
			}
			szPtr += iWritten;
			iBytes -= iWritten;
		} while (iBytes > 0);
	}
	iSaveErrno = errno;
	close(fdS);
	close(fdT);
	errno = iSaveErrno;

	if (iBytes < 0)
		return 1;

	return 0;
}
