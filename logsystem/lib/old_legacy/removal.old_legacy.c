/*========================================================================
        E D I S E R V E R

        File:		logsystem/removal.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/

#ifdef MACHINE_WNT

/* replace all */

#include "removal.old_legacy.c.w.NT"

#else /* ! WNT */

#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: removal.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 02.04.96/JN	LOGSYS_EXT_IS_DIR.
  3.02 26.09.97/JN	Leak in remove.
  3.03 09.11.05/CD	Do not try to remove if entry is not active.
                    Before we were doing a deletion even on already 
                    deleted entries, which could cause problem in database
                    internal structure
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

/*
 *  Just single file or all including entry itself.
 */
old_legacy_logentry_remove(log, idx, extension)
	LogSystem *log;
	LogIndex idx;
	char *extension;
{
	LogEntry *entry = old_legacy_logentry_readindex(log, idx);
	int rv = 0;

	if (!entry)
		return (-1);

    /* 3.03/CD do not try to remove if entry is not active */
	if (!ACTIVE_ENTRY(entry->record))
		return (-1);

	if (extension) {
		/*
		 *  Delete single file and release entry just read.
		 */
		rv = old_legacy_logentry_removefiles(entry, extension);
		old_legacy_logentry_free(entry);
	} else {
		/*
		 *  Delete all files and, if successfull,
		 *  try to remove the entry itself.
		 *  If all files cannot be removed, space of entry
		 *  has to be released here (destroy releases too).
		 */
		rv = old_legacy_logentry_removefiles(entry, NULL);
		if (rv == 0)
        {
			rv = old_legacy_logentry_destroy(entry);
        }
		else
        {
			old_legacy_logentry_free(entry);
        }
	}
	return (rv);
}

old_legacy_logsys_removebyfilter(log, lf, extension)
	LogSystem *log;
	LogFilter *lf;
	char *extension;
{
	LogEntry *entry;
	int maxcount;
	int count = 0;
	int errors = 0;
	time_t now;

	old_legacy_logsys_rewind(log);
	time(&now);
	old_legacy_logsys_refreshmap(log);

	/*
	 *  This many entries 'now'.
	 */
	maxcount = log->map->numused;

	while (count < maxcount) {
		if ((entry = old_legacy_logentry_get(log)) == NULL)
			break;

		/*
		 *  Cannot account for newly created ones.
		 */
		if (entry->record_ctime < now)
			++count;

		if (old_legacy_logentry_passesfilter(entry, lf)) {
			if (extension) {
				if (old_legacy_logentry_removefiles(entry, extension))
					++errors;
			} else {
				if (old_legacy_logentry_removefiles(entry, NULL))
					++errors;
				else {
					/*
					 *  This branch always releases entry.
					 */
					if (old_legacy_logentry_destroy(entry))
						++errors;
					entry = NULL;
				}
			}
		}
		if (entry)
			old_legacy_logentry_free(entry);
	}
	return (errors);
}

/*
 * Remove the file with "extension" under index.
 * extension in quotes, as it is not really an extension,
 * but a subdirectory in .../files.
 * That is, files are named for example like:
 * .../users/demo/syslog/files/a/12345
 *
 * If extension is NULL, all files are removed.
 *
 * .../syslog/tmp/index.* and
 * .../syslog/tmp/index   is also removed, just in case.
 *
 * Returns number of problems detected.
 * If a directory or file did not exist, it is ok.
 */
old_legacy_logentry_removefiles(entry, extension)
	LogEntry *entry;
	char *extension;
{
	char *path;
	char *base;
	int rv = 0;

	/*
	 * This gives us a static buffer long enough to hold any
	 * valid path.
	 * ALERT
	 * Dont call any logsystem-functions
	 * as long using `directory'.
	 */
	path = old_legacy_logsys_filepath(entry->logsys, LSFILE_FILES);
	base = path + strlen(path);

	if (extension) {
		if (LOGSYS_EXT_IS_DIR) {
			sprintf(base, "/%s/%d",
				extension, old_legacy_logentry_getindex(entry));
		} else {
			sprintf(base, "/%d.%s",
				old_legacy_logentry_getindex(entry), extension);
		}
		if (unlink(path) && errno != ENOENT)
			++rv;
	} else {
		/*
		 * Removing all files under this index.
		 * Scan through files-directory and try
		 * to unlink index-file in every one.
		 */
		DIRENT *dp;
		DIR *dirp;
		int error = 0;
		int namelen;
		char namebuf[64];

		sprintf(namebuf, "%d", old_legacy_logentry_getindex(entry));
		namelen = strlen(namebuf);

		if ((dirp = opendir(path)) == NULL) {
			if (errno != ENOENT)
				++rv;
		} else {
			while ((dp = readdir(dirp)) != NULL) {
				/*
				 * Skip files which start with a dot.
				 *
				 * Do not take it as an error if
				 * the file either does not exist,
				 * or the directory we tried (dp->d_name)
				 * was not a directory after all.
				 */
				if (*dp->d_name == '.')
					continue;

				if (LOGSYS_EXT_IS_DIR) {

					sprintf(base, "/%s/%s", dp->d_name, namebuf);
					if (unlink(path) != 0
					 && (errno != ENOENT && errno != ENOTDIR)) {
						++rv;
						if (error == 0)
							error = errno;
					}
				} else {
					if (strncmp(dp->d_name, namebuf, namelen) != 0
						|| (dp->d_name[namelen] != '.' &&
							dp->d_name[namelen] != 0))
						continue;

					sprintf(base, "/%s", dp->d_name);
					if (unlink(path))
						++rv;
				}
			}
			closedir(dirp);
		}
		/*
		 * Run thru tmp directory too
		 * for .../tmp/index.*
		 */
		path = old_legacy_logsys_filepath(entry->logsys, LSFILE_TMP);
		base = path + strlen(path);

		if ((dirp = opendir(path)) == NULL) {
			if (errno != ENOENT)
				++rv;
		} else {
			while ((dp = readdir(dirp)) != NULL) {
				/*
				 * ours ? index . * or just index
				 */
				if (*dp->d_name == '.')
					continue;
				if (strncmp(dp->d_name, namebuf, namelen) != 0
					|| (dp->d_name[namelen] != '.' &&
					    dp->d_name[namelen] != 0))
					continue;

				sprintf(base, "/%s", dp->d_name);
				if (unlink(path))
					++rv;
			}
			closedir(dirp);
		}
		/*
		 *  Set errno from first encountered error.
		 */
		if (rv && error)
			errno = error;
	}
	return (rv);
}

/*
 * Moves files under entry to new index.
 */
old_legacy_logentry_movefiles(entry, newidx)
	LogEntry *entry;
	LogIndex newidx;
{
	LogSystem *log = entry->logsys;
	char path[1025];
	char newpath[1025];
	char *base;
	char *newbase;
	DIRENT *dp;
	DIR *dirp;
	int rv = 0;

	strcpy(path, old_legacy_logsys_filepath(log, LSFILE_FILES));
	strcpy(newpath, path);

	base = path + strlen(path);
	newbase = newpath + strlen(newpath);

	if ((dirp = opendir(path)) == NULL)
		return (errno != ENOENT ? -1 : 0);

	if (LOGSYS_EXT_IS_DIR) {

		while ((dp = readdir(dirp)) != NULL) {
			if (*dp->d_name == '.')
				continue;

			sprintf(base, "/%s/%d",
				dp->d_name, old_legacy_logentry_getindex(entry));
			sprintf(newbase, "/%s/%d",
				dp->d_name, newidx);

			if (link(path, newpath)) {
				if (errno != ENOENT) {
					old_legacy_logsys_warning(log, "link %s to %s: %s",
						path, newpath, old_legacy_syserr_string(errno));
					++rv;
				}
			} else if (unlink(path)) {
				old_legacy_logsys_warning(log, "unlink %s: %s",
					path, old_legacy_syserr_string(errno));
				++rv;
			}
		}
	} else {
		/*
		 *  .../files/IDX.EXT
		 *
		 *  walk thru files and look for "IDX.*" or just "IDX"
		 */
		char *cp;
		int namelen;
		char namebuf[64];

		sprintf(namebuf, "%d", old_legacy_logentry_getindex(entry));
		namelen = strlen(namebuf);

		while ((dp = readdir(dirp)) != NULL) {
			if (*dp->d_name == '.')
				continue;

			if (strncmp(dp->d_name, namebuf, namelen) != 0
			 || (dp->d_name[namelen] != 0
			  && dp->d_name[namelen] != '.'))
				continue;

			sprintf(base, "/%s", dp->d_name);

			if (dp->d_name[namelen]) {
				sprintf(newbase, "/%d%s", newidx, &dp->d_name[namelen]);
			} else {
				sprintf(newbase, "/%d", newidx);
			}
			if (link(path, newpath)) {
				if (errno != ENOENT) {
					old_legacy_logsys_warning(log, "link %s to %s: %s",
						path, newpath, old_legacy_syserr_string(errno));
					++rv;
				}
			} else if (unlink(path)) {
				old_legacy_logsys_warning(log, "unlink %s: %s",
					path, old_legacy_syserr_string(errno));
				++rv;
			}
		}
	}
	closedir(dirp);
	return (rv);
}

/*
 *  Check to see if the .../files/ hierarchy is IDX.EXT
 *  or EXT/IDX
 *
 *  ESBASECOMPAT=311 can be added to environment,
 *  if 1234.l is the preferred style for files.
 *
 *  Currently we check here for < 32 (ignoring . in between) in the variable.
 */
void
old_legacy_logsys_compability_setup()
{
	char *cp;
	char major, minor;

	LOGSYS_EXT_IS_DIR = 1;

	cp = getenv("ESBASECOMPAT");

	if (cp && *cp) {
		major = *cp++;
		minor = *cp++;
		if (minor == '.')
			minor = *cp++;
		if (major < '3' || minor < '2')
			LOGSYS_EXT_IS_DIR = 0;
	}
}

#endif /* ! WNT */

