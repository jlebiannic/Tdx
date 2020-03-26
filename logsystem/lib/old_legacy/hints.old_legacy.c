#include "conf/local_config.h"
/*========================================================================
        E D I S E R V E R

        File:		hints.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Update .created and .modified

	These files contain the most recently created/modified
	indexes.
	Every time an entry is created, the index is inserted at
	the beginning of .created
	Every time an entry is modified, the index is inserted at
	the beginning of .modified
	Other entries are shifted downwards by one.

	Errors are accepted silently.
	Use writev instead of silly copying.
========================================================================*/
static char *version = "@(#)TradeXpress $Id: hints.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $";
static char *uname = UNAME;
extern char *liblogsystem_version;
static char **lib = &liblogsystem_version;
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifndef MACHINE_WNT
#include <unistd.h>
#include <sys/uio.h>
#endif

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"

old_legacy_logsys_freehints(hints)
	LogHints *hints;
{
	if (hints)
		free(hints);
}

/*
 * Read in hints from given file
 */
LogHints *
old_legacy_logsys_gethints(log, basename, count)
	LogSystem *log;
	char *basename;
	int *count;
{
	struct stat st;
	LogHints *hints;
	int fd, n;

	if ((fd = old_legacy_logsys_openfile(log, basename,
			O_RDONLY | O_BINARY, 0)) < 0)
		return (NULL);

	old_legacy_logfd_readlock(fd);

	if (fstat(fd, &st) || st.st_size < sizeof(*hints)) {
		close(fd);
		return (NULL);
	}
	if ((hints = (void *) malloc(st.st_size)) == NULL) {
		close(fd);
		return (NULL);
	}
	if (read(fd, hints, st.st_size) != st.st_size) {
		free(hints);
		close(fd);
		return (NULL);
	}
	/*
	 * Closing releases the lock set above.
	 */
	close(fd);

	n = st.st_size / sizeof(*hints);

	/*
	 * There probably is a couple of purged indexes
	 * at the end. Caller does not want to know about them.
	 */
	while (--n >= 0 && hints[n].idx == 0)
		;

	*count = n + 1;

	return (hints);
}

/*
 * Nothing is done, if configured hint-count is zero.
 * File is NOT created, if it is missing.
 */
void
old_legacy_logsys_updatehints(log, idx, stamp, basename, maxcount)
	LogSystem *log;
	LogIndex idx;
	TimeStamp stamp;
	char *basename;
	int maxcount;
{
	struct iovec iov[3];
	int niov = 0;

	LogHints *hints, first;
	int size, maxsize, count;
	int i, fd;

	if (maxcount <= 0)
		return;

	if ((fd = old_legacy_logsys_openfile(log, basename,
			O_RDWR | O_BINARY, 0)) < 0)
		return;

	old_legacy_logfd_writelock(fd);

	/*
	 * File exists and is now locked by us.
	 * Simply closing the file gets rid of lock.
	 */
	maxsize = maxcount * sizeof(*hints);

	if ((hints = (void *) malloc(maxsize)) == NULL) {
		close(fd);
		return;
	}
	/*
	 * Read in all we had earlier.
	 * Zero length is not an error.
	 */
	if ((size = read(fd, hints, maxsize)) < 0)
		goto out;

	count = size / sizeof(*hints);

	/*
	 * We will go first, in any case.
	 */
	first.idx = idx;
	first.stamp = stamp;

	iov[niov].iov_base = (void *) &first;
	iov[niov].iov_len = sizeof(first);
	++niov;

	if (count) {
		for (i = 0; i < count; ++i)
			if (hints[i].idx == idx)
				break;
		if (i == 0) {
			/*
			 * Already first, skip rewrite.
			 */
			goto out;
		}
		if (i < count) {
			/*
			 * We were there earlier, count does not change,
			 * we only get lifted to beginning.
			 *
			 * Hints that were before us.
		 	 * Hints that were after us.
			 */
			iov[niov].iov_base = (void *) &hints[0];
			iov[niov].iov_len = i * sizeof(*hints);
			++niov;

			iov[niov].iov_base = (void *) &hints[i + 1];
			iov[niov].iov_len = (count - i - 1) * sizeof(*hints);
			++niov;
		} else {
			/*
			 * We are probably dropping somebody.
			 */
			iov[niov].iov_base = (void *) &hints[0];
			iov[niov].iov_len = count * sizeof(*hints);
			++niov;
		}
		/*
		 * Make sure we dont overgrow the file.
		 */
		size = 0;
		for (count = 0; count < niov; ++count) {
			size += iov[count].iov_len;
			if (size > maxsize) {
				iov[count].iov_len -= size - maxsize;
				niov = count + 1;
				break;
			}
		}
	}
	if (lseek(fd, 0, 0) != -1)
		writev(fd, iov, niov);
out:
	free(hints);
	close(fd);
}

/*
 * Zero given index from hints-file.
 * Index is pushed last in the file and zeroed.
 * Nothing is done, if configured hint-count is zero.
 */
void
old_legacy_logsys_removehints(log, idx, basename, maxcount)
	LogSystem *log;
	LogIndex idx;
	char *basename;
	int maxcount;
{
	struct iovec iov[3];
	int niov = 0;

	LogHints *hints;
	int size, maxsize, count;
	int i, fd;

	if (maxcount <= 0)
		return;

	if ((fd = old_legacy_logsys_openfile(log, basename,
			O_RDWR | O_BINARY, 0)) < 0)
		return;

	old_legacy_logfd_writelock(fd);

	/*
	 * File exists and is now locked by us.
	 * Simply closing the file gets rid of lock.
	 */
	maxsize = maxcount * sizeof(*hints);

	if ((hints = (void *) malloc(maxsize)) == NULL) {
		close(fd);
		return;
	}
	/*
	 * Read in all we had earlier.
	 * Zero length ? Nothing to remove.
	 */
	if ((size = read(fd, hints, maxsize)) <= 0)
		goto out;
	if ((count = size / sizeof(*hints)) < 1)
		goto out;

	for (i = 0; i < count; ++i)
		if (hints[i].idx == idx)
			break;
	if (i == count) {
		/*
		 * Not there.
		 */
		goto out;
	}
	hints[i].idx = 0;

	if (i == count - 1) {
		/*
		 * We were last. write 0..
		 */

		iov[niov].iov_base = (void *) &hints[0];
		iov[niov].iov_len = count * sizeof(*hints);
		++niov;

	} else if (i == 0) {
		/*
		 * We were first. Write 1.. and 0
		 */
		iov[niov].iov_base = (void *) &hints[1];
		iov[niov].iov_len = (count - 1) * sizeof(*hints);
		++niov;

		iov[niov].iov_base = (void *) &hints[i];
		iov[niov].iov_len = sizeof(*hints);
		++niov;

	} else {
		/*
		 * Somewhere in the middle. write 0..i i.. i
		 */
		iov[niov].iov_base = (void *) &hints[0];
		iov[niov].iov_len = i * sizeof(*hints);
		++niov;

		iov[niov].iov_base = (void *) &hints[i + 1];
		iov[niov].iov_len = (count - i - 1) * sizeof(*hints);
		++niov;

		iov[niov].iov_base = (void *) &hints[i];
		iov[niov].iov_len = sizeof(*hints);
		++niov;
	}
	if (lseek(fd, 0, 0) != -1)
		writev(fd, iov, niov);
out:
	free(hints);
	close(fd);
}

/*
 * Update-interfaces.
 */

void
old_legacy_logentry_created(entry)
	LogEntry *entry;
{
	old_legacy_logsys_updatehints(entry->logsys, entry->idx, entry->record_ctime,
		LSFILE_CREATED, entry->logsys->label->creation_hints);
}

void
old_legacy_logentry_modified(entry)
	LogEntry *entry;
{
	old_legacy_logsys_updatehints(entry->logsys, entry->idx, entry->record_mtime,
		LSFILE_MODIFIED, entry->logsys->label->modification_hints);
}

/*
 * Read-interfaces
 */

LogHints *
old_legacy_logsys_modifiedhints(log, count)
	LogSystem *log;
	int *count;
{
	if (count)
		*count = 0;
	return (old_legacy_logsys_gethints(log, LSFILE_MODIFIED, count));
}

LogHints *
old_legacy_logsys_createdhints(log, count)
	LogSystem *log;
	int *count;
{
	if (count)
		*count = 0;
	return (old_legacy_logsys_gethints(log, LSFILE_CREATED, count));
}

/*
 * Remove-interface.
 */

void
old_legacy_logentry_purgehints(entry)
	LogEntry *entry;
{
	old_legacy_logsys_removehints(entry->logsys, entry->idx,
		LSFILE_CREATED, entry->logsys->label->creation_hints);

	old_legacy_logsys_removehints(entry->logsys, entry->idx,
		LSFILE_MODIFIED, entry->logsys->label->modification_hints);
}

