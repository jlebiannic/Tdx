/*========================================================================
	E D I S E R V E R   T R A N S L A T O R

	File:		tr_getfp.c
	Programmer:	Mikko Valimaki (MV)
	Date:		Sat Oct 10 08:25:51 EET 1992

	Copyright (c) 1992-1994 Telecom Finland/EDI Operations
========================================================================*/
#include "conf/local_config.h"
MODULE("@(#)TradeXpress $Id: tr_getfp.c 55330 2020-02-10 12:42:58Z sodifrance $")
/*LIBRARY(libruntime_version)
*/
/*========================================================================
  Record all changes here and update the above string accordingly.

  3.00 12.01.93/MV	Created.
  3.01 02.12.94/JN	Removed the limit from open files.
  3.02 07.12.94/JN	Limit and active-count for fileshuffling.
  3.03 06.02.95/JN	Removed excess #includes.
  3.04 08.05.95/JN	{f,p}{open,close} changed to tr_ counterparts.
  3.05 30.04.96/JN	tr_LookupFpFromName()
  4.00 09.06.99/HT	trFileCloseAll for client-remote closing
  TX-3123 - 03.06.2019 - Olivier REMAUD - UNICODE adaptation
  Jira TX-3123 10.02.2020 - Olivier REMAUD - UNICODE adaptation
==========================================================================

	Filenames are strdupped, and the copies are free'd in here.

	Files can be opened with following modes:
	  r	Open for reading.
	  w	Create a new file.
	  a	Append to an existing file
		(or create a new if it does not exist)
	If the file is really a process (p-opened)
	write and append behave identically (write).

	File can be written with either w or a.
	After initial open, w or a can be mixed freely.
	This indicates the user does not know what he wants,
	but we update our marks with the newest seen mode.

	Records for open files are kept in linked list,
	and every lookup for file lifts the record to head of list.

	Following functionality (reusal of files) is not active by default,
	integers tr_config_OFILE_ACTIVE and tr_config_OFILE_MAX
	have to be set by user, by calling bfSetFileLimits(active, max).

	The limits defaults to 40, which is what ES 3.1 had.

		If we hit the system imposed limit for open files,
		or userspecified active count,
		we try to find the least recently used open file and
		close that. The position is remembered so that when
		an access happens to that file, it is reopened and
		seeked into position. Obviously this does not work
		with pipes or special files and is not even tried
		with them.

		Actually the limit might not be about fildescs,
		but on silly stdio max, _NFILE or some such.
		stdio should be dumped in this...

	If limit is set to zero or less, it means there is no limit.
	Otherwise the userspecified limit and the system imposed limit
	cause either immediate failures or reusal of older files.
	If limit from user is below limit from system, we never
	try to open, nor reuse files.
	If systemlimit is smaller, then reusal happens when needed.

	Clarification:

	ACTIVE number of files can be opened by the user, and
	at most MAX number of files are really open.
	So, the ACTIVE could be a safeguard to catch programming errors
	and MAX would prevent this stuff from robbing all available
	FILE-pointers.

	Example:
	Calling bfSetFileLimits(1000, 40)
	would cause us to
	use at most 40 FILE-pointers to give user the
	illusion of having 1000 concurrently open files.

=========================================================================*/

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#ifndef S_ISREG
#define S_ISREG(mode) ((mode & S_IFMT) == S_IFREG)
#endif

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#include "tr_strings.h"
#include "tr_externals.h"
#include "tr_privates.h"
#include "tr_prototypes.h"
#include "../bottom/tr_edifact.h"

#ifndef MACHINE_LINUX
extern int errno;
#endif

int tr_config_OFILE_ACTIVE = 40;	/* runtime ... */
int tr_config_OFILE_MAX = 40;		/* ... modifiable */

#define TR_NULLFILE		((FILE*) 0)

static struct record {
	struct record *next;
	char *name;
	FILE *fp;
	long offset;		/* offset if currently closed */
	int status;		/* saved status from close */
	char mode;		/* r, w or a */
	char isregular;		/* just a regular plain file */
	int  iconv_initd;	/* are iconv informations initialized ? */
	void *converter; 	/* iconv converter */
	char *encoding;		/* name of encoding for this file */
	int  read_pad;		/* read padding, for LE encodings */
	char pad2;
	char pad3;
} *tr_files = NULL;

static int tr_nfiles_active = 0;/* count of entries in above list */
static int tr_nfiles_open = 0;	/* count of entries really open */

#define isprocess(entry)	((entry)->name[0] == '|')

static int do_open(struct record *);

int bfSetFileLimit(double activefiles, double maxopen)
{
	tr_config_OFILE_ACTIVE = (int) activefiles;
	tr_config_OFILE_MAX    = (int) maxopen;

	return (1);
}

void *tr_GetIconvInfo( FILE* fh, int *initd, int *padding, char **encoding )
{
	struct record *entry;
	*initd = 0;
	for (entry = tr_files; entry; entry = entry->next) {
		if ( entry->fp == fh ) {
			*initd = entry->iconv_initd;
			*padding = entry->read_pad;
			*encoding = entry->encoding;
			return entry->converter;
		}
	}
	*initd = 0;
	*padding = 0;
	*encoding = NULL;
	return NULL;
}

void tr_SetIconvInfo( FILE* fh, int padding, char *encoding, void* converter )
{
	struct record *entry;
	for (entry = tr_files; entry; entry = entry->next) {
		if ( entry->fp == fh ) {
			entry->iconv_initd = 1;
			entry->read_pad = padding;
			entry->encoding = (encoding == NULL) ? NULL : tr_strdup( encoding );
			entry->converter = converter;
			return;
		}
	}
	/* file not opened. Nothing to do. Should we log ? */
}



FILE *tr_LookupFpFromName(char *filename, char *accmode)
{
	struct record *entry;

	for (entry = tr_files; entry; entry = entry->next) {
		if (!strcmp(entry->name, filename)) {
			if (accmode == NULL || *accmode == 0)
				break;
			if (*accmode == 'r') {
				if (entry->mode == 'r')
					break;
			} else {
				if (entry->mode != 'r')
					break;
			}
		}
	}
	return (entry ? entry->fp : NULL);
}

FILE *tr_GetFp(char *filename, char *accmode)
{
	struct record **fpp, *entry;
	int err;
	char mode;

	if (!filename || !accmode || strlen(accmode) != 1)
		tr_Fatal(TR_FATAL_INVALID_GETFP_ARGS);

	/*
	 * We are forgiving,
	 * uppercase accessmode is lowered.
	 */
	mode = *accmode;
	if (isupper(mode))
		mode = tr_tolower(mode);

	if (mode != 'r' && mode != 'w' && mode != 'a')
		tr_Fatal (TR_FATAL_INVALID_GETFP_MODE);

	/*
	 * Try to find the file from previous entries.
	 */
	entry = NULL;

	for (fpp = &tr_files; *fpp; fpp = &(*fpp)->next) {
		entry = *fpp;
		if (strcmp(entry->name, filename) != 0)
			continue;
		/*
		** The names are an exact match:
		** now check that the mode is also equal.
		** If this and previous mode are not in synch,
		** keep looking for the right one
		** This means that we shall eventually have
		** the file open both for write and read.
		**
		** The switch either continue-s above loop,
		** or break-s out of it.
		*/
		switch (entry->mode) {
		default:
			/*
			 * Should not happen ever.
			 */
			tr_Log(TR_WARN_INVALID_MODE_IN_FILETAB, entry->mode);
			continue;

		case 'w':
		case 'a':
			if (mode == 'r') {
				/*
				**  Opening this file for reading has to be a
				**  new entry, so we just continue looking.
				*/
				continue;
			}
			/*
			** Whether the file has been opened
			** for writing or appending
			** is all the same after the opening.
			**
			** We update the mode in table, however.
			*/
			entry->mode = mode;
			break;

		case 'r':
			if (mode != 'r') {
				/*
				**  Opening this file for writing has to be a
				**  new entry, so we just continue looking.
				*/
				continue;
			}
			/*
			**  This is what we have been looking for.
			*/
			break;
		}
		/*
		 * this is a hit.
		 * Remove entry from list.
		 */
		*fpp = entry->next;
		goto found;
	}
	/*
	 * This is a new file to be opened.
	 */
	if (tr_nfiles_active >= tr_config_OFILE_ACTIVE &&
				tr_config_OFILE_ACTIVE > 0) {
		/*
		 * Userspecified limit reached.
		 * This does not mean we have run out of
		 * descriptors, but that the user has
		 * set a limit.
		 */
		err = EMFILE;
		goto failed;
	}
	entry = tr_zalloc(sizeof(*entry));
	entry->fp     = NULL;
	entry->offset = 0;
	entry->name   = tr_strdup(filename);
	entry->mode   = mode;
	++tr_nfiles_active;
found:
	/*
	 * This goes to top.
	 */
	entry->next = tr_files;
	tr_files = entry;

	if (entry->fp)
		return (entry->fp);

	/*
	 * While open fails because of exhausted FILEs,
	 * we loop back to here.
	 * Dont even try to open the file
	 * if we have hit the open limit.
	 */
retry:
	if (tr_nfiles_open >= tr_config_OFILE_MAX &&
				tr_config_OFILE_MAX > 0) {
		errno = EMFILE;
	} else {
		if (do_open(entry) == 0)
			return (entry->fp);
	}
	err = errno;

	if (err == EMFILE || err == ENFILE) {
		/*
		 * Too many open files,
		 * either for us or in the whole system.
		 * Find out LRU record and silently close it
		 * remembering current position.
		 */
		struct record *old = NULL;
		struct record *lru = NULL;
		long offset;

		for (old = entry->next; old; old = old->next) {
			if (old->isregular && old->fp)
				lru = old;
		}
		/*
		 * If there was older regular file,
		 * and it was open for reading or we can fflush it,
		 * and it's position is available
		 *	then close it and retry.
		 */
		if (lru != NULL
		 && (lru->mode == 'r' || fflush(lru->fp) == 0)
		 && (offset = ftell(lru->fp)) != -1) {
			lru->offset = offset;
			lru->status = tr_fclose(lru->fp, 0);
			lru->fp = NULL;
			--tr_nfiles_open;
			goto retry;
		}
	}
	/*
	**  Failed.
	**
	**  Print an error message and return a
	**  "spare tire".  In output we return the
	**  stderr and in input TR_NULLFILE, for
	**  which the tr_FileRead() routine returns
	**  an EOF.
	*/
	entry = tr_files;
	tr_files = entry->next;
	--tr_nfiles_active;
	tr_free(entry->name);
	tr_free(entry);
failed:
	errno = err;

	if (errno == ENFILE || errno == EMFILE) {
		if (mode == 'r') {
			tr_Log(TR_WARN_TOO_MANY_FILES_FOR_READ, filename);
		} else {
			tr_Log(TR_WARN_TOO_MANY_FILES_FOR_WRITE, filename);
		}
	} else {
		if (mode == 'r') {
			tr_Log(TR_WARN_GETFP_FOR_READ_FAILED,
				tr_PrettyPrintFileName(filename), errno);
		} else {
			tr_Log(TR_WARN_GETFP_FOR_WRITE_FAILED,
				tr_PrettyPrintFileName(filename), errno);
		}
	}
	return((mode == 'r') ? TR_NULLFILE : stderr);
}

/*========================================================================
  Open or reopen a file.
========================================================================*/

static int do_open(struct record *entry)
{
	struct stat st;
	char mode[4];
	FILE *fp;

	mode[0] = entry->mode;
	mode[1] = 0;
	mode[2] = 0;
	mode[3] = 0;

	if (isprocess(entry)) {
		/*
		 * Use "w" instead of "a"
		 * when "appending" to pipe.
		 */
		if (mode[0] == 'a')
			mode[0] = 'w';

		fp = tr_popen(entry->name + 1, mode);
	} else {
		/*
		 * Reopening an old file for write,
		 * old contents have to be preserved, hence "r+".
		 */
		if (mode[0] == 'w' && entry->offset) {
			mode[0] = 'r';
			mode[1] = '+';
			mode[2] = 0;
		}
		fp = tr_fopen(entry->name, mode);
	}
	if (fp != NULL) {
		/*
		 * Immediate success.
		 * Is output unbuffered ?
		 * When starting a file, check if it is regular file.
		 * When continuing a regular file, seek to position.
		 */
		
		if (tr_unBufferedOutput)
			setbuf(fp, NULL);

		if (entry->offset == 0 && !isprocess(entry)) {
			if (fstat(fileno(fp), &st) == 0 && S_ISREG(st.st_mode))
				entry->isregular = 1;
		}
		if (entry->offset != 0 && entry->isregular)
			fseek(fp, entry->offset, SEEK_SET);

		entry->fp = fp;
		++tr_nfiles_open;
		return (0);
	}
	return (1);
}

/*============================================================================
  Close an opened file descriptor.
  Currently this closes the first matching filename only !
============================================================================*/

double tr_FileClose(char *filename)
{
	struct record **fpp, *entry;
	int status;

	if (!filename || !*filename)
		return (-1.0);

	for (fpp = &tr_files; *fpp; fpp = &(*fpp)->next) {
		entry = *fpp;

		if (strcmp(entry->name, filename) != 0)
			continue;

		*fpp = entry->next;

		/*
		 * The file can be already closed,
		 * if we were reusing FILEs and this
		 * was not reopened after lru-close.
		 */
		if (entry->fp == NULL) {
			status = entry->status;
		} else {
			if (isprocess(entry))
				status = tr_pclose(entry->fp);
			else
				status = tr_fclose(entry->fp, 0);
			--tr_nfiles_open;
		}
		--tr_nfiles_active;
		tr_free(entry->name);
		tr_free(entry);

		
		/* free encoding information */
		if ( entry->converter != NULL ) {
			tr_FreeIconvInfo( entry->converter );
			entry->converter = NULL;
		}
		if ( entry->encoding != NULL ) {
			tr_free( entry->encoding );
		}


		return ((double) status);
	}
	return (-1.0);
}

/*
**  4.00/HT
*/
void tr_FileCloseAll()
{
	struct record **fpp, *entry;
/* 	int status; */

	fpp = &tr_files;
	while (*fpp) {
		entry = *fpp;

		*fpp = entry->next;

		/*
		 * The file can be already closed,
		 * if we were reusing FILEs and this
		 * was not reopened after lru-close.
		 */
		if (entry->fp == NULL) {
/* 			status = entry->status; */
		} else {
			if (isprocess(entry))
			/*	status =*/ tr_pclose(entry->fp);
			else
			/*	status =*/ tr_fclose(entry->fp, 0);
			--tr_nfiles_open;
		}
		--tr_nfiles_active;
		tr_free(entry->name);
		tr_free(entry);

	}
	return;
}
