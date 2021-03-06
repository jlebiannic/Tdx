/*========================================================================
        E D I S E R V E R

        File:		logsystem/copyfiles.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Copying supplemental files between entries.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: copyfiles.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/* LIBRARY(liblogsystem_version) */
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 04.12.97/JN	Created.
  3.01 30.11.99/KP	Try to create dst. directory if it does not exist (NT)
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#ifdef MACHINE_HPUX
#define _LARGEFILE64_SOURCE
#else
#define __USE_LARGEFILE64
#endif
#ifdef MACHINE_WNT
#define open64 open
#endif

#ifndef MACHINE_WNT
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#endif
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

static int old_legacy_docopy(LogEntry *src, char *spath, LogEntry *dst, char *dpath);

old_legacy_logentry_copyfiles(LogEntry *src, LogEntry *dst)
{
#ifdef MACHINE_WNT
	HANDLE dirp;
	WIN32_FIND_DATA dp;
#define dname dp.cFileName
#else
	DIRENT *dp;
	DIR *dirp;
#define dname dp->d_name
#endif
	int rv = 0;
	char *srcbase;
	char *dstbase;
	char srcpath[1025];
	char dstpath[1025];

	strcpy(srcpath, old_legacy_logsys_filepath(src->logsys, LSFILE_FILES));
	strcpy(dstpath, old_legacy_logsys_filepath(dst->logsys, LSFILE_FILES));

	srcbase = srcpath + strlen(srcpath);
	dstbase = dstpath + strlen(dstpath);

#ifdef MACHINE_WNT
	strcat(srcpath, "\\*");
	dirp = FindFirstFile(srcpath, &dp);
	srcpath[strlen(srcpath) - 2] = 0;

	if (dirp == INVALID_HANDLE_VALUE) {
		dirp = NULL;
		if (GetLastError() == ERROR_FILE_NOT_FOUND
		 || GetLastError() == ERROR_PATH_NOT_FOUND)
			errno = ENOENT;
		else
			errno = -1;
	}
#else
	dirp = opendir(srcpath);
#endif
	if (dirp == NULL)
		return (errno != ENOENT ? -1 : 0);

	if (LOGSYS_EXT_IS_DIR) {

#ifdef MACHINE_WNT
		do
#else
		while ((dp = readdir(dirp)) != NULL)
#endif
		{
			if (*dname == '.')
				continue;

			sprintf(srcbase, "/%s/%d",
				dname, old_legacy_logentry_getindex(src));
			sprintf(dstbase, "/%s/%d",
				dname, old_legacy_logentry_getindex(dst));

			rv += old_legacy_docopy(src, srcpath, dst, dstpath);
		}
#ifdef MACHINE_WNT
		while (FindNextFile(dirp, &dp));
#endif
	} else {
		/*
		 *  .../files/IDX.EXT
		 *
		 *  walk thru files and look for "IDX.*" or just "IDX"
		 */
		char *cp;
		int namelen;
		char namebuf[64];

		sprintf(namebuf, "%d", old_legacy_logentry_getindex(src));
		namelen = strlen(namebuf);

#ifdef MACHINE_WNT
		do
#else
		while ((dp = readdir(dirp)) != NULL)
#endif
		{
			if (*dname == '.')
				continue;

			if (strncmp(dname, namebuf, namelen) != 0
			 || (dname[namelen] != 0
			  && dname[namelen] != '.'))
				continue;

			sprintf(srcbase, "/%s", dname);

			if (dname[namelen]) {
				sprintf(dstbase, "/%d%s",
					old_legacy_logentry_getindex(dst),
					&dname[namelen]);
			} else {
				sprintf(dstbase, "/%d",
					old_legacy_logentry_getindex(dst));
			}
			rv += old_legacy_docopy(src, srcpath, dst, dstpath);
		}
#ifdef MACHINE_WNT
		while (FindNextFile(dirp, &dp));
#endif
	}
#ifdef MACHINE_WNT
	FindClose(dirp);
#else
	closedir(dirp);
#endif
	return (rv);
}

/*
 *  /bin/cp really - give warning and return 1 on error.
 */

static int
old_legacy_docopy(LogEntry *src, char *srcpath, LogEntry *dst, char *dstpath)
{
#ifdef MACHINE_WNT
	int dirfix = 1;
	/*
	 *  Use W32 API to copy.
	 */
again:
	if (CopyFile(srcpath, dstpath, FALSE /* overwrite */))
		return (0);

	/*
	 *  Nonexisting source ? Not an error.
	 */
	if (GetLastError() == ERROR_FILE_NOT_FOUND)
		return (0);

	/*
	 * 3.01/KP
	 * CopyFile() failed, try to create the dst directory...
	 * (note that we do not know that this error is due to 
	 * the missing dstdir... but this assumption is rather safe.
	 * Right?
	 */
	if (dirfix) {
        char *cp, *cp2;

		/*
		 * We try just once...
		 */
		dirfix = 0;

        /*
         *  Accept either / or \ or a mixture of them both.
         *  Scan thru problem_path and remember last
         *  separator seen.
         */
        cp = NULL;
        for (cp2 = dstpath; *cp2; ++cp2) {
            if (*cp2 == '\\'
             || *cp2 == '/')
                cp = cp2;
		}

		/*
		 * Now:
		 * cp = pointer to last pathseparator (or NULL if none found)
		 */
		if (cp) { 
			char c = *cp;

			*cp = 0;
			if (CreateDirectory(dstpath, NULL)) {
				*cp = c;
				goto again;
			}
			*cp = c;
		}
	}

	/*
	 *  No idea what went wrong.
	 */
	return (1);

#else /* U*X */

	int dirfix = 1;
	int fi = -1, fo = -1;
	int cc, saverr;
	char *cp;
	char buf[8192];

	if ((fi = open64(srcpath, O_RDONLY)) == -1) {
		/*
		 *  Someone just removed it ?
		 */
		if (errno == ENOENT)
			return (0);

		old_legacy_logsys_warning(src->logsys, "file %s: %s", srcpath, old_legacy_syserr_string(errno));
		goto err;
	}
again:
	if ((fo = open64(dstpath, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1) {
		/*
		 *  New subdir ? Create it, if so.
		 */
		if (errno == ENOENT && dirfix && LOGSYS_EXT_IS_DIR) {
			dirfix = 0;
			if ((cp = strrchr(dstpath, '/')) != NULL) {
				*cp = 0;
				mkdir(dstpath, 0777);
				*cp = '/';
				goto again;
			}
			errno = ENOENT;
		}
		old_legacy_logsys_warning(dst->logsys, "file %s: %s", dstpath, old_legacy_syserr_string(errno));
		goto err;
	}
	while ((cc = read(fi, buf, sizeof(buf))) > 0) {
		if (write(fo, buf, cc) != cc) {
			/*
			 *  Disk full ? No other likely errors.
			 *  Remove the partial (or empty) dst file.
			 */
			saverr = errno;
			unlink(dstpath);
			errno = saverr;
			old_legacy_logsys_warning(dst->logsys, "file %s: %s", dstpath, old_legacy_syserr_string(errno));
			goto err;
		}
	}
	if (cc != 0) {
		/*
		 *  Very rare...
		 */
		old_legacy_logsys_warning(src->logsys, "file %s: %s", srcpath, old_legacy_syserr_string(errno));
		goto err;
	}
	if (close(fo)) {
		saverr = errno;
		unlink(dstpath);
		errno = saverr;
		old_legacy_logsys_warning(dst->logsys, "file %s: %s", dstpath, old_legacy_syserr_string(errno));
		goto err;
	}
	close(fi);

	return (0);

err:
	if (fi != -1)
		close(fi);
	if (fo != -1)
		close(fo);

	return (1);

#endif /* U*X */
}

