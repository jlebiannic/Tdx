/*========================================================================
        E D I S E R V E R

        File:		logsystem/build.c
        Author:		Juha Nurmela (JN)
        Date:		Mon Oct  3 02:18:43 EET 1994

        Copyright (c) 1994 Telecom Finland/EDI Operations

	Building a new logsystem.
========================================================================*/
#include "conf/config.h"
LIBRARY(liblogsystem_version)
MODULE("@(#)TradeXpress $Id: build.c 55239 2019-11-19 13:50:31Z sodifrance $")
/*========================================================================
  Record all changes here and update the above string accordingly.
  3.00 03.10.94/JN	Created.
  3.01 19.09.97/JN	Fix for both \ and / pathseps on NT.
  3.02 29.06.05/CD  Remove all the .files if they already exist at build time 
  Jira TX-3143 19.11.2019 - Olivier REMAUD - Passage au 64 bits
========================================================================*/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#ifndef MACHINE_WNT
#include <unistd.h>
#endif

#include "logsystem/lib/logsystem.h"
#include "logsystem/lib/logsystem.sqlite.h"
#include "logsystem/lib/old_legacy/logsystem.old_legacy.h"
#include "logsystem/lib/old_legacy/private.old_legacy.h"
#include "logsystem/lib/lstool.h"

extern int Pre_allocate;

static char *Sysdirs[] = {
	LSFILE_FILES,
	LSFILE_TRACE,
	LSFILE_TMP,
	NULL
};

extern char *logsys_Filesdirs[];

void logsys_createlabel(LogSystem *log);
void logsys_createmap(LogSystem *log);
void logsys_createdirs(char *sysname);
void logsys_createdata(LogSystem *log);
void logsys_createhints(LogSystem *log, int count, char *name);
int logsys_copyconfig(char *cfgfile, char *sysname);

void
logsys_buildsystem(sysname, cfgfile)
	char *sysname;
	char *cfgfile;
{
	LogSystem *log;

	log = logsys_alloc(sysname);

	/*
	 * label is expanded as needed by readconfig.
	 */
	log->label = logsys_readconfig(cfgfile,0);
	if (log->label == NULL)
		bail_out("Cannot open %s", cfgfile);

	if (!Quiet) {
		logsys_dumplabelfields(log->label);
	}
	if (Really) {
		int omask = umask(0);

		logsys_createdirs(log->sysname);
		logsys_copyconfig(cfgfile, log->sysname);
		logsys_createlabel(log);
		logsys_createdata(log);
		logsys_createmap(log);
		logsys_createhints(log,
			log->label->creation_hints, LSFILE_CREATED);
		logsys_createhints(log,
			log->label->modification_hints, LSFILE_MODIFIED);

		umask(omask);
	}
}

void
logsys_createlabel(log)
	LogSystem *log;
{
	int labelsize, fd;
	LogField *field;
	char *labelbase;

	log->label->magic = LSMAGIC_LABEL;
	log->label->version = LSVERSION;

	labelsize = LABELSIZE(log->label);
	labelbase = (char *) log->label;

	fd = logsys_openfile(log, LSFILE_LABEL,
		O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
	if (fd < 0)
		bail_out("Cannot create label");
	
	/*
	 * Adjust pointers.
	 */
	for (field = log->label->fields; field->type; ++field) {
		field->name.offset   = field->name.string   - labelbase;
		field->format.offset = field->format.string - labelbase;
		field->header.offset = field->header.string - labelbase;
	}

	if (write(fd, log->label, labelsize) != labelsize)
		bail_out("Label write error");

	close(fd);

	/*
	 * And back to normal.
	 */
	for (field = log->label->fields; field->type; ++field) {
		field->name.string   = labelbase + field->name.offset;
		field->format.string = labelbase + field->format.offset;
		field->header.string = labelbase + field->header.offset;
	}
}

void
logsys_createmap(log)
	LogSystem *log;
{
	int fd;

	log->map->magic = LSMAGIC_MAP;
	log->map->version = LSVERSION;

	fd = logsys_openfile(log, LSFILE_MAP,
			O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
	if (fd < 0)
		bail_out("Cannot create map");

	if (write(fd, log->map, sizeof(*log->map)) != sizeof(*log->map))
		bail_out("Map write error");

	log->mapfd = fd;

	fd = logsys_openfile(log, LSFILE_CLEANLOCK,
			O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
	if (fd < 0)
		bail_out("Cannot create cleanlock");
	close(fd);
}

void
logsys_createdirs(sysname)
	char *sysname;
{
	char path[1024];
	int mode = Access_mode;
	int i;

	/*
	 * Add search permission,
	 * if either corresponding rw bit is on.
	 */
	if (mode & 0600)
		mode |= 0100;
	if (mode & 0060)
		mode |= 0010;
	if (mode & 0006)
		mode |= 0001;

	if (mkdir(sysname, mode) && errno != EEXIST)
		bail_out("Cannot make directory");

	for (i = 0; Sysdirs[i]; ++i) {

		strcpy(path, sysname);
		strcat(path, Sysdirs[i]);

		if (mkdir(path, mode) && errno != EEXIST)
			bail_out("Cannot mkdir %s", path);
	}
	for (i = 0; logsys_Filesdirs[i]; ++i) {

		strcpy(path, sysname);
		strcat(path, LSFILE_FILES);
		strcat(path, "/");
		strcat(path, logsys_Filesdirs[i]);

		if (mkdir(path, mode) && errno != EEXIST)
			bail_out("Cannot mkdir %s", path);
	}
}

/*
 * Copy the given configuration file to system directory,
 * if it does not already exist.
 */
int
logsys_copyconfig(cfgfile, sysname)
	char *cfgfile;
	char *sysname;
{
	char filename[1024];
	char *basename, *cp;
	int c;
	FILE *from, *to;

	basename = cfgfile;

	for (cp = basename; *cp; ++cp)
		switch (*cp) {
#ifdef MACHINE_WNT
		case '\\':
#endif
		case '/':
			basename = cp + 1;
		}

	sprintf(filename, "%s/%s", sysname, basename);

	if ((to = fopen(filename, "r")) != NULL) {
		/*
		 * It's already there.
		 */
		fclose(to);
		return (0);
	}
	if ((to = fopen(filename, "w")) == NULL) {
		/*
		 * Cannot copy the configuration, not a fatal error.
		 */
		if (!Quiet)
			fprintf(stderr, "\
Warning: copy configuration file into system directory: %s\n",
				syserr_string(errno));
		return (1);
	}
	if ((from = fopen(cfgfile, "r")) == NULL)
		bail_out("%s", cfgfile);

	while ((c = getc(from)) != EOF)
		if (putc(c, to) == EOF)
			bail_out("%s", filename);
	if (ferror(from))
		bail_out("%s", cfgfile);
	
	return (0);
}

void
logsys_createdata(log)
	LogSystem *log;
{
	int n, fd;
	int recsize;
	LogLabel *label = log->label;
	DiskLogRecord *record;

	recsize = label->recordsize;
	record = (void *) malloc(recsize);
	if (record == NULL)
		bail_out("Out of memory");

	log->map->numused = 0;
	log->map->firstfree = 0;
	log->map->lastfree = 0;
	log->map->lastindex = 0;

	fd = logsys_openfile(log, LSFILE_DATA,
			O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
	if (fd < 0)
		bail_out("Cannot create data");

	/*
	 * First record is a dummy
	 */
	memset(record, 0, recsize);

	if (write(fd, record, recsize) != recsize)
		bail_out("Cannot write first record");

	for (n = 1; n <= Pre_allocate; ++n) {

		record->header.idx = n;
		record->header.ctime = 0;
		if (n == Pre_allocate)
			record->header.nextfree = 0;
		else
			record->header.nextfree = n + 1;

		if (!log->map->firstfree)
			log->map->firstfree = n;
		log->map->lastfree = n;
		log->map->lastindex = n;

		if (write(fd, record, recsize) != recsize)
			bail_out("Cannot write record %d", n);
	}
	free(record);

	log->datafd = fd;
}

/*
 * Hints-files are created/removed/truncated.
 *
 * Old contents are preserved if appropriate.
 */
void
logsys_createhints(log, count, name)
	LogSystem *log;
	int count;
	char *name;
{
	int fd;
	struct stat st;

	if (count > 0) {
		/*
		 * Create file and truncate if greater than allowed.
		 */
		fd = logsys_openfile(log, name,
				O_TRUNC | O_RDWR | O_CREAT | O_BINARY, Access_mode);
		if (fd >= 0) {
			if (fstat(fd, &st))
				st.st_size = 0;
			if (st.st_size > count * sizeof(LogHints))
				ftruncate(fd, count * sizeof(LogHints));
			close(fd);
		}
	} else {
		unlink(logsys_filepath(log, name));
	}
}

