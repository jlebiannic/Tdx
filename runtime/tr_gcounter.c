/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		runtime/tr_gcounter.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Fri Oct  9 11:01:41 EET 1992

	c_XXX counters are special, they do not autoincrement like cXXX
	but are can be operated atomically much like nXXX.

	Counters are kept as integers in the usual counter-location,
	saved as "%7d\n", read with atoi()

XXX	Important to note that using these counters in calculations
XXX	does not prevent others to nudge them concurrently.
XXX	Uh, then, whats the use ???

========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_gcounter.c 47429 2013-10-29 15:27:44Z cdemory $")
/*LIBRARY(libruntime_version)
*/
/*==============================================================================
  Record all changes here and update the above string accordingly.
  3.00 12.01.93/MV	Created.
  3.02 06.02.96/JN	Stolen from tr_counter.c
  3.03 27.11.96/JN	Get rid of nt compatlib.
==============================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#ifdef MACHINE_WNT
#include <io.h>
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "tr_strings.h"

#define TR_WARN_OPEN_FAILED    "Cannot open %s (%d)"
#define TR_WARN_CLOSE_FAILED   "Closing %s failed (%d)"
#define TR_WARN_READ_FAILED    "Reading from %s failed (%d)"
#define TR_WARN_WRITE_FAILED   "Writing to %s failed (%d)"

#ifndef MACHINE_LINUX
extern int errno;
#endif

extern char *getenv();

static int tr_operate_gcounter(char *name, int op);

double tr_set_gcounter(char *name, double value)
{
	/*
	 *  Add 1/2 into value if it is positive,
	 *  so 1.999999999 gets into 2
	 *  XXX Negative ?
	 */
	return (tr_operate_gcounter(name, value > 0 ? (int)(value + 0.5) : (int)(value)));
}

double tr_get_gcounter(char *name)
{
	return (tr_operate_gcounter(name, 0));
}

double tr_inc_gcounter(char *name)
{
	return (tr_operate_gcounter(name, 1));
}

double tr_dec_gcounter(char *name)
{
	return (tr_operate_gcounter(name, -1));
}

/*
 *  Add op into named counter, return new value
 *  Rewritten if counter changed value.
 *
 *  XXX expand to generic calculation
 */

static int tr_operate_gcounter(char *name, int op)
{
	char ctrbuf[128]; /* and that is a long integer... */
	char ctrfile[1028];
	int  ctr = 0;
	int  retry = 0;
	int  err;
#ifndef MACHINE_WNT
	int  cc;
	char *home;
	int  fd;

	/*
	 *  $HOME mandatory
	 */
	if ((home = getenv("HOME")) == NULL) {
		tr_Log (TR_WARN_HOME_NOT_SET);
		return (0);
	}
	sprintf(ctrfile, "%s/%s/%s", home, TR_DIR_COUNTERS, name);

again:
	/*
	 *  Let umask narrow 0666
	 */
	if ((fd = open(ctrfile, O_RDWR | O_CREAT, 0666)) == -1) {
		err = errno;
		if (err == ENOENT && retry++ == 0) {
			/*
			 *  No TR_COUNTERS directory maybe ?
			 *  Try to make it and loop back if it could.
			 *  "Hanakka"
			 */
			int rv;
			char *p = strrchr(ctrfile, '/');

			*p = 0;
			rv = mkdir(ctrfile, 0777);
			*p = '/';
			if (rv == 0)
				goto again;
		}
		tr_Log (TR_WARN_OPEN_FAILED, ctrfile, err);
		errno = err;
		return (0);
	}
	if (lockf(fd, F_LOCK, 0)) {
		tr_Log (TR_WARN_LOCKING_FAILED, ctrfile, err = errno);
		close(fd);
		errno = err;
		return (0);
	}
	if ((cc = read(fd, ctrbuf, sizeof(ctrbuf) - 1)) == -1) {
		tr_Log (TR_WARN_READ_FAILED, ctrfile, err = errno);
		close(fd);
		errno = err;
		return (0);
	}
	ctrbuf[cc] = 0;

	if (cc == sizeof(ctrbuf) - 1)
		tr_Log("Warning: absurd value for %s: %s", name, ctrbuf);

	/*
	 *  Will be zero if empty.
	 *  Just an atoi is used to get it,
	 *  setting uses %7d\n, overrunning the length is ok.
	 */
	ctr = atoi(ctrbuf);

	if (op != 0) {

		ctr += op;

		sprintf(ctrbuf, "%7d\n", ctr);

		/*
		 *  Seek back if offset changed and rewrite is needed.
		 *  (no seek necessary for empty file, once only).
		 *  Log error, but continue (has to be a FIFO or something...)
		 *
		 *  ftruncate the rest, maybe, but not done.
		 *  the value is on the first line, rest always ignored.
		 */
		if (lseek(fd, 0L, 0) != 0) {
			tr_Log (TR_WARN_SEEK_FAILED, ctrfile, errno);
		}

		cc = strlen(ctrbuf);
		errno = 0;

		if (write(fd, ctrbuf, cc) != cc) {
			tr_Log (TR_WARN_WRITE_FAILED, ctrfile, err = errno);
			errno = err;
		}
	}
	/*
	 *  closing releases the lockf too
	 */
	if (close(fd)) {
		tr_Log (TR_WARN_CLOSE_FAILED, ctrfile, err = errno);
		errno = err;
	}
#else /* WNT */
	HANDLE h;
	char *cp;
	DWORD n, m;

	/*
	 *  %HOMEDRIVE %HOMEPATH
	 */
	if ((cp = getenv("HOMEDRIVE")) == NULL)
		cp = "C:";
	strcpy(ctrfile, cp);
	if ((cp = getenv("HOMEPATH")) != NULL && strcmp(cp, "\\") != 0)
		strcat(ctrfile, cp);
	strcat(ctrfile, "\\");
	strcat(ctrfile, TR_DIR_COUNTERS);
	strcat(ctrfile, "\\");
	strcat(ctrfile, name);

again:
	/*
	 *  Let umask narrow 0666
	 */
	if ((h = CreateFile(ctrfile,
			GENERIC_READ | GENERIC_WRITE,
			FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL,
			OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			NULL)) == INVALID_HANDLE_VALUE) {
		err = GetLastError();
		if ((err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)
		 && retry++ == 0) {
			/*
			 *  No TR_COUNTERS directory maybe ?
			 *  Try to make it and loop back if it could.
			 */
			int rv;
			char *p = strrchr(ctrfile, '\\');

			*p = 0;
			rv = mkdir(ctrfile, 0777);
			*p = '\\';
			if (rv == 0)
				goto again;
		}
		tr_Log (TR_WARN_OPEN_FAILED, ctrfile, err);
		SetLastError(err);
		return (0);
	}
	retry = 0;
re_try:
	if (!LockFile(h, 0, 0, 1, 0)) {
		if (++retry <= 8) {
			Sleep(retry);
			goto re_try;
		}
		tr_Log (TR_WARN_LOCKING_FAILED, ctrfile, err = GetLastError());
		CloseHandle(h);
		SetLastError(err);
		return (0);
	}
	if (!ReadFile(h, ctrbuf, sizeof(ctrbuf) - 1, &n, NULL)) {
		tr_Log (TR_WARN_READ_FAILED, ctrfile, err = GetLastError());
		CloseHandle(h);
		SetLastError(err);
		return (0);
	}
	ctrbuf[n] = 0;

	if (n == sizeof(ctrbuf) - 1)
		tr_Log("Warning: absurd value for %s: %s", name, ctrbuf);

	/*
	 *  Will be zero if empty.
	 *  Just an atoi is used to get it,
	 *  setting uses %7d\n, overrunning the length is ok.
	 */
	ctr = atoi(ctrbuf);

	if (op != 0) {

		ctr += op;

		sprintf(ctrbuf, "%7d\n", ctr);

		/*
		 *  Seek back if offset changed and rewrite is needed.
		 *  (no seek necessary for empty file, once only).
		 *  Log error, but continue (has to be a FIFO or something...)
		 *
		 *  ftruncate the rest, maybe, but not done.
		 *  the value is on the first line, rest always ignored.
		 */
		if (SetFilePointer(h, 0, NULL, FILE_BEGIN) != 0) {
			tr_Log (TR_WARN_SEEK_FAILED, ctrfile, errno);
		}

		n = strlen(ctrbuf);
		errno = 0;

		if (!WriteFile(h, ctrbuf, n, &m, NULL) || m != n) {
			tr_Log (TR_WARN_WRITE_FAILED,
				ctrfile, err = GetLastError());
			SetLastError(err);
		}
	}
	UnlockFile(h, 0, 0, 1, 0);

	if (!CloseHandle(h)) {
		tr_Log (TR_WARN_CLOSE_FAILED, ctrfile, err = GetLastError());
		SetLastError(err);
	}
#endif /* WNT */

	/*
	 *  Return (new) value.
	 */
	return (ctr);
}

