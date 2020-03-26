/*========================================================================
        E D I S E R V E R

        File:		io.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Reading and writing logsystem stuff.
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: io.old_legacy.c 55239 2019-11-19 13:50:31Z sodifrance $ $Name:  $")
/*LIBRARY(liblogsystem_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 27.04.95/JN	mmap added to readraw.
  3.02 24.01.96/JN	Solaris. MAP_FILE undef.
  3.04 03.04.96/JN	Clustering reads.
  3.05 09.11.05/CD  Set correct errorno when index not found in logentry_readindex
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef MACHINE_WNT
#include <sys/mman.h>
#include <unistd.h>
#endif

#include "logsystem.old_legacy.h"
#include "private.old_legacy.h"
#include "../logsystem.sqlite.h"
#include "runtime/tr_constants.h"
#include "../logsystem.h"

#ifndef MACHINE_WNT
#include <sys/uio.h>
#endif

#ifdef MAXIOV
#define CLUSTER_MAX MAXIOV
#endif
#ifdef UIO_MAX
#define CLUSTER_MAX UIO_MAX
#endif
#ifndef CLUSTER_MAX
#define CLUSTER_MAX 16
#endif

void old_legacy_logsys_voidclose(int fd);

TimeStamp
old_legacy_log_curtime()
{
	return ((TimeStamp) time(NULL));
}

int old_legacy_logsys_opendata(log)
	LogSystem *log;
{
	if (log->datafd >= 0)
		return (0);
retry:
	log->datafd = old_legacy_logsys_openfile(log, LSFILE_DATA,
			log->open_mode | O_BINARY, 0644);
	if (log->datafd >= 0)
		return (0);

#if 0
	if ((errno == EACCESS || errno == EPERM) && log->open_mode == O_RDWR) {
		log->open_mode = O_RDONLY;
		goto retry;
	}
#endif
	old_legacy_logsys_warning(log, "Cannot open datafile: %s", old_legacy_syserr_string(errno));
	return (-1);
}

old_legacy_logsys_openmap(log)
	LogSystem *log;
{
	if (log->mapfd >= 0)
		return (0);

	log->mapfd = old_legacy_logsys_openfile(log, LSFILE_MAP, log->open_mode | O_BINARY, 0644);
	if (log->mapfd >= 0)
		return (0);
    else
    {
        old_legacy_logsys_warning(log, "Cannot open mapfile: %s", old_legacy_syserr_string(errno));
        return (-1);
    }
}

static
old_legacy_validate(entry)
	LogEntry *entry;
{
#if 0
	/*
	 * This has never been on disk.
	 */
	if (entry->record_idx == 0)
		return;

	/*
	 * Record is marked with an index,
	 * it has to equal with entry.
	 */
	if (entry->record_idx != entry->idx)
		logsys_warning(entry->log, "Record %d has index %d",
			entry->idx, entry->record_idx);

	if (!ACTIVE_ENTRY(entry->record))
		logsys_warning(entry->log, "Record %d has zero creation-time",
			entry->idx);
#endif
}

old_legacy_log_readbufs(fd, bufs, nbufs, bufsiz, offset)
	int fd;
	void **bufs;
	int nbufs;
	int bufsiz;
	int offset;
{
	int n;
	struct iovec iov[CLUSTER_MAX];

	if (nbufs > CLUSTER_MAX)
		nbufs = CLUSTER_MAX;

	for (n = 0; n < nbufs; ++n) {
		iov[n].iov_base = bufs[n];
		iov[n].iov_len  = bufsiz;
	}
	if (lseek(fd, offset, 0) == -1) {
		return (-1);
	}
	n = readv(fd, iov, nbufs);
	if (n < 0) {
		n = 0;
	}
	return (n / bufsiz);
}

old_legacy_log_readbuf(fd, buf, nbytes, offset)
	int fd;
	char *buf;
	int nbytes;
	int offset;
{
	if (lseek(fd, offset, 0) == -1) {
		return (-1);
	}
	while (nbytes > 0) {
		int n = read(fd, buf, nbytes);
		if (n == -1 && errno == EINTR)
			continue;
		if (n <= 0)
			return (-1);
		buf += n;
		nbytes -= n;
	}
	return (0);
}

int
old_legacy_log_writebuf(fd, buf, nbytes, offset)
	int fd;
	char *buf;
	int nbytes;
	int offset;
{
	if (lseek(fd, offset, 0) == -1) {
		return (-1);
	}
	while (nbytes > 0) {
		int n = write(fd, buf, nbytes);
		if (n == -1 && errno == EINTR)
			continue;
		if (n <= 0)
			return (-1);
		buf += n;
		nbytes -= n;
	}
	return (0);
}

/*
 * Read a record from datafile.
 * Returned entry is allocated here, caller must free when done.
 * Nothing locked.
 * The given index is the user-visible index.
 */
LogEntry *
old_legacy_logentry_readindex(log, idx)
	LogSystem *log;
	LogIndex idx;
{
	LogEntry *entry;

	if (old_legacy_logsys_opendata(log))
		return (NULL);

	entry = old_legacy_logentry_alloc_skipclear(log, idx / LS_GENERATION_MOD);

	if (old_legacy_log_readbuf(log->datafd,
			entry->record, log->label->recordsize,
			entry->idx  *  log->label->recordsize)) {
		errno = ENOENT; /* 3.05/CD added correct errno */
		old_legacy_logentry_free(entry);
		return (NULL);
	}
	if (idx % LS_GENERATION_MOD
			!= entry->record_generation % LS_GENERATION_MOD) {
		errno = ENOENT;
		old_legacy_logentry_free(entry);
		return (NULL);
	}
	old_legacy_validate(entry);

	return (entry);
}

/*
 * Read a record from datafile.
 * Returned entry is allocated here, caller must free when done.
 * Nothing locked.
 * This index is the physical index, not the user-visible one.
 */
LogEntry *
old_legacy_logentry_readraw(log, idx)
	LogSystem *log;
	LogIndex idx;
{
	LogEntry *entry;
	if (old_legacy_logsys_opendata(log))
		return (NULL);

	entry = old_legacy_logentry_alloc_skipclear(log, idx);

	if (log->mmapping) {
		if (entry->idx > log->map->lastindex) {
			old_legacy_logentry_free(entry);
			return (NULL);
		}
		entry->record = MMAPPED_RECORD(log, entry->idx);
	} else {
		if (old_legacy_log_readbuf(log->datafd,
				entry->record, log->label->recordsize,
				entry->idx  *  log->label->recordsize)) {
			old_legacy_logentry_free(entry);
			return (NULL);
		}
	}
	return (entry);
}

/*
 * Read a record from datafile.
 * Returned entry is allocated here, caller must free when done.
 * Nothing locked.
 * This index is the physical index, not the user-visible one.
 */
int
old_legacy_logentry_readraw_clustered(log, idx, count, entries)
	LogSystem *log;
	LogIndex idx;
	int count;
	LogEntry **entries; /* return */
{
	int did;

	if (old_legacy_logsys_opendata(log))
		return (0);

	if (log->mmapping) {
		LogEntry *entry;

		for (did = 0; did < count; ++did) {
			entry = old_legacy_logentry_alloc_skipclear(log, idx++);

			if (entry->idx > log->map->lastindex) {
				old_legacy_logentry_free(entry);
				break;
			}
			entry->record = MMAPPED_RECORD(log, entry->idx);
			*entries++ = entry;
		}
	} else {
		int n;
		void *list[CLUSTER_MAX];

		if (count > CLUSTER_MAX)
			count = CLUSTER_MAX;

		for (n = 0; n < count; ++n) {
			entries[n] = old_legacy_logentry_alloc_skipclear(log, idx++);
			list[n] = entries[n]->record;
		}
		did = old_legacy_log_readbufs(log->datafd,
				list, count,
				log->label->recordsize,
				entries[0]->idx * log->label->recordsize);
		if (did < 0)
			did = 0;
		for (n = did; n < count; ++n)
			old_legacy_logentry_free(entries[n]);
	}
	return (did);
}

/*
 * Write a record to datafile.
 * Entry is NOT free'ed.
 * Nothing locked.
 * Record updated before write.
 */
old_legacy_logentry_write(entry)
	LogEntry *entry;
{
	LogSystem *log = entry->logsys;

	if (old_legacy_logsys_opendata(log))
		return (-1);

	/*
	 * These are overridden every time
	 * and modifications from user get silently lost.
	 */
	entry->record_idx = LS_VINDEX(entry->idx, entry->record_generation);
	entry->record_mtime = old_legacy_log_curtime();

	if (old_legacy_log_writebuf(log->datafd,
			entry->record, log->label->recordsize,
			entry->idx  *  log->label->recordsize)) {
		return (-2);
	}
	/*
	 * Update hints
	 */
	old_legacy_logentry_modified(entry);

	return (0);
}

/*
 * Write a record to datafile.
 * Entry is NOT free'ed.
 * Nothing locked.
 * Record is NOT updated before write.
 */
int
old_legacy_logentry_writeraw(entry)
	LogEntry *entry;
{
	LogSystem *log = entry->logsys;

	if (old_legacy_logsys_opendata(log))
		return (-1);

	if (old_legacy_log_writebuf(log->datafd,
			entry->record, log->label->recordsize,
			entry->idx  *  log->label->recordsize)) {
		return (-2);
	}
	return (0);
}

LogSystem *
old_legacy_logsys_open(sysname, flags)
	char *sysname;
	int flags;
{
	LogSystem *log;

	old_legacy_logsys_compability_setup();
	/*
	 *  The LOGSYS_EXT_IS_DIR was set there...
	 */

	log = old_legacy_logsys_alloc(sysname);

	log->walkidx = 1;
	log->open_mode = (flags & LS_READONLY) ? O_RDONLY : O_RDWR;

	if (old_legacy_logsys_readlabel(log)) {
		if (flags & LS_FAILOK) {
			old_legacy_logsys_free(log);
			return (NULL);
		}
		fprintf(stderr, "Cannot open logsystem %s: %s\n",
			sysname, old_legacy_syserr_string(errno));
		exit(1);
	}
	old_legacy_logsys_was_opened(log);

	return (log);
}

void old_legacy_logsys_rewind(log)
	LogSystem *log;
{
	log->walkidx = 1;
}

void
old_legacy_logsys_close(log)
	LogSystem *log;
{
	if (log->labelfd >= 0)
		close(log->labelfd);
	if (log->mapfd >= 0)
		close(log->mapfd);
	if (log->datafd >= 0)
		close(log->datafd);

	if (log->label)
		old_legacy_loglabel_free(log->label);
	old_legacy_logsys_free(log);
}

/*
 * Open the datafile of the logsystem and
 * map it into our address space.
 * Kernel is told we shall access the mapped region sequentially,
 * if this is supported (currently not in use).
 * Errors here are not fatal, as we can always use read.
 */

static old_legacy_mapit(), old_legacy_unmapit();

old_legacy_logsys_mmapdata(log)
	LogSystem *log;
{

	if (old_legacy_logsys_opendata(log))
		return (-1);

	if (old_legacy_logsys_refreshmap(log))
		return (-2);

	return (old_legacy_mapit(log));
}

old_legacy_logsys_munmapdata(log)
	LogSystem *log;
{

	if (log->mmapping == NULL)
		return (0);

	if (old_legacy_logsys_opendata(log))
		return (-1);

	if (old_legacy_logsys_refreshmap(log))
		return (-2);

	return (old_legacy_unmapit(log));
}

/*
 * Make sure we have a valid mapping, spanning all the records.
 */
old_legacy_logsys_remapdata(log)
	LogSystem *log;
{
	int len;

	if (old_legacy_logsys_opendata(log))
		return (-1);

	if (old_legacy_logsys_refreshmap(log))
		return (-2);

	len = (log->map->lastindex + 1) * log->label->recordsize;

	if (log->mmapping
	 && (char *) log->mmapping + len <= (char *) log->mmapping_end)
		return (0);

	if (log->mmapping)
		old_legacy_unmapit(log);

	return (old_legacy_mapit(log));
}

static
old_legacy_mapit(log)
	LogSystem *log;
{
	void *p;
	size_t len;
	char dummy;

	len = (log->map->lastindex + 1) * log->label->recordsize;

#ifndef MACHINE_WNT
	p = mmap(0, len, PROT_READ,
#ifdef MAP_FILE
		MAP_FILE |
#endif
		MAP_SHARED, log->datafd, 0);

	if (p == (void *) -1)
#endif
	return (-3);

	dummy = *(char *) p; /* make sure we can read the area. */

#ifdef MADV_SEQUENTIAL
	/*
	 * Grr, define but no func !
	 */
#ifndef MACHINE_SCO
#ifndef MACHINE_ATT4
	madvise(p, len, MADV_SEQUENTIAL);
#endif
#endif
#endif
	log->mmapping     = p;
	log->mmapping_end = (char *) p + len;

	return (0);
}

static
old_legacy_unmapit(log)
	LogSystem *log;
{
#ifndef MACHINE_WNT
	munmap(log->mmapping,
		(char *) log->mmapping_end - (char *) log->mmapping);
#endif
	log->mmapping     = NULL;
	log->mmapping_end = NULL;

	return (0);
}

old_legacy_logsys_readlabel(log)
	LogSystem *log;
{
	struct stat st;
	LogLabel *label;
	LogField *field;
	int fd;

	if ((fd = old_legacy_logsys_openfile(log, LSFILE_LABEL,
			O_RDONLY | O_BINARY, 0)) < 0)
		return (-1);

	if (fstat(fd, &st)) {
		old_legacy_logsys_voidclose(fd);
		return (-2);
	}
	if (st.st_size < sizeof(*label)) {
		/*
		 * It has to be at least this big.
		 */
		old_legacy_logsys_voidclose(fd);
		errno = EINVAL;		/* XXX */
		return (-3);
	}
	if ((label = old_legacy_loglabel_alloc(st.st_size)) == NULL) {
		old_legacy_logsys_voidclose(fd);
		return (-4);
	}
	if (read(fd, label, st.st_size) != st.st_size) {
		old_legacy_logsys_voidclose(fd);
		old_legacy_loglabel_free(label);
		return (-5);
	}
	if (label->magic != LSMAGIC_LABEL) {
		old_legacy_logsys_voidclose(fd);
		old_legacy_loglabel_free(label);
		old_legacy_logsys_warning(log, "Label is not valid");
		errno = EINVAL;		/* XXX */
		return (-6);
	}
	close(fd);

	/*
	 * Adjust fieldname offsets into strings.
	 */
	for (field = label->fields; field->type; ++field) {
		field->name.string   = (char *) label + field->name.offset;
		field->header.string = (char *) label + field->header.offset;
		field->format.string = (char *) label + field->format.offset;
	}
	log->label = label;
	return (0);
}

/*
 * Read in the mapfile.
 * Many fatal signals get blocked and the mapfile is locked.
 * Locks and signal blocks are restored when the map is written back
 * or when it is released.
 */
int
old_legacy_logsys_readmap(log)
	LogSystem *log;
{
	if (log->mapfd >= 0) {
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
	}
	if ((log->mapfd = old_legacy_logsys_openfile(log, LSFILE_MAP,
			log->open_mode | O_BINARY, 0)) < 0) {
		int e = errno;
		old_legacy_logsys_warning(log, "Cannot open map: %s", old_legacy_syserr_string(e));
        /* try to see if LSFILE_SQLITE do exist in case we have a sqlite base */
        {
            int testfd = open(sqlite_logsys_filepath(log, LSFILE_SQLITE), O_RDONLY);
            if ((testfd >= 0) || (errno != ENOENT))
            {
                if (testfd >= 0) close(testfd);
                sqlite_logsys_warning(log, "You have a %s file present : maybe you should try and use sqlite mode", LSFILE_SQLITE);
            }    
            return (-1);
        }
		errno = e;
		return (-2);
	}
	old_legacy_logsys_blocksignals();

	if (old_legacy_logsys_writelock(log)) {
		int e = errno;
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_restoresignals();
		old_legacy_logsys_warning(log, "Cannot lock map: %s", old_legacy_syserr_string(e));
		errno = e;
		return (-3);
	}
	if (old_legacy_log_readbuf(log->mapfd, log->map, sizeof(*log->map), 0)) {
		int e = errno;
		old_legacy_logsys_unlock(log);
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_restoresignals();
		old_legacy_logsys_warning(log, "Cannot read map: %s", old_legacy_syserr_string(e));
		errno = e;
		return (-4);
	}
	if (log->map->magic != LSMAGIC_MAP) {
		old_legacy_logsys_unlock(log);
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_restoresignals();
		old_legacy_logsys_warning(log, "Map is not a valid mapfile");
		errno = EINVAL;
		return (-5);
	}
	return (0);
}

old_legacy_logsys_writemap(log)
	LogSystem *log;
{
	int e, rv;

	rv = old_legacy_log_writebuf(log->mapfd, log->map, sizeof(*log->map), 0);
	e = errno;

	old_legacy_logsys_unlock(log);
	old_legacy_logsys_voidclose(log->mapfd);
	log->mapfd = -1;

	old_legacy_logsys_restoresignals();

	if (rv) {
		old_legacy_logsys_warning(log, "Map write failed: %s", old_legacy_syserr_string(e));
	}
	errno = e;
	return (rv ? -1 : 0);
}

void
old_legacy_logsys_releasemap(log)
	LogSystem *log;
{
	old_legacy_logsys_unlock(log);
	old_legacy_logsys_voidclose(log->mapfd);
	log->mapfd = -1;

	old_legacy_logsys_restoresignals();
}

/*
 * Just refresh our copy of the map.
 */
old_legacy_logsys_refreshmap(log)
	LogSystem *log;
{
	if (log->mapfd >= 0) {
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
	}
	if ((log->mapfd = old_legacy_logsys_openfile(log, LSFILE_MAP,
			O_RDONLY | O_BINARY, 0)) < 0) {
		int e = errno;
		old_legacy_logsys_warning(log, "Cannot open map: %s", old_legacy_syserr_string(e));
        /* try to see if LSFILE_SQLITE do exist in case we have a sqlite base */
        {
            int testfd = open(sqlite_logsys_filepath(log, LSFILE_SQLITE), O_RDONLY);
            if ((testfd >= 0) || (errno != ENOENT))
            {
                if (testfd >= 0) close(testfd);
                sqlite_logsys_warning(log, "You have a %s file present : maybe you should try and use sqlite mode", LSFILE_SQLITE);
            }    
            return (-1);
        }
		errno = e;
		return (-2);
	}
	if (old_legacy_logsys_readlock(log)) {
		int e = errno;
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_warning(log, "Cannot lock map: %s", old_legacy_syserr_string(e));
		errno = e;
		return (-3);
	}
	if (old_legacy_log_readbuf(log->mapfd, log->map, sizeof(*log->map), 0)) {
		int e = errno;
		old_legacy_logsys_unlock(log);
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_warning(log, "Cannot read map: %s", old_legacy_syserr_string(e));
		errno = e;
		return (-4);
	}
	if (log->map->magic != LSMAGIC_MAP) {
		old_legacy_logsys_unlock(log);
		old_legacy_logsys_voidclose(log->mapfd);
		log->mapfd = -1;
		old_legacy_logsys_warning(log, "Map is not a valid mapfile");
		errno = EINVAL;
		return (-5);
	}
	old_legacy_logsys_unlock(log);
	old_legacy_logsys_voidclose(log->mapfd);
	log->mapfd = -1;
	return (0);
}

/*
 * Return path into named file in logsystem.
 *
 * ALERT
 *
 * Returned buffer is static and is overwritten on each call.
 */

char *
old_legacy_logsys_filepath(log, basename)
	LogSystem *log;
	char *basename;
{
	static char filename[1025];

	strcpy(filename, log->sysname);
	strcat(filename, basename);

	return (filename);
}

/*
 * Open specified file and make sure
 * it does not clash with stdio.
 */
old_legacy_logsys_openfile(log, basename, flags, mode)
	LogSystem *log;
	char *basename;
	int flags, mode;
{
	int fd;

	for (;;) {
		fd = open(old_legacy_logsys_filepath(log, basename), flags, mode);
		if (fd < 0)
			return (fd);
		if (fd > 2) {
#ifdef F_SETFD
			fcntl(fd, F_SETFD, 1);
#endif
			return (fd);
		}
		close(fd);

		if (open("/dev/null", 0) < 0) {
			perror("/dev/null");
			return (-1);
		}
	}
}

void
old_legacy_logsys_voidclose(fd)
	int fd;
{
	int err = errno;

	(void) close(fd);
	errno = err;
}

LogEntry *
old_legacy_logentry_get(log)
	LogSystem *log;
{
	LogEntry *entry;

	writeLog(LOGLEVEL_BIGDEVEL, "begin old_legacy_logentry_get");
	if (old_legacy_logsys_opendata(log))
		return (NULL);
	writeLog(LOGLEVEL_BIGDEVEL, "begin old_legacy_logsys_opendata OK");

	entry = old_legacy_logentry_alloc_skipclear(log, log->walkidx);
	writeLog(LOGLEVEL_BIGDEVEL, "old_legacy_logentry_alloc_skipclear(log, log->walkidx) OK");

	for (;;) {
		if (log->mmapping) {
			if (entry->idx > log->map->lastindex) {
				old_legacy_logentry_free(entry);
				return (NULL);
			}
			entry->record = MMAPPED_RECORD(log, entry->idx);
		} else {
			if (old_legacy_log_readbuf(log->datafd,
					entry->record, log->label->recordsize,
					entry->idx  *  log->label->recordsize)) {
				old_legacy_logentry_free(entry);
				return (NULL);
			}
		}
		++log->walkidx;

		if (ACTIVE_ENTRY(entry->record))
			break;

		++entry->idx;
	}
	return (entry);
}

old_legacy_logentry_put(entry)
	LogEntry *entry;
{
	LogSystem *log = entry->logsys;

	old_legacy_logentry_write(entry);
	old_legacy_logentry_free(entry);

	return (0);
}

old_legacy_logsys_needsreread(log)
	LogSystem *log;
{
	struct stat st;
	char *filename = old_legacy_logsys_filepath(log, LSFILE_DATA);

	if (stat(filename, &st) == 0
	 && st.st_mtime < log->last_reread)
		return (0);

	log->last_reread = time(NULL);

	return (1);
}

old_legacy_logsys_rereaddone(log)
	LogSystem *log;
{
	log->last_reread = time(NULL);
}

time_t
old_legacy_logsys_lastchange(log)
	LogSystem *log;
{
	struct stat st;
	char *filename = old_legacy_logsys_filepath(log, LSFILE_DATA);

	if (stat(filename, &st) != 0)
		return (0);

	return (st.st_mtime);
}

