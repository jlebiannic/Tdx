/*========================================================================
        E D I S E R V E R

        File:		tr_getuniq.c
        Author:		Juha Nurmela (JN)
        Date:		Thu May 25 09:09:49 EET 1995

        Copyright (c) 1995 Telecom Finland/EDI Operations

	Converted from getunique.tcl in .../src/edisend/.

	Called as tfGetUniq(LOGENTRY, "filezo", "FIXED", "COUNTER", "MAXCOUNTER").

	FIXED, COUNTER and MAXCOUNTER are often, BUT NOT ALWAYS,
	really called as "FIXED", "COUNTER" and "MAXCOUNTER".

 	LOGENTRY is a struct LogSysHandle * which points to an existing
	entry in the logsystem. It can be NULL, in which case the tExt
	is used as an ordinary file name.

	A unique string is returned, consisting of a fixed prefix
	and variable suffix.
	If both <COUNTER> and <MAXCOUNTER> are empty, only the fixed
	part is returned. Then, if fixed part is empty too,
	a freerunning counter is returned.

	Failure return is TR_EMPTY.

	The variable suffix is always >= minimum and <= maximum,
	minimum being the value of <COUNTER> and
	maximum being the value of <MAXCOUNTER>.

	Values for <FIXED>, <COUNTER> and <MAXCOUNTER> are looked in
	filezo, filezo is a regular, not changed, namevalue file.
	The counter proper is kept in separate file, called
	filezo_<COUNTER>, where is only a single line containing the
	current counter value. Actually there might be garbage after 
	the first line, from older, longer, counter values. This is ignored.

	XXX 4.00/KP : filezo is no longer a regular file, but an extension
		      table of the database, thus no direct file oprations
		      can be made...
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: tr_getuniq.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.01 30.05.95/JN	Created.
  3.02 27.11.96/JN	NT compatlib removed.
  3.03 16.06.98/JN	rls aware getuniq()
  4.00 09.09.98/KP	input file may not be a real file anymore... 
		 	(see XXX above)
  4.01 04.12.99/KP	Changed parameters for tr_lsTryGetUniq()
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef MACHINE_WNT
#include <io.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifndef MACHINE_LINUX
extern int errno;
#endif

extern char *TR_EMPTY;

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

char *tfGetUniq(char *tExt, char *tFixed, char *tCounter, char *tMaxCounter)
{
	return tfGetUniqEx( NULL, tExt, tFixed, tCounter, tMaxCounter);
}

char *tfGetUniqEx(LogSysHandle *pLogentry, char *tExt, char *tFixed, char *tCounter, char *tMaxCounter)
{
#ifndef MACHINE_WNT
	int n, fd = -1;
#else
	int retries;
	DWORD n, m;
	HANDLE h = INVALID_HANDLE_VALUE;
#endif
	FILE *fp = NULL;
	char *value, *cp;
	char buf[512];

	char *CounterFile = NULL;
	char *Result      = TR_EMPTY;

	char *FixedPart   = NULL;
	int   MinCounter  = 0;
	int   MaxCounter  = 0;
	int   Counter     = 0;

	/*
	 * Read in the values for Fixed and MaxCounter.
	 */
	if (pLogentry) {
		/* 
		Use database parameter fetch
		*/
		MinCounter = atoi(tr_am_dbparmget(pLogentry, tExt, tCounter));
		MaxCounter = atoi(tr_am_dbparmget(pLogentry, tExt, tMaxCounter));
		FixedPart  = tr_am_dbparmget(pLogentry, tExt, tFixed);
	}
	else {
		/* 
		This is the old way... read input from a regular file
		*/
		if ((fp = tr_fopen(tExt, "r")) == NULL)
			goto out;

		while (fgets(buf, sizeof(buf), fp)) {
			/*
			 * Ignore comment lines
			 * and lines without =.
			 */
			if (*buf == '#' || (cp = strchr(buf, '=')) == NULL)
				continue;
			*cp = 0;
			value = cp + 1;

			if (!strcmp(buf, tFixed)) {
				if ((cp = strchr(value, '\r')) != NULL)
					*cp = 0;
				if ((cp = strchr(value, '\n')) != NULL)
					*cp = 0;
				if (FixedPart)
					tr_free(FixedPart);
				FixedPart = tr_strdup(value);
				continue;
			}
			if (!strcmp(buf, tCounter)) {
				MinCounter = atoi(value);
				continue;
			}
			if (!strcmp(buf, tMaxCounter)) {
				MaxCounter = atoi(value);
				continue;
			}
		}
		tr_fclose(fp, 0);
	}

	/*
	 * Now we have all constant data we need.
	 *
	 * If there is no values for neither min nor max
	 * just return the fixed part.
	 *
	 * Otherwise, use the counter file to get the variable part.
	 */
	if (FixedPart != NULL
	 && MinCounter == 0
	 && MaxCounter == 0) {
		Result = tr_BuildText("%s", FixedPart);
		goto out;
	}
	if (MinCounter < 0)
		MinCounter = 0;
	if (MaxCounter < 0)
		MaxCounter = 0;

	/*
	 *  Check if it is a remote counter, and if not, 
	 *  continue using old code (could be just a local file).
	 *
	 *  First try a "new" method, where we have a LogSysHandle and extension...
	 *
	 * 4.01/KP : Changed params...
	 */
	if ((!pLogentry) || !tr_lsTryGetUniq(pLogentry, tExt, tCounter, MinCounter, MaxCounter, &Counter)) {
		/*
		 *  ...if that did not work out, try to get remote counter by rls filename...
		 */
		if (!tr_lsTryGetUniq(NULL, tExt, tCounter, MinCounter, MaxCounter, &Counter)) {
			/*
			 *  ...and as a last resort, try if this is a normal local file
			 */
			CounterFile = tr_malloc(
				strlen(tExt) + strlen("_") + strlen(tCounter) + 1);

			sprintf(CounterFile, "%s_%s", tExt, tCounter);

			/*
			 * Start of different ----------------------
			 */
#ifndef MACHINE_WNT

#define CLOSE_COUNTER_FD if (fd >= 0) close(fd);

			if ((fd = open(CounterFile, O_RDWR | O_CREAT, 0666)) == -1) {
				fprintf(stderr, "Cannot lock counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}
			if (lockf(fd, F_LOCK, 0)) {
				fprintf(stderr, "Cannot lock counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}
			if ((n = read(fd, buf, sizeof(buf) - 1)) < 0) {
				fprintf(stderr, "Cannot read counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}
			buf[n] = 0;
			Counter = atoi(buf);

			if (Counter < MinCounter || (Counter > MaxCounter && MaxCounter > 0))
				Counter = MinCounter;

			sprintf(buf, "%d\n", Counter + 1);
			n = strlen(buf);

			if (lseek(fd, 0, 0) != 0) {
				fprintf(stderr, "Cannot seek counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}

			if (write(fd, buf, n) != n) {
				fprintf(stderr, "Cannot rewrite counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}
			if (lockf(fd, F_ULOCK, 0)) {
			fprintf(stderr, "Warning: unlocking counter %s (%d)\n",
			CounterFile, errno);
			}
			if (close(fd)) {
				fprintf(stderr, "Close failed with counter %s (%d)\n",
					CounterFile, errno);
				goto out;
			}
			fd = -1;

#else /* NT */

#define CLOSE_COUNTER_FD if (h != INVALID_HANDLE_VALUE) CloseHandle(h);

			if ((h = CreateFile(CounterFile,
					GENERIC_READ | GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL,
					NULL)) == INVALID_HANDLE_VALUE) {
					fprintf(stderr, "Cannot lock counter %s (%d)\n",
						CounterFile, GetLastError());
					goto out;
				}
				retries = 0;
		retry:
			if (!LockFile(h, 0, 0, 1, 0)) {
				if (++retries <= 8) {
					Sleep(retries);
					goto retry;
				}
				fprintf(stderr, "Cannot lock counter %s (%d)\n",
					CounterFile, GetLastError());
				goto out;
			}
			if (!ReadFile(h, buf, sizeof(buf) - 1, &n, NULL)) {
				fprintf(stderr, "Cannot read counter %s (%d)\n",
				CounterFile, GetLastError());
			goto out;
			}
			buf[n] = 0;
			Counter = atoi(buf);

			if (Counter < MinCounter || (Counter > MaxCounter && MaxCounter > 0))
				Counter = MinCounter;

			sprintf(buf, "%d\n", Counter + 1);
			n = strlen(buf);

			if (SetFilePointer(h, 0, NULL, FILE_BEGIN) != 0) {
				fprintf(stderr, "Cannot seek counter %s (%d)\n",
					CounterFile, GetLastError());
				goto out;
			}
			if (!WriteFile(h, buf, n, &m, NULL) || m != n) {
				fprintf(stderr, "Cannot rewrite counter %s (%d)\n",
					CounterFile, GetLastError());
				goto out;
			}
			if (!UnlockFile(h, 0, 0, 1, 0)) {
				fprintf(stderr, "Warning: unlocking counter %s (%d)\n",
					CounterFile, GetLastError());
			}
			if (!CloseHandle(h)) {
				fprintf(stderr, "Close failed with counter %s (%d)\n",
					CounterFile, GetLastError());
				goto out;
			}
			h = INVALID_HANDLE_VALUE;

#endif /* NT */

		/*
		 * End of differing ----------------------
		 */
		}
	}
	if (MaxCounter) {
		/*
		 * Pad the result to be the same length as maxcounter.
		 */
		sprintf(buf, "%d", MaxCounter);
		Result = tr_BuildText("%s%0*d",
			FixedPart ? FixedPart : "",
			strlen(buf), Counter);
	} else {
		Result = tr_BuildText("%s%d",
			FixedPart ? FixedPart : "",
			Counter);
	}
out:
	CLOSE_COUNTER_FD;

	if (FixedPart)
		tr_free(FixedPart);
	if (CounterFile)
		tr_free(CounterFile);

	return (Result);
}

