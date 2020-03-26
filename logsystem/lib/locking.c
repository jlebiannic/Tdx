/*========================================================================
        E D I S E R V E R

        File:		logsystem/locking.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Logsystem locking routines.
========================================================================*/
#include "conf/config.h"
MODULE("@(#)TradeXpress $Id: locking.c 47371 2013-10-21 13:58:37Z cdemory $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 04.12.97/JN	block/nonblock constants were mixed on NT.
  3.02 29.07.98/JN	_locking() does not understand "rest of file".
========================================================================*/

#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>

#include "logsystem.sqlite.h"

#ifdef MACHINE_WNT

/*
 *  Make up posix constants and emulate them with locking()
 *
 *  locking() returns -1 and sets errno to EDEADLOCK
 *  if the file cannot be locked after 10 tries,
 *  with an interval of 1 second between tries.
 *  The error is immediate EACCES if nonblocking lock is wanted.
 */

#include <sys/locking.h>
#include <io.h>

#define F_SETLKW 1
#define F_SETLK  2

#define F_UNLCK  3
#define F_RDLCK  4
#define F_WRLCK  5

static sqlite_lock(LogSystem *log, int fd, int op, int type, long start, long length)
{
	long savpos;
	int rv, how, saverr;

	if (type == F_UNLCK)
    {
		how = _LK_UNLCK; /* UNLCK  unlock */
    }
	else
    {
        if (op == F_SETLKW)
            how = _LK_LOCK;  /* SETLKW blocking lock */
        else
            how = _LK_NBLCK; /* SETLK  nonblocking lock */
    }

	if (length == 0)
		length = -1L;

	if ((savpos = _lseek(fd, start, SEEK_SET)) == -1L)
		return (-1);

	/* Loop until get the lock. */
	while ((rv = locking(fd, how, length)) == -1)
    {
		/* No blocking allowed ? */
		if (how != _LK_LOCK)
			break;
		/* Neither of the expected errors ? */
		if (errno != EACCES && errno != EDEADLOCK)
			break;
	}
	/* Restore position in file, for completeness. */
	saverr = errno;

	if (_lseek(fd, savpos, SEEK_SET) == -1L)
    {
		if (rv == 0)
			rv = -1;
	}
    else
		errno = saverr;

	return (rv);
}

#else /* Then use POSIX locks */

static int sqlite_lock(LogSystem *log, int fd, int op, int type, int start, int length)
{
	int rv;
	struct flock flock;

	flock.l_type = type;
	flock.l_start = start;
	flock.l_whence = SEEK_SET;
	flock.l_len = length;
	flock.l_pid = 0;

	rv = fcntl(fd, op, &flock);

	return (rv);
}

/* See who is locking the logsystem.
 * 0 nobody, > 0 pid, -1 syserr. */
int sqlite_log_getlocker(LogSystem *log)
{
	struct flock flock;

	flock.l_type = F_RDLCK;
	flock.l_start = 0;
	flock.l_whence = SEEK_SET;
	flock.l_len = 0;
	flock.l_pid = 0;

	if (fcntl(log->datafd, F_GETLK, &flock))
		return (-1);

	if (flock.l_type == F_UNLCK)
		return (0);

	return (flock.l_pid);
}

#endif /* POSIX locks */

/* Single-record locking. */
int sqlite_logentry_readlock(LogEntry *entry)
{
	return (sqlite_logentry_lockrecord(entry, F_RDLCK));
}

int sqlite_logentry_writelock(LogEntry *entry)
{
	return (sqlite_logentry_lockrecord(entry, F_WRLCK));
}

int sqlite_logentry_unlock(LogEntry *entry)
{
	return (sqlite_logentry_lockrecord(entry, F_UNLCK));
}

int sqlite_logentry_lockrecord(LogEntry *entry, int locktype)
{
	LogSystem *log = entry->logsys;

	return (lock(log, log->datafd, F_SETLKW, locktype, entry->idx * log->label->recordsize, log->label->recordsize));
}

/* System level locking (whole logsystem). */

int sqlite_logsys_readlock(LogSystem *log)
{
	return (sqlite_logsys_locksystem(log, F_RDLCK));
}

int sqlite_logsys_writelock(LogSystem *log)
{
	return (sqlite_logsys_locksystem(log, F_WRLCK));
}

int sqlite_logsys_unlock(LogSystem *log)
{
	return (sqlite_logsys_locksystem(log, F_UNLCK));
}

int sqlite_logsys_locksystem(LogSystem *log, int locktype)
{
	return (sqlite_lock(log, log->datafd, F_SETLKW, locktype, 0, 0));
}

/*
 * Simple general locking functions.
 *
 * First byte is (un)locked, blocking.
 *
 * Locker can either call logsys_unlock()
 * or simply close the locked fd.
 *
 * Locking can fail if
 *  fd is bad (should not happen)
 *  kernel runs out of locking structures (not very likely)
 *  deadlock is detected (should not happen, otherwise logsys logic is wrong)
 *
 * In any case, proceeding after failure is questionable at best.
 */

int sqlite_logfd_readlock(int fd)
{
	if (sqlite_lock(NULL, fd, F_SETLKW, F_RDLCK, 0, 1))
    {
		perror("readlock");
		exit(1);
	}
    return 0;
}

int sqlite_logfd_writelock(int fd)
{
	if (sqlite_lock(NULL, fd, F_SETLKW, F_WRLCK, 0, 1))
    {
		perror("writelock");
		exit(1);
	}
    return 0;
}

int sqlite_logfd_unlock(int fd)
{
	if (sqlite_lock(NULL, fd, F_SETLKW, F_UNLCK, 0, 1))
    {
		perror("unlock");
		exit(1);
	}
    return 0;
}

int sqlite_logsys_cleanup_lock(LogSystem *log)
{
	int fd;

	fd = sqlite_logsys_openfile(log, LSFILE_CLEANLOCK, O_RDWR | O_CREAT, 0644);
	if (fd < 0)
    {
		perror("cleanup lock, open");
		exit(1);
	}
	log->cleanupfd = fd;

	if (sqlite_lock(log, fd, F_SETLKW, F_WRLCK, 0, 1))
    {
		perror("cleanup lock, lock");
		exit(1);
	}
	return (0);
}

int sqlite_logsys_cleanup_lock_nonblock(LogSystem *log)
{
	int fd;

	fd = sqlite_logsys_openfile(log, LSFILE_CLEANLOCK, O_RDWR | O_CREAT, 0644);
	if (fd < 0)
    {
		perror("cleanup lock nonblock, open");
		exit(1);
	}
	log->cleanupfd = fd;

	if (sqlite_lock(log, fd, F_SETLK, F_WRLCK, 0, 1) == 0)
    {
		/* Got the lock. */
		return (0);
	}
	if (errno == EACCES || errno == EAGAIN)
    {
		/* Did not get it. */
		return (1);
	}
	perror("cleanup lock nonblock, lock");
	exit(1);
}

int sqlite_logsys_cleanup_unlock(LogSystem *log)
{
	if (close(log->cleanupfd) == 0)
		return (0);

	sqlite_logsys_warning(log, "cleanup unlock: %s", syserr_string(errno));
	return (-1);
}

