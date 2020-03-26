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

#include "removal.c.w.NT"

#else /* ! WNT */

#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: removal.c 55239 2019-11-19 13:50:31Z sodifrance $")
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
#include <sys/types.h>
#include <dirent.h>
#ifdef MACHINE_LINUX
#include <unistd.h>
#endif
#include <string.h>

#include "port.h"

#include "private.h"

#define _REMOVAL_C
#include "logsystem.sqlite.h"
#undef _REMOVAL_C

/* Just single file or all including entry itself. */
int sqlite_logentry_remove(LogSystem *log, LogIndex idx, char *extension)
{
	LogEntry *entry = sqlite_logentry_readindex(log, idx);
	int rv = 0;

	if (!entry)
		return (-1);

	if (extension)
    {
		/* Delete single file and release entry just read. */
		rv = sqlite_logentry_removefiles(entry, extension);
		sqlite_logentry_free(entry);
	}
    else
    {
		/* Delete all files and, if successfull,
		 * try to remove the entry itself.
		 * If all files cannot be removed, space of entry
		 * has to be released here (destroy releases too). */
		rv = sqlite_logentry_removefiles(entry, NULL);
		if (rv == 0)
			rv = sqlite_logentry_destroy(entry);
		else
			sqlite_logentry_free(entry);
	}
	return (rv);
}

/* when removing by filter, we can't do a DELETE WHERE "filters" directly 
 * cause we must be sure we have destroy all extensions files for an entry 
 * before proceeding to the DELETE */
int sqlite_logsys_removebyfilter(LogSystem *log, LogFilter *lf, char *extension)
{
    LogIndex *indexes;
    int matchingIndexes = 0;
    int errors = 0;
    int result = 0;

    indexes = sqlite_logsys_list_indexed(log,lf);
    
	if (indexes == NULL)
		return 0; /* no answer is not an error ? */
	
    while (indexes[matchingIndexes] != 0)
    {
        result = sqlite_logentry_remove(log,indexes[matchingIndexes],extension);
        if (result == -1)
            ++errors;
        else
            errors += result;
        
        matchingIndexes++;
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
int sqlite_logentry_removefiles(LogEntry *entry, char *extension)
{
	char *path;
	char *base;
	int rv = 0;

	/* This gives us a static buffer long enough to hold any
	 * valid path.
	 * ALERT
	 * Dont call any logsystem-functions
	 * as long using `directory'. */
	path = sqlite_logsys_filepath(entry->logsys, LSFILE_FILES);
	base = path + strlen(path);

	if (extension)
    {
		if (LOGSYS_EXT_IS_DIR)
			sprintf(base, "/%s/%d", extension, entry->idx);
		else
			sprintf(base, "/%d.%s", entry->idx, extension);

		if (unlink(path) && errno != ENOENT)
			++rv;
	}
    else
    {
		/* Removing all files under this index.
		 * Scan through files-directory and try
		 * to unlink index-file in every one. */
		DIRENT *dp;
		DIR *dirp;
		int error = 0;
		int namelen;
		char namebuf[64];

		sprintf(namebuf, "%d", entry->idx);
		namelen = strlen(namebuf);

		if ((dirp = opendir(path)) == NULL)
        {
			if (errno != ENOENT)
				++rv;
        }
		else
        {
			while ((dp = readdir(dirp)) != NULL)
            {
				/* Skip files which start with a dot.
				 *
				 * Do not take it as an error if
				 * the file either does not exist,
				 * or the directory we tried (dp->d_name)
				 * was not a directory after all. */
				if (*dp->d_name == '.')
					continue;

				if (LOGSYS_EXT_IS_DIR)
                {
					sprintf(base, "/%s/%s", dp->d_name, namebuf);
					if (unlink(path) != 0 && (errno != ENOENT && errno != ENOTDIR))
                    {
						++rv;
						if (error == 0)
							error = errno;
					}
				}
                else
                {
					if (strncmp(dp->d_name, namebuf, namelen) != 0 || (dp->d_name[namelen] != '.' && dp->d_name[namelen] != 0))
						continue;
					sprintf(base, "/%s", dp->d_name);
					if (unlink(path))
						++rv;
				}
			}
			closedir(dirp);
		}
		/* Run thru tmp directory too
		 * for .../tmp/index.* */
		path = sqlite_logsys_filepath(entry->logsys, LSFILE_TMP);
		base = path + strlen(path);

		if ((dirp = opendir(path)) == NULL)
        {
			if (errno != ENOENT)
				++rv;
		}
        else
        {
			while ((dp = readdir(dirp)) != NULL)
            {
				/* ours ? index . * or just index */
				if (*dp->d_name == '.')
					continue;
				if (strncmp(dp->d_name, namebuf, namelen) != 0 || (dp->d_name[namelen] != '.' && dp->d_name[namelen] != 0))
					continue;
				sprintf(base, "/%s", dp->d_name);
				if (unlink(path))
					++rv;
			}
			closedir(dirp);
		}
		/* Set errno from first encountered error. */
		if (rv && error)
			errno = error;
	}
	return (rv);
}

/* Moves files under entry to new index. */
int sqlite_logentry_movefiles(LogEntry *entry, LogIndex newidx)
{
	LogSystem *log = entry->logsys;
	char path[1025];
	char newpath[1025];
	char *base;
	char *newbase;
	DIRENT *dp;
	DIR *dirp;
	int rv = 0;

	strcpy(path, sqlite_logsys_filepath(log, LSFILE_FILES));
	strcpy(newpath, path);

	base = path + strlen(path);
	newbase = newpath + strlen(newpath);

	if ((dirp = opendir(path)) == NULL)
		return (errno != ENOENT ? -1 : 0);

	if (LOGSYS_EXT_IS_DIR)
    {
		while ((dp = readdir(dirp)) != NULL)
        {
			if (*dp->d_name == '.')
				continue;
			sprintf(base, "/%s/%d", dp->d_name, entry->idx);
			sprintf(newbase, "/%s/%d", dp->d_name, newidx);

			if (link(path, newpath))
            {
				if (errno != ENOENT)
                {
					sqlite_logsys_warning(log, "link %s to %s: %s", path, newpath, sqlite_syserr_string(errno));
					++rv;
				}
			}
            else if (unlink(path))
            {
				sqlite_logsys_warning(log, "unlink %s: %s", path, sqlite_syserr_string(errno));
				++rv;
			}
		}
	}
    else
    {
		/* .../files/IDX.EXT
		 * walk thru files and look for "IDX.*" or just "IDX" */
		int namelen;
		char namebuf[64];

		sprintf(namebuf, "%d", entry->idx);
		namelen = strlen(namebuf);

		while ((dp = readdir(dirp)) != NULL)
        {
			if (*dp->d_name == '.')
				continue;

			if (strncmp(dp->d_name, namebuf, namelen) != 0 || (dp->d_name[namelen] != 0 && dp->d_name[namelen] != '.'))
				continue;

			sprintf(base, "/%s", dp->d_name);

			if (dp->d_name[namelen])
				sprintf(newbase, "/%d%s", newidx, &dp->d_name[namelen]);
			else
				sprintf(newbase, "/%d", newidx);

			if (link(path, newpath))
            {
				if (errno != ENOENT)
                {
					sqlite_logsys_warning(log, "link %s to %s: %s", path, newpath, sqlite_syserr_string(errno));
					++rv;
				}
			}
            else if (unlink(path))
            {
				sqlite_logsys_warning(log, "unlink %s: %s", path, sqlite_syserr_string(errno));
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
void sqlite_logsys_compability_setup()
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

